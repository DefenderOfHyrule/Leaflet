#include "util/avm_maintenance.hpp"
#include "nx/ipc/ns_ext.h"
#include <algorithm>

namespace inst::util {

    std::uint64_t GetPatchIdForApp(std::uint64_t baseAppId) {
        return baseAppId | 0x800ULL;
    }

    std::uint32_t GetInstalledContentVersion(std::uint64_t titleId, NcmContentMetaType type) {
        std::uint32_t best = 0;
        for (NcmStorageId sid : { NcmStorageId_SdCard, NcmStorageId_BuiltInUser }) {
            NcmContentMetaDatabase db = {};
            if (R_FAILED(ncmOpenContentMetaDatabase(&db, sid))) continue;
            NcmContentMetaKey key = {};
            if (R_SUCCEEDED(ncmContentMetaDatabaseGetLatestContentMetaKey(&db, &key, titleId)) && key.type == type)
                if (key.version > best) best = key.version;
            ncmContentMetaDatabaseClose(&db);
        }
        return best;
    }

    static std::uint64_t AvmMaintenanceTimestamp() {
        std::uint64_t ts = 0;
        if (R_FAILED(timeGetCurrentTime(TimeType_UserSystemClock, &ts)))
            ts = 4102444800ULL;
        return ts;
    }

    void SetAvmLaunchFloor(std::uint64_t baseAppId, std::uint32_t version) {
        const std::uint64_t patchId = GetPatchIdForApp(baseAppId);

        nsextPushLaunchVersion(baseAppId, version);

        Result avmRc = avmInitialize();
        if (R_SUCCEEDED(avmRc)) {
            avmPushLaunchVersion(baseAppId, version);
            AvmVersionListImporter importer;
            if (R_SUCCEEDED(avmGetVersionListImporter(&importer))) {
                AvmVersionListEntry entries[2] = {
                    { .application_id = baseAppId, .version = version, .required = version },
                    { .application_id = patchId,   .version = version, .required = version },
                };
                avmVersionListImporterSetTimestamp(&importer, AvmMaintenanceTimestamp());
                avmVersionListImporterSetData(&importer, entries, 2);
                avmVersionListImporterFlush(&importer);
                avmVersionListImporterClose(&importer);
            }
            avmExit();
        }
    }

    void FixAvmFloorForTitle(std::uint64_t baseAppId) {
        const std::uint32_t maxVersion = std::max(
            GetInstalledContentVersion(baseAppId, NcmContentMetaType_Application),
            GetInstalledContentVersion(GetPatchIdForApp(baseAppId), NcmContentMetaType_Patch));
        SetAvmLaunchFloor(baseAppId, maxVersion);
    }

    std::vector<std::uint64_t> ListDigitallyInstalledAppIds() {
        std::vector<std::uint64_t> result;

        for (NcmStorageId sid : { NcmStorageId_SdCard, NcmStorageId_BuiltInUser }) {
            NcmContentMetaDatabase db = {};
            if (R_FAILED(ncmOpenContentMetaDatabase(&db, sid))) continue;

            NcmContentMetaKey probeKey = {};
            s32 total = 0, written = 0;
            Result rc = ncmContentMetaDatabaseList(&db, &total, &written, &probeKey, 1,
                NcmContentMetaType_Application, 0, 0, UINT64_MAX, NcmContentInstallType_Full);
            if (R_FAILED(rc) || total <= 0) {
                ncmContentMetaDatabaseClose(&db);
                continue;
            }

            std::vector<NcmContentMetaKey> keys(static_cast<std::size_t>(total));
            rc = ncmContentMetaDatabaseList(&db, &total, &written, keys.data(), total,
                NcmContentMetaType_Application, 0, 0, UINT64_MAX, NcmContentInstallType_Full);
            ncmContentMetaDatabaseClose(&db);
            if (R_FAILED(rc)) continue;

            for (s32 i = 0; i < written; i++) {
                std::uint64_t id = keys[i].id;
                if (std::find(result.begin(), result.end(), id) == result.end())
                    result.push_back(id);
            }
        }

        return result;
    }

    int FixAllAvmFloors() {
        auto ids = ListDigitallyInstalledAppIds();
        for (std::uint64_t id : ids)
            FixAvmFloorForTitle(id);
        return static_cast<int>(ids.size());
    }

}
