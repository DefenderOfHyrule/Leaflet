#include "gc_direct_install.hpp"
#include "util/crypto.hpp"
#include "util/error.hpp"
#include "util/avm_maintenance.hpp"
#include "install/nca.hpp"
#include "ui/instPage.hpp"
#include "ui/MainApplication.hpp"
#include "util/lang.hpp"
#include "util/config.hpp"
#include "util/title_util.hpp"
#include "util/file_util.hpp"
#include "nx/ipc/ns_ext.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <switch.h>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <cmath>

namespace inst::ui {
    extern MainApplication *mainApp;
}

#define NCA_SECTOR_SIZE     0x200
#define GC_NCA_HEADER_SIZE  0xC00
#define NCA_MAGIC           0x3341434E  // "NCA3"
#define PFS0_MAGIC          0x30534650  // "PFS0"
#define NCA_SECTION_TOTAL   4
#define INSTALL_BUFFER_SIZE 0x800000

#pragma pack(push, 1)
struct NcaSectionHeader {
    uint32_t media_start;
    uint32_t media_end;
    uint64_t _pad;
};

struct NcaPfs0SuperBlock {
    uint8_t  master_hash[0x20];
    uint32_t hash_block_size;
    uint32_t _pad;
    uint64_t hash_table_offset;
    uint64_t hash_table_size;
    uint64_t pfs0_offset;
    uint64_t pfs0_size;
};

struct NcaFsHeader {
    uint16_t version;
    uint8_t  fs_type;
    uint8_t  hash_type;
    uint8_t  encryption_type;
    uint8_t  _pad0[3];
    union {
        NcaPfs0SuperBlock pfs0_sb;
        uint8_t  raw[0xF8];
    };
    uint8_t  _pad1[0x40];
    uint8_t  section_ctr[8];
    uint8_t  _pad2[0xB8];
};
static_assert(sizeof(NcaFsHeader) == 0x200, "NcaFsHeader size");

struct NcaKeyArea {
    uint8_t area[0x10];
};

struct NcaFullHeader {
    uint8_t  rsa_fixed[0x100];
    uint8_t  rsa_npdm[0x100];
    uint32_t magic;
    uint8_t  distribution_type;
    uint8_t  content_type;
    uint8_t  old_key_gen;
    uint8_t  kaek_index;
    uint64_t size;
    uint64_t title_id;
    uint32_t context_id;
    uint32_t sdk_version;
    uint8_t  key_gen;
    uint8_t  header_1_sig_key_gen;
    uint8_t  _reserved[0xE];
    FsRightsId rights_id;
    NcaSectionHeader sections[4];
    uint8_t  section_hashes[4][0x20];
    NcaKeyArea key_area[4];
    uint8_t  _reserved2[0xC0];
    NcaFsHeader section_header[4];
};
static_assert(sizeof(NcaFullHeader) == 0xC00, "NcaFullHeader size");

struct Pfs0Header {
    uint32_t magic;
    uint32_t total_files;
    uint32_t string_table_size;
    uint32_t _pad;
};

struct Pfs0FileEntry {
    uint64_t data_offset;
    uint64_t data_size;
    uint32_t string_table_offset;
    uint32_t _pad;
};

struct CnmtHeader {
    uint64_t title_id;
    uint32_t title_version;
    uint8_t  meta_type;
    uint8_t  _pad0;
    uint16_t extended_header_size;
    uint16_t content_count;
    uint16_t content_meta_count;
    uint8_t  attributes;
    uint8_t  storage_id;
    uint8_t  install_type;
    uint8_t  _pad1;
    uint32_t required_download_sys_version;
    uint8_t  _pad2[4];
};
#pragma pack(pop)

// parsed CNMT data
struct ParsedCnmtData {
    NcmContentMetaKey key;
    NcmContentMetaHeader dbHeader;  // for ncmContentMetaDatabaseSet
    std::vector<uint8_t> extendedHeader;
    std::vector<NcmContentInfo> contentInfos;  // cnmt at [0], then content NCAs
    uint8_t keyGen;
};

// logging
static void gcLog(const char* fmt, ...) {
    char buf[512]; va_list a; va_start(a, fmt);
    vsnprintf(buf, sizeof(buf), fmt, a); va_end(a);
    LOG_DEBUG("gc: %s", buf);
}

static std::string g_lastGcError;

static NcmContentId IdFromString(const char* s) {
    NcmContentId id = {};
    char lo[17] = {}, hi[17] = {};
    memcpy(lo, s, 16); memcpy(hi, s+16, 16);
    *(uint64_t*)id.c = __bswap64(strtoul(lo, nullptr, 16));
    *(uint64_t*)(id.c+8) = __bswap64(strtoul(hi, nullptr, 16));
    return id;
}
static std::string IdToString(const NcmContentId& id) {
    char b[33];
    snprintf(b, 33, "%016lx%016lx", __bswap64(*(uint64_t*)id.c), __bswap64(*(uint64_t*)(id.c+8)));
    return b;
}
static uint64_t AppIdFromTitle(uint64_t tid, uint8_t type) {
    if (type == NcmContentMetaType_Patch) return tid ^ 0x800;
    if (type == NcmContentMetaType_AddOnContent) return (tid ^ 0x1000) & ~0xFFFULL;
    return tid;
}

// NCA header XTS crypto
static void DecryptHeader(const void* in, void* out) {
    Crypto::AesXtr d(Crypto::Keys().headerKey, false);
    d.decrypt(out, in, GC_NCA_HEADER_SIZE, 0, NCA_SECTOR_SIZE);
}
static void EncryptHeader(const void* in, void* out) {
    Crypto::AesXtr e(Crypto::Keys().headerKey, true);
    e.encrypt(out, in, GC_NCA_HEADER_SIZE, 0, NCA_SECTOR_SIZE);
}

// key Area Encryption Key sources
static const uint8_t KEAK_APP_SOURCE[0x10] = { 0x7F, 0x59, 0x97, 0x1E, 0x62, 0x9F, 0x36, 0xA1, 0x30, 0x98, 0x06, 0x6F, 0x21, 0x44, 0xC3, 0x0D };
static const uint8_t KEAK_OCEAN_SOURCE[0x10] = { 0x32, 0x7D, 0x36, 0x08, 0x5A, 0xD1, 0x75, 0x8D, 0xAB, 0x4E, 0x6F, 0xBA, 0xA5, 0x55, 0xD8, 0x82 };
static const uint8_t KEAK_SYSTEM_SOURCE[0x10] = { 0x87, 0x45, 0xF1, 0xBB, 0xA6, 0xBE, 0x79, 0x64, 0x7D, 0x04, 0x8B, 0xA6, 0x7B, 0x5F, 0xDA, 0x4A };

static const uint8_t* GetKeakSource(uint8_t index) {
    switch (index) {
        case 0: return KEAK_APP_SOURCE;
        case 1: return KEAK_OCEAN_SOURCE;
        case 2: return KEAK_SYSTEM_SOURCE;
        default: return nullptr;
    }
}

static bool DecryptKeyArea(const NcaFullHeader* h, NcaKeyArea* outKey) {
    uint8_t kg = h->key_gen ? h->key_gen : h->old_key_gen;
    const uint8_t* keakSrc = GetKeakSource(h->kaek_index);
    if (!keakSrc) { gcLog("unknown kaek index %u\n", h->kaek_index); return false; }

    NcaKeyArea kek = {};
    if (R_FAILED(splCryptoGenerateAesKek(keakSrc, kg, 0, kek.area))) {
        gcLog("splCryptoGenerateAesKek failed\n");
        return false;
    }
    NcaKeyArea dec[NCA_SECTION_TOTAL] = {};
    for (int i = 0; i < NCA_SECTION_TOTAL; i++) {
        if (R_FAILED(splCryptoGenerateAesKey(kek.area, h->key_area[i].area, dec[i].area))) {
            gcLog("splCryptoGenerateAesKey failed slot %d\n", i);
            return false;
        }
    }
    memcpy(outKey, &dec[2], sizeof(NcaKeyArea)); // slot 2 = CTR key
    return true;
}

static void AesCtrDecrypt(void* data, size_t size, const uint8_t* key,
                           uint64_t offset) {
    Crypto::AesCtr iv(0);
    Crypto::Aes128Ctr ctr(key, iv);
    ctr.seek(offset);
    ctr.decrypt(data, data, size);
}

static bool ParseCnmtFromFile(const std::string& path, const NcmContentId& cnmtId,
                               uint64_t fileSize, ParsedCnmtData& out) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;

    // read encrypted NCA header
    NcaFullHeader encH = {}, h = {};
    if (fread(&encH, 1, GC_NCA_HEADER_SIZE, fp) != GC_NCA_HEADER_SIZE) { fclose(fp); return false; }
    DecryptHeader(&encH, &h);
    if (h.magic != NCA_MAGIC) { gcLog("bad magic in cnmt nca\n"); fclose(fp); return false; }

    out.keyGen = h.key_gen ? h.key_gen : h.old_key_gen;

    // zero the dbHeader, storage_id MUST be 0 (not gamecard)
    memset(&out.dbHeader, 0, sizeof(NcmContentMetaHeader));

    NcaKeyArea ctrKey = {};
    if (!DecryptKeyArea(&h, &ctrKey)) { fclose(fp); return false; }

    uint64_t sectionOff = (uint64_t)h.sections[0].media_start * 0x200;
    uint64_t sectionEnd = (uint64_t)h.sections[0].media_end * 0x200;
    if (sectionOff == 0 || sectionEnd <= sectionOff) {
        gcLog("invalid section 0 offsets\n"); fclose(fp); return false;
    }

    uint64_t pfs0Off = h.section_header[0].pfs0_sb.pfs0_offset;
    uint64_t pfs0Size = h.section_header[0].pfs0_sb.pfs0_size;
    if (pfs0Size == 0 || pfs0Size > 0x100000) {
        gcLog("invalid pfs0 size: %lu\n", pfs0Size); fclose(fp); return false;
    }

    auto pfs0Data = std::make_unique<uint8_t[]>(pfs0Size);
    fseeko(fp, sectionOff + pfs0Off, SEEK_SET);
    if (fread(pfs0Data.get(), 1, pfs0Size, fp) != pfs0Size) { fclose(fp); return false; }
    fclose(fp);

    AesCtrDecrypt(pfs0Data.get(), pfs0Size, ctrKey.area,
                   sectionOff + pfs0Off);

    Pfs0Header* pfs0H = (Pfs0Header*)pfs0Data.get();
    if (pfs0H->magic != PFS0_MAGIC) {
        gcLog("bad pfs0 magic: 0x%x\n", pfs0H->magic); return false;
    }

    Pfs0FileEntry* entries = (Pfs0FileEntry*)(pfs0Data.get() + sizeof(Pfs0Header));
    uint64_t dataOff = sizeof(Pfs0Header) + pfs0H->total_files * sizeof(Pfs0FileEntry) + pfs0H->string_table_size;

    if (pfs0H->total_files == 0) { gcLog("pfs0 has no files\n"); return false; }

    uint8_t* cnmtRaw = pfs0Data.get() + dataOff + entries[0].data_offset;
    size_t cnmtSize = entries[0].data_size;

    if (cnmtSize < sizeof(CnmtHeader)) { gcLog("cnmt too small\n"); return false; }
    CnmtHeader* ch = (CnmtHeader*)cnmtRaw;

    memset(&out.key, 0, sizeof(out.key));
    out.key.id = ch->title_id;
    out.key.version = ch->title_version;
    out.key.type = (NcmContentMetaType)ch->meta_type;

    out.dbHeader.extended_header_size = ch->extended_header_size;
    out.dbHeader.content_meta_count = ch->content_meta_count;
    out.dbHeader.attributes = ch->attributes;

    // copy extended header
    uint64_t off = sizeof(CnmtHeader);
    out.extendedHeader.assign(cnmtRaw + off, cnmtRaw + off + ch->extended_header_size);
    off += ch->extended_header_size;

    // build cnmt's own content info (placed at index 0, matching GC-Installer-NX)
    NcmContentInfo cnmtInfo = {};
    cnmtInfo.content_id = cnmtId;
    ncmU64ToContentInfoSize(fileSize & 0xFFFFFFFFFFFF, &cnmtInfo);
    cnmtInfo.content_type = NcmContentType_Meta;
    cnmtInfo.id_offset = 0;
    out.contentInfos.push_back(cnmtInfo);

    for (uint16_t i = 0; i < ch->content_count; i++) {
        if (off + sizeof(NcmPackagedContentInfo) > cnmtSize) break;
        NcmPackagedContentInfo* pci = (NcmPackagedContentInfo*)(cnmtRaw + off);
        off += sizeof(NcmPackagedContentInfo);
        if (pci->info.content_type == NcmContentType_DeltaFragment) continue;
        out.contentInfos.push_back(pci->info);
    }

    // update content count = actual infos (including cnmt)
    out.dbHeader.content_count = out.contentInfos.size();

    gcLog("CNMT parsed: id=%016lX ver=%u type=%u contents=%u\n",
          out.key.id, out.key.version, out.key.type, out.dbHeader.content_count);
    return true;
}

// set content meta database stuff
static bool SetDb(const ParsedCnmtData& c, NcmStorageId sid) {
    size_t sz = sizeof(NcmContentMetaHeader) + c.dbHeader.extended_header_size
              + c.dbHeader.content_count * sizeof(NcmContentInfo);
    auto buf = std::make_unique<uint8_t[]>(sz);
    memset(buf.get(), 0, sz);
    size_t off = 0;
    memcpy(buf.get() + off, &c.dbHeader, sizeof(NcmContentMetaHeader)); off += sizeof(NcmContentMetaHeader);
    memcpy(buf.get() + off, c.extendedHeader.data(), c.dbHeader.extended_header_size); off += c.dbHeader.extended_header_size;

    if (inst::config::ignoreReqVers &&
        (c.key.type == NcmContentMetaType_Application || c.key.type == NcmContentMetaType_Patch)) {
        if (c.dbHeader.extended_header_size > 12) {
            *(uint32_t*)(buf.get() + sizeof(NcmContentMetaHeader) + 8) = 0;
        }
    }

    memcpy(buf.get() + off, c.contentInfos.data(), c.dbHeader.content_count * sizeof(NcmContentInfo));

    NcmContentMetaDatabase db = {};
    Result rc = ncmOpenContentMetaDatabase(&db, sid);
    if (R_FAILED(rc)) { gcLog("open meta db fail: 0x%x\n", rc); return false; }
    rc = ncmContentMetaDatabaseSet(&db, &c.key, (NcmContentMetaHeader*)buf.get(), sz);
    if (R_FAILED(rc)) { gcLog("set meta db fail: 0x%x\n", rc); ncmContentMetaDatabaseClose(&db); return false; }
    rc = ncmContentMetaDatabaseCommit(&db);
    ncmContentMetaDatabaseClose(&db);
    if (R_FAILED(rc)) { gcLog("commit meta db fail: 0x%x\n", rc); return false; }
    gcLog("DB set OK\n");
    return true;
}

static void DeleteExistingGcContent(const NcmContentMetaKey& existingKey, NcmStorageId existingSid) {
    NcmContentMetaDatabase db = {};
    if (R_FAILED(ncmOpenContentMetaDatabase(&db, existingSid))) return;
    NcmContentStorage cs = {};
    bool hasCs = R_SUCCEEDED(ncmOpenContentStorage(&cs, existingSid));
    s32 offset = 0;
    constexpr s32 kBatch = 32;
    NcmContentInfo infos[kBatch];
    while (true) {
        s32 got = 0;
        if (R_FAILED(ncmContentMetaDatabaseListContentInfo(&db, &got, infos, kBatch, &existingKey, offset))) break;
        if (got == 0) break;
        if (hasCs) {
            for (s32 j = 0; j < got; j++) {
                bool has = false;
                if (R_SUCCEEDED(ncmContentStorageHas(&cs, &has, &infos[j].content_id)) && has)
                    ncmContentStorageDelete(&cs, &infos[j].content_id);
            }
        }
        offset += got;
        if (got < kBatch) break;
    }
    NcmContentId cnmtId = {};
    if (R_SUCCEEDED(ncmContentMetaDatabaseGetContentIdByType(&db, &cnmtId, &existingKey, NcmContentType_Meta))) {
        bool has = false;
        if (hasCs && R_SUCCEEDED(ncmContentStorageHas(&cs, &has, &cnmtId)) && has)
            ncmContentStorageDelete(&cs, &cnmtId);
    }
    if (hasCs) ncmContentStorageClose(&cs);
    ncmContentMetaDatabaseRemove(&db, &existingKey);
    ncmContentMetaDatabaseCommit(&db);
    ncmContentMetaDatabaseClose(&db);
}

// import ticket + cert
static void ImportTickets(const std::string& mp, const std::vector<std::string>& files, uint64_t tid) {
    char ts[17]; snprintf(ts, 17, "%016lx", tid);
    std::string tikP, certP;
    for (auto& f : files) {
        if (f.find(".tik") != std::string::npos) tikP = mp + f;
        if (f.find(".cert") != std::string::npos) certP = mp + f;
    }
    if (tikP.empty() || certP.empty()) return;
    gcLog("importing ticket\n");

    auto readAll = [](const std::string& p, std::vector<uint8_t>& out) {
        FILE* f = fopen(p.c_str(), "rb"); if (!f) return false;
        fseek(f, 0, SEEK_END); size_t s = ftell(f); fseek(f, 0, SEEK_SET);
        out.resize(s); fread(out.data(), 1, s, f); fclose(f); return true;
    };
    std::vector<uint8_t> tik, cert;
    if (!readAll(tikP, tik) || !readAll(certP, cert)) return;
    Result rc = esImportTicket(tik.data(), tik.size(), cert.data(), cert.size());
    if (R_FAILED(rc)) gcLog("ticket import fail: 0x%x\n", rc);
    else gcLog("ticket OK\n");
}

struct NcaCtx {
    FILE* fp; uint8_t* buf; uint64_t foff;
    size_t bufSz, written, total;
    NcmContentStorage cs; NcmPlaceHolderId pid; NcmContentId cid;
    std::mutex mtx; std::condition_variable canR, canW;
    bool err;
    bool cancelled;
};
static void ncaReader(NcaCtx* c) {
    auto rb = std::make_unique<uint8_t[]>(INSTALL_BUFFER_SIZE);
    for (uint64_t done = c->written; done < c->total; ) {
        if (c->cancelled || c->err) return;
        size_t chunk = std::min((size_t)INSTALL_BUFFER_SIZE, c->total - done);
        fseeko(c->fp, c->foff, SEEK_SET);
        size_t got = fread(rb.get(), 1, chunk, c->fp);
        if (got < chunk) {
            gcLog("read short at offset %lu: expected %zu, got %zu (gamecard removed or read error)\n",
                  c->foff, chunk, got);
            g_lastGcError = "Lost contact with the gamecard while reading. It may have been removed.";
            std::lock_guard<std::mutex> lk(c->mtx);
            c->err = true;
            c->canW.notify_one();
            return;
        }
        { std::unique_lock<std::mutex> lk(c->mtx);
          c->canR.wait(lk, [c]{ return c->bufSz == 0 || c->cancelled || c->err; });
          if (c->cancelled || c->err) return;
          memcpy(c->buf, rb.get(), chunk); c->bufSz = chunk; }
        c->canW.notify_one();
        c->foff += chunk; done += chunk;
    }
}
static void ncaWriter(NcaCtx* c) {
    while (c->written < c->total) {
        if (c->cancelled || c->err) return;
        size_t chunk;
        { std::unique_lock<std::mutex> lk(c->mtx);
          c->canW.wait(lk, [c]{ return c->bufSz > 0 || c->cancelled || c->err; });
          if (c->cancelled || c->err) return;
          chunk = c->bufSz; }
        Result rc = ncmContentStorageWritePlaceHolder(&c->cs, &c->pid, c->written, c->buf, chunk);
        if (R_FAILED(rc)) {
            gcLog("write fail 0x%x\n", rc);
            g_lastGcError = "Failed to write content to storage during gamecard install.";
            std::lock_guard<std::mutex> lk(c->mtx);
            c->err = true;
            c->canR.notify_one();
            return;
        }
        c->written += chunk;
        { std::lock_guard<std::mutex> lk(c->mtx); c->bufSz = 0; }
        c->canR.notify_one();
    }
}

static bool VerifyExistingNca(NcmContentStorage& cs, const NcmContentId& contentId, u64 expectedSize, const std::string& name) {
    inst::ui::instPage::setInstInfoText("inst.info_page.verifying"_lang + name + ".nca...");

    constexpr size_t kChunk = 1024 * 1024;
    auto buf = std::make_unique<u8[]>(kChunk);
    Sha256Context ctx;
    sha256ContextCreate(&ctx);

    u64 offset = 0;
    while (offset < expectedSize) {
        size_t toRead = static_cast<size_t>(std::min<u64>(kChunk, expectedSize - offset));
        Result rc = ncmContentStorageReadContentIdFile(&cs, buf.get(), toRead, &contentId, static_cast<s64>(offset));
        if (R_FAILED(rc)) return false;
        sha256ContextUpdate(&ctx, buf.get(), toRead);
        offset += toRead;

        const float pct = expectedSize > 0 ? (static_cast<float>(offset) / static_cast<float>(expectedSize)) * 100.0f : 100.0f;
        inst::ui::instPage::setInstBarPerc(pct);
        if (inst::ui::instPage::isInstallCancelRequested()) return false;
    }

    u8 hash[32];
    sha256ContextGetHash(&ctx, hash);
    return std::memcmp(hash, &contentId, sizeof(NcmContentId)) == 0;
}

static bool InstallNca(const std::string& path, const NcmContentId& cid,
                        NcmStorageId sid, const std::string& name) {
    inst::ui::instPage::setInstInfoText("inst.info_page.top_info0"_lang + name + ".nca...");

    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) { gcLog("open fail: %s\n", path.c_str()); return false; }

    NcaFullHeader encH = {}, h = {};
    if (fread(&encH, 1, GC_NCA_HEADER_SIZE, fp) != GC_NCA_HEADER_SIZE) { fclose(fp); return false; }
    DecryptHeader(&encH, &h);
    if (h.magic != NCA_MAGIC) { gcLog("bad magic\n"); fclose(fp); return false; }

    gcLog("NCA %s: dist=%u content=%u size=%lu keygen=%u\n",
          name.c_str(), h.distribution_type, h.content_type, h.size,
          h.key_gen ? h.key_gen : h.old_key_gen);

    h.distribution_type = 0;

    NcaFullHeader reEnc = {};
    EncryptHeader(&h, &reEnc);

    {
        NcaFullHeader verify = {};
        DecryptHeader(&reEnc, &verify);
        gcLog("verify: magic=0x%x dist=%u size=%lu\n", verify.magic, verify.distribution_type, verify.size);
        if (verify.distribution_type != 0) {
            gcLog("ERROR: distribution type not flipped after re-encrypt!\n");
        }
    }

    // setup placeholder
    NcmContentStorage cs = {};
    if (R_FAILED(ncmOpenContentStorage(&cs, sid))) { fclose(fp); return false; }
    NcmPlaceHolderId pid = {};
    ncmContentStorageGeneratePlaceHolderId(&cs, &pid);
    ncmContentStorageDeletePlaceHolder(&cs, &pid);
    Result rc = ncmContentStorageCreatePlaceHolder(&cs, &cid, &pid, (s64)h.size);
    if (R_FAILED(rc)) { gcLog("create ph fail 0x%x sz=%lu\n", rc, h.size);
        ncmContentStorageClose(&cs); fclose(fp); return false; }

    // write modified header
    rc = ncmContentStorageWritePlaceHolder(&cs, &pid, 0, &reEnc, GC_NCA_HEADER_SIZE);
    if (R_FAILED(rc)) { gcLog("write hdr fail\n");
        ncmContentStorageDeletePlaceHolder(&cs, &pid);
        ncmContentStorageClose(&cs); fclose(fp); return false; }

    if (h.size > GC_NCA_HEADER_SIZE) {
        auto shared = std::make_unique<uint8_t[]>(INSTALL_BUFFER_SIZE);
        NcaCtx ctx = {};
        ctx.fp = fp; ctx.buf = shared.get(); ctx.foff = GC_NCA_HEADER_SIZE;
        ctx.bufSz = 0; ctx.written = GC_NCA_HEADER_SIZE; ctx.total = h.size;
        ctx.cs = cs; ctx.pid = pid; ctx.cid = cid; ctx.err = false;

        std::thread tR(ncaReader, &ctx), tW(ncaWriter, &ctx);
        u64 lastWritten = GC_NCA_HEADER_SIZE;
        double lastMbps = 0.0;
        double emaRate = 0.0;
        auto lastTime = std::chrono::steady_clock::now();
        while (ctx.written < ctx.total && !ctx.err) {
            if (inst::ui::instPage::isInstallCancelRequested()) {
                ctx.cancelled = true;
                ctx.canR.notify_all();
                ctx.canW.notify_all();
                tR.join(); tW.join();
                ncmContentStorageDeletePlaceHolder(&cs, &pid);
                ncmContentStorageClose(&cs);
                fclose(fp);
                THROW_FORMAT("Installation canceled.");
            }
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - lastTime).count();
            float pct = (float)ctx.written / (float)ctx.total * 100.0f;

            if (elapsed >= 0.5) {
                u64 delta = ctx.written - lastWritten;
                lastMbps = (delta / elapsed) / (1024.0 * 1024.0);
                double rate = delta / elapsed;
                if (rate > 0.0) {
                    emaRate = (emaRate <= 0.0) ? rate : (emaRate * 0.7 + rate * 0.3);
                }
                lastWritten = ctx.written;
                lastTime = now;
            }

            std::string etaText = "Calculating...";
            if (emaRate > 0.0 && ctx.written < ctx.total) {
                const auto remaining = static_cast<std::uint64_t>(ctx.total - ctx.written);
                const auto seconds = static_cast<std::uint64_t>(remaining / emaRate);
                const auto eh = seconds / 3600;
                const auto em = (seconds % 3600) / 60;
                const auto es = seconds % 60;
                if (eh > 0) {
                    etaText = std::to_string(eh) + ":" + (em < 10 ? "0" : "") + std::to_string(em)
                        + ":" + (es < 10 ? "0" : "") + std::to_string(es);
                } else {
                    etaText = std::to_string(em) + ":" + (es < 10 ? "0" : "") + std::to_string(es);
                }
                etaText += " remaining";
            }

            char pb[128];
            snprintf(pb, sizeof(pb), "%d%% • %s • %.1f MB/s",
                     (int)(pct + 0.5f), etaText.c_str(), lastMbps);
            inst::ui::instPage::setProgressDetailText(pb);
            inst::ui::instPage::setInstBarPerc(pct);
            svcSleepThread(100000000ULL);
        }
        tR.join(); tW.join();
        if (ctx.err) { ncmContentStorageDeletePlaceHolder(&cs, &pid);
            ncmContentStorageClose(&cs); fclose(fp); return false; }
    }
    fclose(fp);

    // delete existing and register
    bool has = false;
    if (R_SUCCEEDED(ncmContentStorageHas(&cs, &has, &cid)) && has)
        ncmContentStorageDelete(&cs, &cid);
    rc = ncmContentStorageRegister(&cs, &cid, &pid);
    if (R_FAILED(rc)) { gcLog("REGISTER FAIL 0x%x\n", rc);
        ncmContentStorageDeletePlaceHolder(&cs, &pid);
        ncmContentStorageClose(&cs); return false; }
    ncmContentStorageClose(&cs);
    gcLog("installed %s OK\n", name.c_str());
    return true;
}

namespace inst::gc::direct {

    std::string GetLastError() {
        return g_lastGcError;
    }

// enumerateContent parse all CNMTs, return one entry per title, no install
std::vector<GameCardContentEntry> EnumerateContent(const std::string& mountPath) {
    std::vector<GameCardContentEntry> result;

    DIR* dir = opendir(mountPath.c_str());
    if (!dir) return result;
    std::vector<std::string> cnmtFiles;
    struct dirent* e;
    while ((e = readdir(dir))) {
        std::string fn = e->d_name;
        if (fn.find(".cnmt.nca") != std::string::npos && fn.size() >= 32)
            cnmtFiles.push_back(fn);
    }
    closedir(dir);

    for (const auto& cfn : cnmtFiles) {
        NcmContentId cid = IdFromString(cfn.substr(0, 32).c_str());
        std::string cfull = mountPath + cfn;
        struct stat cst = {};
        if (stat(cfull.c_str(), &cst) != 0) continue;

        ParsedCnmtData parsed = {};
        if (!ParseCnmtFromFile(cfull, cid, cst.st_size, parsed)) continue;

        GameCardContentEntry entry = {};
        entry.key   = parsed.key;
        entry.appId = AppIdFromTitle(parsed.key.id, parsed.key.type);
        entry.keyGen = parsed.keyGen;

        // sum the sizes of all NCAs listed in this CNMT
        for (const auto& info : parsed.contentInfos) {
            u64 sz = 0;
            ncmContentInfoSizeToU64(&info, &sz);
            entry.totalNcaSize += sz;
        }

        result.push_back(entry);
    }

    // sort: Application first, then Patch, then AddOnContent
    std::sort(result.begin(), result.end(), [](const GameCardContentEntry& a,
                                               const GameCardContentEntry& b) {
        return a.key.type < b.key.type;
    });

    return result;
}

// install only chosen entries by index
bool InstallSelectedFromGamecard(const std::string& mountPath, NcmStorageId storageId,
                                  const std::vector<size_t>& selection) {
    gcLog("=== INSTALL SELECTED START (%zu entries) ===\n", selection.size());
    g_lastGcError.clear();

    // enumerate all files and all CNMTs on the card
    DIR* dir = opendir(mountPath.c_str());
    if (!dir) return false;
    std::vector<std::string> allFiles, cnmtFiles;
    struct dirent* e;
    while ((e = readdir(dir))) {
        std::string fn = e->d_name;
        allFiles.push_back(fn);
        if (fn.find(".cnmt.nca") != std::string::npos) cnmtFiles.push_back(fn);
    }
    closedir(dir);
    if (cnmtFiles.empty()) return false;

    std::vector<ParsedCnmtData> allParsed;
    for (const auto& cfn : cnmtFiles) {
        if (cfn.size() < 32) continue;
        NcmContentId cnmtCid = IdFromString(cfn.substr(0, 32).c_str());
        std::string cfull = mountPath + cfn;
        struct stat cst = {}; if (stat(cfull.c_str(), &cst) != 0) continue;
        ParsedCnmtData parsed = {};
        if (!ParseCnmtFromFile(cfull, cnmtCid, cst.st_size, parsed)) {
            gcLog("cnmt parse fail: %s\n", cfn.c_str());
            return false;
        }
        allParsed.push_back(parsed);
    }
    if (allParsed.empty()) return false;

    // Sort to match the ordering EnumerateContent() produces, so that
    // selection indices from callers (which use EnumerateContent) are correct.
    std::sort(allParsed.begin(), allParsed.end(), [](const ParsedCnmtData& a, const ParsedCnmtData& b) {
        return a.key.type < b.key.type;
    });

    std::vector<ParsedCnmtData> selectedParsed;
    for (size_t idx : selection) {
        if (idx >= allParsed.size()) {
            gcLog("invalid selection index %zu (have %zu)\n", idx, allParsed.size());
            return false;
        }
        selectedParsed.push_back(allParsed[idx]);
    }
    if (selectedParsed.empty()) return false;

    struct ExistingContentInfo {
        bool foundExisting = false;
        NcmContentMetaKey existingKey = {};
        NcmStorageId existingSid = NcmStorageId_None;
    };
    std::vector<ExistingContentInfo> existingInfoList(selectedParsed.size());

    for (size_t i = 0; i < selectedParsed.size(); i++) {
        auto& parsed = selectedParsed[i];
        auto& info = existingInfoList[i];

        for (NcmStorageId sid : { NcmStorageId_SdCard, NcmStorageId_BuiltInUser }) {
            NcmContentMetaDatabase db = {};
            if (R_FAILED(ncmOpenContentMetaDatabase(&db, sid))) continue;
            NcmContentMetaKey key = {};
            if (R_SUCCEEDED(ncmContentMetaDatabaseGetLatestContentMetaKey(&db, &key, parsed.key.id))
                && key.type == parsed.key.type) {
                info.existingKey = key;
                info.existingSid = sid;
                info.foundExisting = true;
            }
            ncmContentMetaDatabaseClose(&db);
            if (info.foundExisting) break;
        }

        if (info.foundExisting) {
            const char* typeStr = "content";
            if (parsed.key.type == NcmContentMetaType_Application) typeStr = "base game";
            else if (parsed.key.type == NcmContentMetaType_Patch) typeStr = "update";
            else if (parsed.key.type == NcmContentMetaType_AddOnContent) typeStr = "DLC";
            const char* locStr = (info.existingSid == NcmStorageId_SdCard) ? "SD card" : "system memory";

            if (info.existingKey.version == parsed.key.version && !inst::config::autoSkipReinstall) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                    "The same version of this %s is already installed on %s.\n\nReinstall?",
                    typeStr, locStr);
                int choice = inst::ui::mainApp->CreateShowDialog(
                    "Already installed", msg, {"Reinstall", "common.cancel"_lang}, false);
                if (choice != 0) return false;
            }
        }
    }

    auto pushRecordsFor = [&](const std::vector<ParsedCnmtData>& sel) {
        uint64_t appId = AppIdFromTitle(allParsed[0].key.id, allParsed[0].key.type);
        std::vector<ContentStorageRecord> records;

        for (auto& parsed : allParsed) {
            bool isSelected = false;
            for (auto& s : sel) {
                if (s.key.id == parsed.key.id && s.key.type == parsed.key.type)
                { isSelected = true; break; }
            }

            if (isSelected) {
                ContentStorageRecord rec = {};
                rec.metaRecord = parsed.key;
                rec.storageId  = (u64)storageId;
                records.push_back(rec);
            } else {
                for (NcmStorageId sid : { NcmStorageId_SdCard, NcmStorageId_BuiltInUser }) {
                    NcmContentMetaDatabase db = {};
                    if (R_FAILED(ncmOpenContentMetaDatabase(&db, sid))) continue;
                    NcmContentMetaKey key = {};
                    bool found = R_SUCCEEDED(ncmContentMetaDatabaseGetLatestContentMetaKey(
                                     &db, &key, parsed.key.id)) && key.type == parsed.key.type;
                    ncmContentMetaDatabaseClose(&db);
                    if (found) {
                        ContentStorageRecord rec = {};
                        rec.metaRecord = key;
                        rec.storageId  = (u64)sid;
                        records.push_back(rec);
                        break;
                    }
                }
            }
        }

        nsDeleteApplicationRecord(appId);
        Result rc = nsPushApplicationRecord(appId, NsApplicationRecordType_Installed,
                    records.data(), records.size());
        if (R_FAILED(rc)) {
            gcLog("push attempt 1 fail: 0x%x, trying merge\n", rc);
            std::vector<ContentStorageRecord> existing;
            s32 cnt2 = 0;
            if (R_SUCCEEDED(nsCountApplicationContentMeta(appId, &cnt2)) && cnt2 > 0) {
                existing.resize(cnt2);
                u32 got = 0;
                if (R_SUCCEEDED(nsListApplicationRecordContentMeta(0, appId, existing.data(), cnt2, &got)))
                    existing.resize(got);
                else
                    existing.clear();
            }
            std::vector<ContentStorageRecord> merged = existing;
            for (auto& newRec : records) {
                bool replaced = false;
                for (auto& m : merged) {
                    if (m.metaRecord.id == newRec.metaRecord.id && m.metaRecord.type == newRec.metaRecord.type)
                    { m = newRec; replaced = true; break; }
                }
                if (!replaced) merged.push_back(newRec);
            }
            nsDeleteApplicationRecord(appId);
            rc = nsPushApplicationRecord(appId, NsApplicationRecordType_Installed,
                        merged.data(), merged.size());
            if (R_FAILED(rc)) {
                gcLog("push attempt 2 fail: 0x%x, last resort\n", rc);
                rc = nsPushApplicationRecord(appId, NsApplicationRecordType_Installed,
                            records.data(), records.size());
                if (R_FAILED(rc))
                    gcLog("WARNING: all push attempts failed (0x%x), proceeding\n", rc);
            }
        }
    };

    auto rollbackRegistration = [&]() {
        for (auto& p : selectedParsed) {
            NcmContentMetaDatabase db = {};
            if (R_SUCCEEDED(ncmOpenContentMetaDatabase(&db, storageId))) {
                ncmContentMetaDatabaseRemove(&db, &p.key);
                ncmContentMetaDatabaseCommit(&db);
                ncmContentMetaDatabaseClose(&db);
            }
        }
        pushRecordsFor({});
        const u64 appId2 = AppIdFromTitle(allParsed[0].key.id, allParsed[0].key.type);
        inst::util::FixAvmFloorForTitle(appId2);
        gcLog("rolled back registration after failed content copy\n");
    };

    for (auto& parsed : selectedParsed) {
        if (!SetDb(parsed, storageId)) return false;
    }
    pushRecordsFor(selectedParsed);
    {
        u32 maxVersion = 0;
        for (auto& parsed : selectedParsed) {
            if ((parsed.key.type == NcmContentMetaType_Application ||
                 parsed.key.type == NcmContentMetaType_Patch) &&
                parsed.key.version > maxVersion)
                maxVersion = parsed.key.version;
        }
        const u64 appId2 = AppIdFromTitle(allParsed[0].key.id, allParsed[0].key.type);
        inst::util::SetAvmLaunchFloor(appId2, maxVersion);
    }

    // import tickets for selected entries only
    for (auto& parsed : selectedParsed)
        ImportTickets(mountPath, allFiles, parsed.key.id);

    for (size_t ci = 0; ci < selectedParsed.size(); ci++) {
        auto& parsed = selectedParsed[ci];
        for (size_t ni = 0; ni < parsed.contentInfos.size(); ni++) {
            const auto& info = parsed.contentInfos[ni];
            std::string ncaIdStr = IdToString(info.content_id);

            { NcmContentStorage cs = {};
              if (R_SUCCEEDED(ncmOpenContentStorage(&cs, storageId))) {
                  bool has = false;
                  ncmContentStorageHas(&cs, &has, &info.content_id);
                  bool needsRepair = false;
                  if (has && inst::config::gcVerifyRepair) {
                      u64 expectedSize = 0;
                      ncmContentInfoSizeToU64(&info, &expectedSize);
                      if (!VerifyExistingNca(cs, info.content_id, expectedSize, ncaIdStr)) {
                          if (inst::ui::instPage::isInstallCancelRequested()) {
                              ncmContentStorageClose(&cs);
                              g_lastGcError = "Installation canceled.";
                              rollbackRegistration();
                              return false;
                          }
                          needsRepair = true;
                          gcLog("existing NCA %s failed verification, repairing\n", ncaIdStr.c_str());
                      }
                  }
                  ncmContentStorageClose(&cs);
                  if (has && !needsRepair) { gcLog("NCA already present, skipping\n"); continue; }
              }
            }

            std::string ncaPath;
            for (const auto& fn : allFiles) {
                if (fn.size() < 32) continue;
                std::string fnLower = fn.substr(0, 32);
                for (auto& c : fnLower) c = std::tolower(c);
                if (fnLower == ncaIdStr && fn.find(".nca") != std::string::npos) {
                    ncaPath = mountPath + fn;
                    break;
                }
            }
            if (ncaPath.empty()) {
                gcLog("NCA file missing for id %s\n", ncaIdStr.c_str());
                g_lastGcError = "Lost contact with the gamecard (a required file disappeared). It may have been removed.";
                rollbackRegistration();
                return false;
            }

            struct stat nst = {};
            if (stat(ncaPath.c_str(), &nst) != 0) {
                gcLog("NCA stat failed: %s\n", ncaPath.c_str());
                g_lastGcError = "Lost contact with the gamecard. It may have been removed.";
                rollbackRegistration();
                return false;
            }

            gcLog("installing NCA %s (%zu/%zu of entry %zu/%zu)\n",
                  ncaPath.c_str(), ni + 1, parsed.contentInfos.size(),
                  ci + 1, selectedParsed.size());

            if (!InstallNca(ncaPath, info.content_id, storageId, ncaIdStr)) {
                rollbackRegistration();
                return false;
            }
        }
    }

    for (size_t i = 0; i < selectedParsed.size(); i++) {
        auto& info = existingInfoList[i];
        auto& parsed = selectedParsed[i];
        if (!info.foundExisting) continue;
        if (info.existingKey.version == parsed.key.version && info.existingSid == storageId) continue;

        gcLog("deleting superseded content from storage %u\n", info.existingSid);
        DeleteExistingGcContent(info.existingKey, info.existingSid);
    }

    gcLog("=== INSTALL SELECTED DONE ===\n");
    return true;
}

std::uint64_t GetGamecardAppId(const std::string& mountPath) {
    DIR* dir = opendir(mountPath.c_str());
    if (!dir) return 0;
    
    std::uint64_t bestAppId = 0;
    struct dirent* e;
    while ((e = readdir(dir))) {
        std::string fn = e->d_name;
        if (fn.find(".cnmt.nca") == std::string::npos || fn.size() < 32) continue;
        NcmContentId cid = IdFromString(fn.substr(0,32).c_str());
        std::string full = mountPath + fn;
        struct stat st = {}; if (stat(full.c_str(), &st) != 0) continue;
        ParsedCnmtData p = {};
        if (ParseCnmtFromFile(full, cid, st.st_size, p)) {
            uint64_t aid = AppIdFromTitle(p.key.id, p.key.type);
            if (bestAppId == 0 || p.key.type == NcmContentMetaType_Application) {
                bestAppId = aid;
                if (p.key.type == NcmContentMetaType_Application) break; // prefer base
            }
        }
    }
    closedir(dir);
    return bestAppId;
}

bool InstallAllFromGamecard(const std::string& mountPath, NcmStorageId storageId) {
    auto entries = EnumerateContent(mountPath);
    if (entries.empty()) return false;
    std::vector<size_t> all;
    for (size_t i = 0; i < entries.size(); i++) all.push_back(i);
    return InstallSelectedFromGamecard(mountPath, storageId, all);
}


}
