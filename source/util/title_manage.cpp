#include "util/title_manage.hpp"
#include "util/avm_maintenance.hpp"
#include "nx/ipc/ns_ext.h"

namespace inst::util {

    namespace {
        constexpr std::uint64_t kPatchIdOffset            = 0x800ULL;
        constexpr std::uint64_t kAddOnContentIdOffset      = 0x1000ULL;
        constexpr std::uint64_t kAddOnContentConversionMask = 0xFFFFFFFFFFFFF000ULL;
        constexpr std::uint64_t kAddOnContentMinIndex      = 1;
        constexpr std::uint64_t kAddOnContentMaxIndex      = 2000;

        std::uint64_t PatchIdForApp(std::uint64_t baseAppId) {
            return baseAppId + kPatchIdOffset;
        }

        std::uint64_t AddOnContentBaseId(std::uint64_t baseAppId) {
            return (baseAppId & kAddOnContentConversionMask) + kAddOnContentIdOffset;
        }

        std::uint64_t GetContentMetaTotalSize(NcmContentMetaDatabase& db, const NcmContentMetaKey& key) {
            std::uint64_t total = 0;
            s32 offset = 0;
            constexpr s32 kBatch = 32;
            NcmContentInfo infos[kBatch];
            while (true) {
                s32 got = 0;
                if (R_FAILED(ncmContentMetaDatabaseListContentInfo(&db, &got, infos, kBatch, &key, offset))) break;
                if (got == 0) break;
                for (s32 i = 0; i < got; i++) {
                    std::uint64_t sz = 0;
                    ncmContentInfoSizeToU64(&infos[i], &sz);
                    total += sz;
                }
                offset += got;
                if (got < kBatch) break;
            }
            return total;
        }

        void DeleteContentMetaAndNcas(const NcmContentMetaKey& key, NcmStorageId storageId) {
            NcmContentMetaDatabase db = {};
            if (R_FAILED(ncmOpenContentMetaDatabase(&db, storageId))) return;
            NcmContentStorage cs = {};
            const bool hasCs = R_SUCCEEDED(ncmOpenContentStorage(&cs, storageId));

            s32 offset = 0;
            constexpr s32 kBatch = 32;
            NcmContentInfo infos[kBatch];
            while (true) {
                s32 got = 0;
                if (R_FAILED(ncmContentMetaDatabaseListContentInfo(&db, &got, infos, kBatch, &key, offset))) break;
                if (got == 0) break;
                if (hasCs) {
                    for (s32 i = 0; i < got; i++) {
                        bool has = false;
                        if (R_SUCCEEDED(ncmContentStorageHas(&cs, &has, &infos[i].content_id)) && has)
                            ncmContentStorageDelete(&cs, &infos[i].content_id);
                    }
                }
                offset += got;
                if (got < kBatch) break;
            }
            NcmContentId cnmtId = {};
            if (R_SUCCEEDED(ncmContentMetaDatabaseGetContentIdByType(&db, &cnmtId, &key, NcmContentType_Meta))) {
                bool has = false;
                if (hasCs && R_SUCCEEDED(ncmContentStorageHas(&cs, &has, &cnmtId)) && has)
                    ncmContentStorageDelete(&cs, &cnmtId);
            }
            if (hasCs) ncmContentStorageClose(&cs);
            ncmContentMetaDatabaseRemove(&db, &key);
            ncmContentMetaDatabaseCommit(&db);
            ncmContentMetaDatabaseClose(&db);
        }
    }

    std::vector<InstalledContentPiece> GetInstalledContentForTitle(std::uint64_t baseAppId) {
        std::vector<InstalledContentPiece> result;
        const std::uint64_t patchId = PatchIdForApp(baseAppId);
        const std::uint64_t aocBase = AddOnContentBaseId(baseAppId);
        const std::uint64_t aocMin  = aocBase + kAddOnContentMinIndex;
        const std::uint64_t aocMax  = aocBase + kAddOnContentMaxIndex;

        for (NcmStorageId sid : { NcmStorageId_SdCard, NcmStorageId_BuiltInUser }) {
            NcmContentMetaDatabase db = {};
            if (R_FAILED(ncmOpenContentMetaDatabase(&db, sid))) continue;

            NcmContentMetaKey baseKey = {};
            if (R_SUCCEEDED(ncmContentMetaDatabaseGetLatestContentMetaKey(&db, &baseKey, baseAppId))
                && baseKey.type == NcmContentMetaType_Application) {
                InstalledContentPiece piece;
                piece.key = baseKey;
                piece.storageId = sid;
                piece.sizeBytes = GetContentMetaTotalSize(db, baseKey);
                piece.label = "Base game";
                piece.isBase = true;
                result.push_back(piece);
            }

            NcmContentMetaKey patchKey = {};
            if (R_SUCCEEDED(ncmContentMetaDatabaseGetLatestContentMetaKey(&db, &patchKey, patchId))
                && patchKey.type == NcmContentMetaType_Patch) {
                InstalledContentPiece piece;
                piece.key = patchKey;
                piece.storageId = sid;
                piece.sizeBytes = GetContentMetaTotalSize(db, patchKey);
                piece.label = "Update";
                piece.isBase = false;
                result.push_back(piece);
            }

            NcmContentMetaKey probe = {};
            s32 total = 0, written = 0;
            Result rc = ncmContentMetaDatabaseList(&db, &total, &written, &probe, 1,
                NcmContentMetaType_AddOnContent, 0, aocMin, aocMax, NcmContentInstallType_Full);
            if (R_SUCCEEDED(rc) && total > 0) {
                std::vector<NcmContentMetaKey> keys(static_cast<std::size_t>(total));
                rc = ncmContentMetaDatabaseList(&db, &total, &written, keys.data(), total,
                    NcmContentMetaType_AddOnContent, 0, aocMin, aocMax, NcmContentInstallType_Full);
                if (R_SUCCEEDED(rc)) {
                    int dlcIndex = 1;
                    for (s32 i = 0; i < written; i++) {
                        InstalledContentPiece piece;
                        piece.key = keys[i];
                        piece.storageId = sid;
                        piece.sizeBytes = GetContentMetaTotalSize(db, keys[i]);
                        piece.label = "DLC #" + std::to_string(dlcIndex++);
                        piece.isBase = false;
                        result.push_back(piece);
                    }
                }
            }

            ncmContentMetaDatabaseClose(&db);
        }

        return result;
    }

    bool DeleteInstalledContentPiece(std::uint64_t baseAppId, const InstalledContentPiece& piece) {
        DeleteContentMetaAndNcas(piece.key, piece.storageId);

        auto remaining = GetInstalledContentForTitle(baseAppId);
        std::vector<ContentStorageRecord> records;
        for (auto& p : remaining) {
            ContentStorageRecord rec = {};
            rec.metaRecord = p.key;
            rec.storageId  = static_cast<std::uint64_t>(p.storageId);
            records.push_back(rec);
        }

        nsDeleteApplicationRecord(baseAppId);
        if (!records.empty())
            nsPushApplicationRecord(baseAppId, NsApplicationRecordType_Installed, records.data(), records.size());

        FixAvmFloorForTitle(baseAppId);

        return true;
    }

    bool DeleteTitleCompletely(std::uint64_t baseAppId) {
        auto pieces = GetInstalledContentForTitle(baseAppId);
        for (auto& piece : pieces) {
            DeleteContentMetaAndNcas(piece.key, piece.storageId);
        }

        nsDeleteApplicationRecord(baseAppId);
        FixAvmFloorForTitle(baseAppId);
        return true;
    }

    bool IsCurrentTakeoverTarget(std::uint64_t baseAppId) {
        if (R_FAILED(pmdmntInitialize())) return false;
        u64 pid = 0;
        Result rc = pmdmntGetApplicationProcessId(&pid);
        pmdmntExit();
        if (R_FAILED(rc)) return false;

        if (R_FAILED(pminfoInitialize())) return false;
        u64 programId = 0;
        rc = pminfoGetProgramId(&programId, pid);
        pminfoExit();
        if (R_FAILED(rc)) return false;

        return programId == baseAppId;
    }

}
