/*
Copyright (c) 2017-2018 Adubbz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "install/install.hpp"
#include "nx/ipc/ns_ext.h"

#include <switch.h>
#include <cstring>
#include <memory>
#include <algorithm>
#include "util/error.hpp"
#include "util/install_diagnostics.hpp"
#include "ui/instPage.hpp"
#include "ui/MainApplication.hpp"
#include "util/lang.hpp"

#include "nx/ncm.hpp"
#include "util/config.hpp"
#include "util/title_util.hpp"

static u64 AvmCurrentTimestamp() {
    u64 ts = 0;
    if (R_FAILED(timeGetCurrentTime(TimeType_UserSystemClock, &ts)))
        ts = 4102444800ULL;
    return ts;
}

namespace inst::ui { extern MainApplication *mainApp; }

namespace leaf::install
{
    Install::Install(NcmStorageId destStorageId, bool ignoreReqFirmVersion) :
        m_destStorageId(destStorageId), m_ignoreReqFirmVersion(ignoreReqFirmVersion), m_contentMeta()
    {
        appletSetMediaPlaybackState(true);
    }

    Install::~Install()
    {
        appletSetMediaPlaybackState(false);
    }

    bool Install::IsSessionInstalledNca(const NcmContentId& ncaId) const
    {
        return std::any_of(m_sessionInstalledNcas.begin(), m_sessionInstalledNcas.end(),
            [&ncaId](const NcmContentId& existing) {
                return std::memcmp(&existing, &ncaId, sizeof(NcmContentId)) == 0;
            });
    }

    void Install::TrackSessionInstalledNca(const NcmContentId& ncaId)
    {
        if (!IsSessionInstalledNca(ncaId))
            m_sessionInstalledNcas.push_back(ncaId);
    }

    void Install::CleanupSessionInstalledNcas()
    {
        if (m_sessionInstalledNcas.empty())
            return;

        nx::ncm::ContentStorage contentStorage(m_destStorageId);
        for (const NcmContentId& ncaId : m_sessionInstalledNcas) {
            try {
                contentStorage.DeletePlaceholder(*(const NcmPlaceHolderId*)&ncaId);
            } catch (...) {}

            try {
                if (contentStorage.Has(ncaId))
                    contentStorage.Delete(ncaId);
            } catch (...) {}
        }
        m_sessionInstalledNcas.clear();
    }

    void Install::InstallContentMetaRecords(leaf::data::ByteBuffer& installContentMetaBuf, int i)
    {
        NcmContentMetaDatabase contentMetaDatabase;
        NcmContentMetaKey contentMetaKey = m_contentMeta[i].GetContentMetaKey();

        try
        {
            ASSERT_OK(ncmOpenContentMetaDatabase(&contentMetaDatabase, m_destStorageId), "Failed to open content meta database");
            ASSERT_OK(ncmContentMetaDatabaseSet(&contentMetaDatabase, &contentMetaKey, (NcmContentMetaHeader*)installContentMetaBuf.GetData(), installContentMetaBuf.GetSize()), "Failed to set content records");
            ASSERT_OK(ncmContentMetaDatabaseCommit(&contentMetaDatabase), "Failed to commit content records");
        }
        catch (std::runtime_error& e)
        {
            serviceClose(&contentMetaDatabase.s);
            THROW_FORMAT(e.what());
        }

        serviceClose(&contentMetaDatabase.s);
    }

    void Install::InstallApplicationRecord(int i)
    {
        const u64 baseTitleId = leaf::util::GetBaseTitleId(this->GetTitleId(i), this->GetContentMetaType(i));

        ContentStorageRecord newRecord;
        newRecord.metaRecord = m_contentMeta[i].GetContentMetaKey();
        newRecord.storageId  = m_destStorageId;

        std::vector<ContentStorageRecord> allRecords;
        s32 existingCount = 0;
        if (R_SUCCEEDED(nsCountApplicationContentMeta(baseTitleId, &existingCount)) && existingCount > 0) {
            allRecords.resize(existingCount);
            u32 real = 0;
            if (R_FAILED(nsListApplicationRecordContentMeta(0, baseTitleId,
                    allRecords.data(), existingCount, &real))) {
                allRecords.clear();
            } else {
                allRecords.resize(real);
            }
        }

        for (auto& r : allRecords) {
            if (r.storageId == 2) {
                NcmContentMetaDatabase db = {};
                if (R_SUCCEEDED(ncmOpenContentMetaDatabase(&db, m_destStorageId))) {
                    NcmContentMetaKey key = {};
                    if (R_SUCCEEDED(ncmContentMetaDatabaseGetLatestContentMetaKey(&db, &key, r.metaRecord.id))) {
                        r.storageId = (u64)m_destStorageId;
                    }
                    ncmContentMetaDatabaseClose(&db);
                }
                if (r.storageId == 2) {
                    r.storageId = (u64)m_destStorageId;
                }
            }
        }

        allRecords.erase(std::remove_if(allRecords.begin(), allRecords.end(),
            [&](const ContentStorageRecord& r) {
                // remove records that match our new entry (we're replacing it)
                return r.metaRecord.id == newRecord.metaRecord.id &&
                       r.metaRecord.type == newRecord.metaRecord.type;
            }), allRecords.end());
        allRecords.push_back(newRecord);

        LOG_DEBUG("Record push details for %016lX:\n", baseTitleId);
        for (size_t ri = 0; ri < allRecords.size(); ri++) {
            LOG_DEBUG("  rec[%zu]: id=%016lX type=%u ver=%u sid=%lu\n",
                      ri, allRecords[ri].metaRecord.id, allRecords[ri].metaRecord.type,
                      allRecords[ri].metaRecord.version, allRecords[ri].storageId);
        }

        nsDeleteApplicationRecord(baseTitleId);

        LOG_DEBUG("Pushing application record with %zu entries...\n", allRecords.size());

        Result pushRc = nsPushApplicationRecord(baseTitleId, NsApplicationRecordType_Installed,
                        allRecords.data(), static_cast<u32>(allRecords.size()));

        if (R_FAILED(pushRc)) {
            LOG_DEBUG("nsPushApplicationRecord attempt 1 failed: 0x%x, trying merge...\n", pushRc);

            // re-read whatever NS currently reports and merge our digital records into it,
            // replacing any storageId=2 entries for our title ID/type with digital ones
            std::vector<ContentStorageRecord> merged;
            s32 cnt2 = 0;
            if (R_SUCCEEDED(nsCountApplicationContentMeta(baseTitleId, &cnt2)) && cnt2 > 0) {
                merged.resize(cnt2);
                u32 got = 0;
                if (R_FAILED(nsListApplicationRecordContentMeta(0, baseTitleId,
                        merged.data(), cnt2, &got))) {
                    merged.clear();
                } else {
                    merged.resize(got);
                }
            }
            for (auto& newRec : allRecords) {
                bool replaced = false;
                for (auto& m : merged) {
                    if (m.metaRecord.id == newRec.metaRecord.id &&
                        m.metaRecord.type == newRec.metaRecord.type) {
                        m = newRec;
                        replaced = true;
                        break;
                    }
                }
                if (!replaced)
                    merged.push_back(newRec);
            }

            nsDeleteApplicationRecord(baseTitleId);
            pushRc = nsPushApplicationRecord(baseTitleId, NsApplicationRecordType_Installed,
                        merged.data(), static_cast<u32>(merged.size()));

            if (R_FAILED(pushRc)) {
                LOG_DEBUG("nsPushApplicationRecord attempt 2 (merged) failed: 0x%x, last resort...\n", pushRc);
                pushRc = nsPushApplicationRecord(baseTitleId, NsApplicationRecordType_Installed,
                            allRecords.data(), static_cast<u32>(allRecords.size()));
                if (R_FAILED(pushRc)) {
                    LOG_DEBUG("nsPushApplicationRecord all attempts failed: 0x%x, proceeding anyway\n", pushRc);
                }
            }
        }

        const NcmContentMetaType recordType = static_cast<NcmContentMetaType>(m_contentMeta[i].GetContentMetaKey().type);
        if (recordType == NcmContentMetaType_Application || recordType == NcmContentMetaType_Patch) {
            const u32 installedVersion = m_contentMeta[i].GetContentMetaKey().version;
            nsextPushLaunchVersion(baseTitleId, installedVersion);
            if (R_SUCCEEDED(avmInitialize())) {
                avmPushLaunchVersion(baseTitleId, installedVersion);
                AvmVersionListImporter importer;
                if (R_SUCCEEDED(avmGetVersionListImporter(&importer))) {
                    const u64 patchId = baseTitleId | 0x800ULL;
                    AvmVersionListEntry entries[2] = {
                        { .application_id = baseTitleId, .version = installedVersion, .required = installedVersion },
                        { .application_id = patchId,     .version = installedVersion, .required = installedVersion },
                    };
                    avmVersionListImporterSetTimestamp(&importer, AvmCurrentTimestamp());
                    avmVersionListImporterSetData(&importer, entries, 2);
                    avmVersionListImporterFlush(&importer);
                    avmVersionListImporterClose(&importer);
                }
                avmExit();
            }
        }
    }

    static void DeleteExistingContent(const NcmContentMetaKey& oldKey, NcmStorageId storageId) {
        const auto oldType = static_cast<NcmContentMetaType>(oldKey.type);
        if (oldType == NcmContentMetaType_Patch) {
            const u64 baseId = leaf::util::GetBaseTitleId(oldKey.id, oldType);

            u32 remainingMaxVersion = 0;
            for (NcmStorageId sid : { NcmStorageId_SdCard, NcmStorageId_BuiltInUser }) {
                NcmContentMetaDatabase db = {};
                if (R_FAILED(ncmOpenContentMetaDatabase(&db, sid))) continue;
                constexpr s32 kBatch = 16;
                NcmContentMetaKey keys[kBatch];
                s32 total = 0, got = 0;
                if (R_SUCCEEDED(ncmContentMetaDatabaseList(&db, &total, &got, keys, kBatch,
                        NcmContentMetaType_Unknown, baseId, 0, UINT64_MAX,
                        NcmContentInstallType_Full))) {
                    for (s32 j = 0; j < got; j++) {
                        if (keys[j].id == oldKey.id && keys[j].type == oldKey.type &&
                            keys[j].version == oldKey.version)
                            continue;
                        if (keys[j].version > remainingMaxVersion)
                            remainingMaxVersion = keys[j].version;
                    }
                }
                ncmContentMetaDatabaseClose(&db);
            }

            LOG_DEBUG("DeleteExistingContent: resetting AVM floor for %016lX to %u\n",
                      baseId, remainingMaxVersion);

            if (R_SUCCEEDED(avmInitialize())) {
                AvmVersionListImporter importer;
                if (R_SUCCEEDED(avmGetVersionListImporter(&importer))) {
                    const u64 patchId = baseId | 0x800ULL;
                    AvmVersionListEntry entries[2] = {
                        { .application_id = baseId,   .version = remainingMaxVersion, .required = remainingMaxVersion },
                        { .application_id = patchId,  .version = remainingMaxVersion, .required = remainingMaxVersion },
                    };
                    avmVersionListImporterSetTimestamp(&importer, AvmCurrentTimestamp());
                    avmVersionListImporterSetData(&importer, entries, 2);
                    avmVersionListImporterFlush(&importer);
                    avmVersionListImporterClose(&importer);
                }
                avmExit();
            }
            nsextPushLaunchVersion(baseId, remainingMaxVersion);
        }

        NcmContentMetaDatabase db = {};
        if (R_FAILED(ncmOpenContentMetaDatabase(&db, storageId))) return;

        NcmContentStorage cs = {};
        const bool hasStorage = R_SUCCEEDED(ncmOpenContentStorage(&cs, storageId));

        s32 total = 0;
        s32 offset = 0;
        constexpr s32 kBatch = 32;
        NcmContentInfo infos[kBatch];
        while (true) {
            s32 got = 0;
            if (R_FAILED(ncmContentMetaDatabaseListContentInfo(&db, &got, infos, kBatch, &oldKey, offset)))
                break;
            if (got == 0) break;
            if (hasStorage) {
                for (s32 j = 0; j < got; j++) {
                    bool has = false;
                    if (R_SUCCEEDED(ncmContentStorageHas(&cs, &has, &infos[j].content_id)) && has)
                        ncmContentStorageDelete(&cs, &infos[j].content_id);
                }
            }
            offset += got;
            total  += got;
            if (got < kBatch) break;
        }

        if (hasStorage) ncmContentStorageClose(&cs);

        NcmContentId cnmtId = {};
        if (R_SUCCEEDED(ncmContentMetaDatabaseGetContentIdByType(&db, &cnmtId, &oldKey, NcmContentType_Meta))) {
            if (hasStorage) {
                NcmContentStorage cs2 = {};
                if (R_SUCCEEDED(ncmOpenContentStorage(&cs2, storageId))) {
                    bool has = false;
                    if (R_SUCCEEDED(ncmContentStorageHas(&cs2, &has, &cnmtId)) && has)
                        ncmContentStorageDelete(&cs2, &cnmtId);
                    ncmContentStorageClose(&cs2);
                }
            }
        }

        ncmContentMetaDatabaseRemove(&db, &oldKey);
        ncmContentMetaDatabaseCommit(&db);
        ncmContentMetaDatabaseClose(&db);

        LOG_DEBUG("DeleteExistingContent: removed %d NCAs for title %016lX v%u\n",
                  total, oldKey.id, oldKey.version);
    }

    static u64 GetInstalledContentSize(const NcmContentMetaKey& key, NcmStorageId storageId)
    {
        NcmContentMetaDatabase db = {};
        if (R_FAILED(ncmOpenContentMetaDatabase(&db, storageId))) return 0;

        NcmContentStorage cs = {};
        const bool hasStorage = R_SUCCEEDED(ncmOpenContentStorage(&cs, storageId));

        u64 sizeBytes = 0;
        s32 offset = 0;
        constexpr s32 kBatch = 32;
        NcmContentInfo infos[kBatch];
        while (true) {
            s32 got = 0;
            if (R_FAILED(ncmContentMetaDatabaseListContentInfo(&db, &got, infos, kBatch, &key, offset)))
                break;
            if (got == 0) break;
            for (s32 j = 0; j < got; j++) {
                bool has = !hasStorage;
                if (hasStorage) ncmContentStorageHas(&cs, &has, &infos[j].content_id);
                if (has) {
                    u64 sz = 0;
                    ncmContentInfoSizeToU64(&infos[j], &sz);
                    sizeBytes += sz;
                }
            }
            offset += got;
            if (got < kBatch) break;
        }

        if (hasStorage) ncmContentStorageClose(&cs);
        ncmContentMetaDatabaseClose(&db);
        return sizeBytes;
    }

    void Install::Prepare()
    {
        leaf::data::ByteBuffer cnmtBuf;

        try {
            { NcmContentStorage cs = {};
              if (R_SUCCEEDED(ncmOpenContentStorage(&cs, m_destStorageId))) {
                  ncmContentStorageCleanupAllPlaceHolder(&cs);
                  ncmContentStorageClose(&cs);
              }
            }

            inst::diag::NoteStep("Prepare phase: reading CNMT records");
            std::vector<std::tuple<nx::ncm::ContentMeta, NcmContentInfo>> tupelList = this->ReadCNMT();
            inst::diag::NoteStep("Prepare phase: discovered " + std::to_string(tupelList.size()) + " CNMT record(s)");

            struct ExistingContentInfo {
                bool foundExisting = false;
                NcmContentMetaKey existingKey = {};
                NcmStorageId existingSid = NcmStorageId_None;
            };
            std::vector<ExistingContentInfo> existingInfoList(tupelList.size());

            u64 requiredBytes = 0;
            u64 reclaimableBytes = 0;
            for (size_t i = 0; i < tupelList.size(); i++) {
                auto& tuple = tupelList[i];

                // CNMT NCA size
                u64 cnmtSz = 0;
                ncmContentInfoSizeToU64(&std::get<1>(tuple), &cnmtSz);
                requiredBytes += cnmtSz;
                for (auto& info : std::get<0>(tuple).GetContentInfos()) {
                    u64 sz = 0;
                    ncmContentInfoSizeToU64(&info, &sz);
                    requiredBytes += sz;
                }

                const nx::ncm::ContentMeta& meta = std::get<0>(tuple);
                const NcmContentMetaKey metaKey = const_cast<nx::ncm::ContentMeta&>(meta).GetContentMetaKey();
                const auto metaType = static_cast<NcmContentMetaType>(metaKey.type);

                for (NcmStorageId sid : { NcmStorageId_SdCard, NcmStorageId_BuiltInUser }) {
                    NcmContentMetaDatabase db = {};
                    if (R_FAILED(ncmOpenContentMetaDatabase(&db, sid))) continue;
                    NcmContentMetaKey key = {};
                    if (R_SUCCEEDED(ncmContentMetaDatabaseGetLatestContentMetaKey(&db, &key, metaKey.id))
                        && static_cast<NcmContentMetaType>(key.type) == metaType) {
                        existingInfoList[i].existingKey = key;
                        existingInfoList[i].existingSid = sid;
                        existingInfoList[i].foundExisting = true;
                    }
                    ncmContentMetaDatabaseClose(&db);
                    if (existingInfoList[i].foundExisting) break;
                }

                if (existingInfoList[i].foundExisting && existingInfoList[i].existingSid == m_destStorageId) {
                    reclaimableBytes += GetInstalledContentSize(existingInfoList[i].existingKey, existingInfoList[i].existingSid);
                }
            }

            const u64 netRequiredBytes = (requiredBytes > reclaimableBytes) ? (requiredBytes - reclaimableBytes) : 0;

            if (netRequiredBytes > 0) {
                NcmContentStorage cs = {};
                s64 freeBytes = 0;
                if (R_SUCCEEDED(ncmOpenContentStorage(&cs, m_destStorageId))) {
                    ncmContentStorageGetFreeSpaceSize(&cs, &freeBytes);
                    ncmContentStorageClose(&cs);
                }

                if (static_cast<u64>(freeBytes) < netRequiredBytes) {
                    const u64 requiredMB = (netRequiredBytes + 1024 * 1024 - 1) / (1024 * 1024);
                    const u64 freeMB     = static_cast<u64>(freeBytes > 0 ? freeBytes : 0) / (1024 * 1024);
                    const char* dest     = (m_destStorageId == NcmStorageId_SdCard) ? "SD card" : "internal storage";
                    THROW_FORMAT(
                        "Not enough free space on %s.\n"
                        "Required: ~%lu MB  |  Available: ~%lu MB\n"
                        "Free up space and try again.",
                        dest, (unsigned long)requiredMB, (unsigned long)freeMB);
                }

                LOG_DEBUG("Free space check passed: need %lu MB net (%lu MB total, %lu MB reclaimed from existing content), have %ld MB on storage %u\n",
                    (unsigned long)(netRequiredBytes / (1024 * 1024)),
                    (unsigned long)(requiredBytes / (1024 * 1024)),
                    (unsigned long)(reclaimableBytes / (1024 * 1024)),
                    (long)(freeBytes / (1024 * 1024)),
                    (unsigned)m_destStorageId);
            }
            
            for (size_t i = 0; i < tupelList.size(); i++) {
                if (inst::ui::instPage::isInstallCancelRequested())
                    THROW_FORMAT("Installation canceled.");

                {
                    const nx::ncm::ContentMeta& meta = std::get<0>(tupelList[i]);
                    const NcmContentMetaKey metaKey = const_cast<nx::ncm::ContentMeta&>(meta).GetContentMetaKey();
                    const auto metaType = static_cast<NcmContentMetaType>(metaKey.type);
                    {
                        const bool foundExisting            = existingInfoList[i].foundExisting;
                        const NcmContentMetaKey existingKey = existingInfoList[i].existingKey;
                        const NcmStorageId existingSid       = existingInfoList[i].existingSid;

                        if (foundExisting) {
                            if (existingKey.version == metaKey.version) {
                                if (!inst::config::autoSkipReinstall && !this->m_skipReinstallCheck) {
                                    if (this->m_suppressReinstallPrompt) {
                                        this->reinstallDetected = true;
                                        return;
                                    }
                                    const int choice = inst::ui::mainApp->CreateShowDialog(
                                        "inst.already_installed.title"_lang,
                                        "inst.already_installed.same_version"_lang,
                                        {"inst.already_installed.reinstall"_lang, "common.cancel"_lang}, false);
                                    if (choice != 0)
                                        THROW_FORMAT("Already installed at the same version.");
                                }
                                inst::diag::NoteStep("Prepare: removing same version before reinstall");
                                DeleteExistingContent(existingKey, existingSid);
                            } else if (metaType == NcmContentMetaType_Patch
                                    || metaType == NcmContentMetaType_AddOnContent) {
                                if (metaKey.version < existingKey.version && !this->m_skipDowngradeCheck) {
                                    this->downgradeDetected    = true;
                                    this->downgradeFromVersion = existingKey.version;
                                    this->downgradeToVersion   = metaKey.version;
                                    return;
                                }
                                inst::diag::NoteStep("Prepare: removing old version " +
                                    std::to_string(existingKey.version) + " before installing " +
                                    std::to_string(metaKey.version));
                                DeleteExistingContent(existingKey, existingSid);
                            } else {
                                if (!inst::config::autoSkipReinstall && !this->m_skipReinstallCheck) {
                                    if (this->m_suppressReinstallPrompt) {
                                        this->reinstallDetected = true;
                                        return;
                                    }
                                    const int choice = inst::ui::mainApp->CreateShowDialog(
                                        "inst.already_installed.title"_lang,
                                        "inst.already_installed.diff_version"_lang,
                                        {"inst.already_installed.reinstall"_lang, "common.cancel"_lang}, false);
                                    if (choice != 0)
                                        THROW_FORMAT("Already installed at the same version.");
                                }
                                DeleteExistingContent(existingKey, existingSid);
                            }
                        }
                    }
                }

                std::tuple<nx::ncm::ContentMeta, NcmContentInfo> cnmtTuple = tupelList[i];
                
                m_contentMeta.push_back(std::get<0>(cnmtTuple));
                NcmContentInfo cnmtContentRecord = std::get<1>(cnmtTuple);

                nx::ncm::ContentStorage contentStorage(m_destStorageId);

                if (!contentStorage.Has(cnmtContentRecord.content_id))
                {
                    LOG_DEBUG("Installing CNMT NCA...\n");
                    inst::diag::NoteStep("Prepare phase: installing CNMT NCA " + leaf::util::GetNcaIdString(cnmtContentRecord.content_id));
                    this->InstallNCA(cnmtContentRecord.content_id);
                    TrackSessionInstalledNca(cnmtContentRecord.content_id);
                }
                else
                {
                    LOG_DEBUG("CNMT NCA already installed. Proceeding...\n");
                    inst::diag::NoteStep("Prepare phase: CNMT already present " + leaf::util::GetNcaIdString(cnmtContentRecord.content_id));
                }

                if (m_ignoreReqFirmVersion)
                    LOG_DEBUG("WARNING: Required system firmware version is being IGNORED!\n");

                leaf::data::ByteBuffer installContentMetaBuf;
                m_contentMeta[i].GetInstallContentMeta(installContentMetaBuf, cnmtContentRecord, m_ignoreReqFirmVersion);
                inst::diag::NoteStep("Prepare phase: writing content meta record #" + std::to_string(i + 1));

                this->InstallContentMetaRecords(installContentMetaBuf, i);
            }
        }
        catch (...) {
            CleanupSessionInstalledNcas();
            throw;
        }
    }

    void Install::Begin()
    {
        LOG_DEBUG("Installing ticket and cert...\n");
        inst::diag::NoteStep("Install phase: ticket/cert import");
        try
        {
            this->InstallTicketCert();
        }
        catch (std::runtime_error& e)
        {
            LOG_DEBUG("WARNING: Ticket installation failed: %s\n", e.what());
            throw;
        }

        std::vector<u64> pushedRecordIds;

        try {
            for (nx::ncm::ContentMeta contentMeta: m_contentMeta) {
                LOG_DEBUG("Installing NCAs...\n");
                for (auto& record : contentMeta.GetContentInfos())
                {
                    if (inst::ui::instPage::isInstallCancelRequested())
                        THROW_FORMAT("Installation canceled.");
                    nx::ncm::ContentStorage contentStorage(m_destStorageId);
                    if (contentStorage.Has(record.content_id)) {
                        LOG_DEBUG("NCA already installed. Skipping %s\n", leaf::util::GetNcaIdString(record.content_id).c_str());
                        inst::diag::NoteStep("Install phase: skip existing NCA " + leaf::util::GetNcaIdString(record.content_id));
                        continue;
                    }

                    LOG_DEBUG("Installing from %s\n", leaf::util::GetNcaIdString(record.content_id).c_str());
                    inst::diag::NoteStep("Install phase: writing NCA " + leaf::util::GetNcaIdString(record.content_id));
                    this->InstallNCA(record.content_id);
                    TrackSessionInstalledNca(record.content_id);
                }
            }

            for (int i = 0; i < static_cast<int>(m_contentMeta.size()); i++) {
                inst::diag::NoteStep("Install phase: pushing application record #" + std::to_string(i + 1));
                this->InstallApplicationRecord(i);
                const u64 baseTitleId = leaf::util::GetBaseTitleId(this->GetTitleId(i), this->GetContentMetaType(i));
                pushedRecordIds.push_back(baseTitleId);
            }
        }
        catch (...) {
            for (const u64 id : pushedRecordIds) {
                LOG_DEBUG("Rolling back application record for %016lX\n", id);
                nsDeleteApplicationRecord(id);
            }
            CleanupSessionInstalledNcas();
            throw;
        }
    }

    u64 Install::GetTitleId(int i)
    {
        return m_contentMeta[i].GetContentMetaKey().id;
    }

    NcmContentMetaType Install::GetContentMetaType(int i)
    {
        return static_cast<NcmContentMetaType>(m_contentMeta[i].GetContentMetaKey().type);
    }
}
