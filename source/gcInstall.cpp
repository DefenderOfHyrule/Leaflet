#include "gcInstall.hpp"
#include "gc_direct_install.hpp"
#include "util/error.hpp"
#include "util/config.hpp"
#include "util/install_diagnostics.hpp"
#include "util/util.hpp"
#include "util/lang.hpp"
#include "util/title_util.hpp"
#include "ui/MainApplication.hpp"
#include "ui/instPage.hpp"

#include <cstring>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <switch.h>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <memory>

namespace inst::ui {
    extern MainApplication *mainApp;
}

namespace inst::gc {

    static FsDeviceOperator s_deviceOp = {};
    static FsGameCardHandle s_gcHandle = {};
    static bool s_initialized = false;
    static bool s_mounted = false;
    static std::string s_mountPath;

    static constexpr const char* GC_MOUNT_NAME = "@GcApp";
    static bool s_servicesUp = false;

    bool Init() {
        if (s_initialized) return true;
        Result rc = fsOpenDeviceOperator(&s_deviceOp);
        if (R_FAILED(rc)) return false;
        s_initialized = true;
        return true;
    }

    void Exit() {
        Unmount();
        if (s_initialized) {
            fsDeviceOperatorClose(&s_deviceOp);
            s_initialized = false;
        }
    }

    bool IsInserted() {
        if (!s_initialized) return false;
        bool inserted = false;
        Result rc = fsDeviceOperatorIsGameCardInserted(&s_deviceOp, &inserted);
        if (R_FAILED(rc)) return false;
        return inserted;
    }

    bool Mount() {
        if (s_mounted) return true;
        if (!s_initialized) return false;

        Result rc = fsDeviceOperatorGetGameCardHandle(&s_deviceOp, &s_gcHandle);
        if (R_FAILED(rc)) return false;

        FsFileSystem gcFs = {};
        rc = fsOpenGameCardFileSystem(&gcFs, &s_gcHandle, FsGameCardPartition_Secure);
        if (R_FAILED(rc)) {
            std::memset(&s_gcHandle, 0, sizeof(s_gcHandle));
            return false;
        }

        int ret = fsdevMountDevice(GC_MOUNT_NAME, gcFs);
        if (ret == -1) {
            fsFsClose(&gcFs);
            std::memset(&s_gcHandle, 0, sizeof(s_gcHandle));
            return false;
        }

        s_mountPath = std::string(GC_MOUNT_NAME) + ":/";
        s_mounted = true;
        return true;
    }

    void Unmount() {
        if (!s_mounted) return;
        fsdevUnmountDevice(GC_MOUNT_NAME);
        std::memset(&s_gcHandle, 0, sizeof(s_gcHandle));
        s_mountPath.clear();
        s_mounted = false;
    }

    bool IsMounted() { return s_mounted; }
    std::string GetMountPath() { return s_mountPath; }

    // read NACP + icon from NS for the given title IDs.
    // uses StorageOnly to bypass NS's cache so a fresh gamecard insertion
    // isn't shadowed by a previously digitally installed version.
    // tries the patch ID first (gives the update's display_version), then
    // the base app ID, then falls back to cached data
    static bool TryGetControlData(std::uint64_t appId, std::uint64_t patchId,
                                   NsApplicationControlData& controlData,
                                   std::size_t& controlDataSize) {
        for (u64 queryId : { patchId, appId }) {
            controlDataSize = 0;
            std::memset(&controlData, 0, sizeof(controlData));
            Result rc = nsGetApplicationControlData(
                NsApplicationControlSource_StorageOnly,
                queryId, &controlData, sizeof(NsApplicationControlData), &controlDataSize);
            if (R_SUCCEEDED(rc) && controlDataSize > sizeof(NacpStruct)
                && controlData.nacp.display_version[0] != '\0')
                return true;
        }
        // cached fallback may return digital install data on first insertion
        controlDataSize = 0;
        std::memset(&controlData, 0, sizeof(controlData));
        Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage,
                        appId, &controlData, sizeof(NsApplicationControlData), &controlDataSize);
        return R_SUCCEEDED(rc) && controlDataSize > sizeof(NacpStruct);
    }

    std::vector<GameCardTitle> EnumerateTitles() {
        std::vector<GameCardTitle> result;
        if (!s_mounted) return result;

        bool weInitedServices = false;
        if (!s_servicesUp) {
            inst::util::initInstallServices();
            s_servicesUp = true;
            weInitedServices = true;
        }

        // use gc_direct_install's CNMT parser to get per-type entries
        auto entries = inst::gc::direct::EnumerateContent(s_mountPath);

        if (!entries.empty()) {
            // group by appId (should all share one appId for a normal gamecard)
            uint64_t appId = entries[0].appId;

            GameCardTitle title = {};
            title.titleId = appId;

            for (const auto& entry : entries) {
                title.totalSize += entry.totalNcaSize;
                if (entry.key.type == NcmContentMetaType_Application) {
                    title.hasBase = true;
                    title.baseVersion = entry.key.version;
                    title.keyGeneration = entry.keyGen;
                    title.baseSize = entry.totalNcaSize;
                } else if (entry.key.type == NcmContentMetaType_Patch) {
                    title.hasUpdate = true;
                    title.updateVersion = entry.key.version;
                    title.updateSize = entry.totalNcaSize;
                } else if (entry.key.type == NcmContentMetaType_AddOnContent) {
                    title.dlcCount++;
                    title.dlcSize += entry.totalNcaSize;
                }
            }

            // get name, author and icon from NS control data.
            // use StorageOnly to bypass the cache, and try the patch ID first
            const uint64_t patchId = appId | 0x800ULL;
            NsApplicationControlData controlData = {};
            std::size_t controlDataSize = 0;
            if (TryGetControlData(appId, patchId, controlData, controlDataSize)) {
                std::string name = leaf::util::GetNameFromNacp(&controlData.nacp);
                if (!name.empty()) {
                    title.name = name;
                } else {
                    char idStr[32] = {};
                    std::snprintf(idStr, sizeof(idStr), "%016lX", appId);
                    title.name = idStr;
                }
                title.author = leaf::util::GetAuthorFromNacp(&controlData.nacp);
                if (controlData.nacp.display_version[0] != '\0')
                    title.displayVersion = controlData.nacp.display_version;
                std::size_t iconSize = controlDataSize - sizeof(NacpStruct);
                if (iconSize > 0)
                    title.icon.assign(controlData.icon, controlData.icon + iconSize);
            } else {
                char idStr[32] = {};
                std::snprintf(idStr, sizeof(idStr), "%016lX", appId);
                title.name = std::string("Title ") + idStr;
                title.author = "Unknown";
            }

            result.push_back(title);
        }

        if (weInitedServices) {
            inst::util::deinitInstallServices();
            s_servicesUp = false;
        }

        return result;
    }

    static void RunInstall(int storageChoice, const std::vector<size_t>& selection,
                           const std::string& titleName, u64 requiredBytes,
                           const std::vector<std::uint8_t>& iconData = {}) {
        if (!s_servicesUp) {
            inst::util::initInstallServices();
            s_servicesUp = true;
        }
        inst::ui::instPage::loadInstallScreen();
        bool installed = true;
        NcmStorageId destStorageId = (storageChoice == 0) ? NcmStorageId_SdCard : NcmStorageId_BuiltInUser;

        inst::diag::StartSession("gamecard", 1);

        try {
            std::string currentName = inst::util::shortenString(titleName, 40, true);
            inst::diag::NoteTransferReceived(currentName);
            inst::ui::instPage::setTopInstInfoText("inst.info_page.top_info0"_lang + currentName + " (Gamecard)");
            inst::ui::instPage::setInstInfoText("inst.info_page.preparing"_lang);
            inst::ui::instPage::setInstBarPerc(0);
            inst::diag::NoteInstallStarted(currentName);

            // show icon immediately when install starts before any NCA is written
            if (!iconData.empty())
                inst::ui::instPage::setInstallIconData(iconData.data(), iconData.size());

            if (requiredBytes > 0) {
                NcmContentStorage cs = {};
                s64 freeBytes = 0;
                if (R_SUCCEEDED(ncmOpenContentStorage(&cs, destStorageId))) {
                    ncmContentStorageGetFreeSpaceSize(&cs, &freeBytes);
                    ncmContentStorageClose(&cs);
                }
                if (static_cast<u64>(freeBytes) < requiredBytes) {
                    const u64 reqMB  = (requiredBytes + 1024 * 1024 - 1) / (1024 * 1024);
                    const u64 freeMB = static_cast<u64>(freeBytes > 0 ? freeBytes : 0) / (1024 * 1024);
                    const char* dest = (destStorageId == NcmStorageId_SdCard) ? "SD card" : "internal storage";
                    THROW_FORMAT(
                        "Not enough free space on %s.\n"
                        "Required: ~%lu MB  |  Available: ~%lu MB\n"
                        "Free up space and try again.",
                        dest, (unsigned long)reqMB, (unsigned long)freeMB);
                }
            }

            bool success = inst::gc::direct::InstallSelectedFromGamecard(s_mountPath, destStorageId, selection);
            if (!success)
                THROW_FORMAT("Gamecard install failed, see debug log for details.");

            Unmount();
            inst::diag::RecordSuccess(currentName);
        }
        catch (std::exception& e) {
            LOG_DEBUG("Gamecard install failed: %s\n", e.what());
            const std::string failedName = titleName;
            const auto failure = inst::diag::ClassifyFailure(e.what());
            inst::diag::RecordFailure(failedName, failure);
            inst::ui::instPage::setInstInfoText(
                failure.canceled ? "Installation canceled."
                                 : ("inst.info_page.failed"_lang + failedName));
            inst::ui::instPage::setInstBarPerc(0);
            if (!failure.canceled) {
                std::string audioPath = "romfs:/audio/bark.wav";
                if (!inst::config::soundEnabled) audioPath = "";
                if (std::filesystem::exists(inst::config::appDir + "/bark.wav"))
                    audioPath = inst::config::appDir + "/bark.wav";
                std::thread audioThread(inst::util::playAudio, audioPath);
                inst::ui::mainApp->CreateShowDialog(
                    "inst.info_page.failed"_lang + failedName + "!",
                    inst::diag::BuildUserMessage(failure),
                    {"common.ok"_lang}, true);
                audioThread.join();
            }
            installed = false;
            if (failure.canceled) svcSleepThread(1500000000ULL);
        }
        
        if (installed) {
            inst::ui::instPage::setInstInfoText("inst.info_page.complete"_lang);
            inst::ui::instPage::setInstBarPerc(100);
            std::string audioPath = "romfs:/audio/success.wav";
            if (!inst::config::soundEnabled) audioPath = "";
            if (std::filesystem::exists(inst::config::appDir + "/success.wav"))
                audioPath = inst::config::appDir + "/success.wav";
            std::thread audioThread(inst::util::playAudio, audioPath);
            inst::ui::mainApp->CreateShowDialog(
                titleName + "inst.info_page.desc1"_lang,
                Language::GetRandomMsg(),
                {"common.ok"_lang}, true);
            audioThread.join();
        }

        inst::ui::instPage::loadMainMenu();
        inst::util::deinitInstallServices();
        s_servicesUp = false;
    }

    void InstallFromGamecard(int storageChoice) {
        if (!s_mounted) return;

        auto entries = inst::gc::direct::EnumerateContent(s_mountPath);
        if (entries.empty()) return;

        std::vector<size_t> all;
        u64 requiredBytes = 0;
        for (size_t i = 0; i < entries.size(); i++) {
            all.push_back(i);
            requiredBytes += entries[i].totalNcaSize;
        }

        auto titles = EnumerateTitles();
        std::string titleName = titles.empty() ? "Gamecard" : titles[0].name;
        std::vector<std::uint8_t> icon = titles.empty() ? std::vector<std::uint8_t>{} : titles[0].icon;

        RunInstall(storageChoice, all, titleName, requiredBytes, icon);
    }

    void InstallSelectedFromGamecard(int storageChoice, uint32_t contentMask) {
        if (!s_mounted) return;

        if (!s_servicesUp) {
            inst::util::initInstallServices();
            s_servicesUp = true;
        }

        auto entries = inst::gc::direct::EnumerateContent(s_mountPath);
        if (entries.empty()) {
            inst::util::deinitInstallServices();
            s_servicesUp = false;
            return;
        }

        std::vector<size_t> selection;
        u64 requiredBytes = 0;
        for (size_t i = 0; i < entries.size(); i++) {
            const auto& e = entries[i];
            if (e.key.type == NcmContentMetaType_Application && (contentMask & 1)) { selection.push_back(i); requiredBytes += e.totalNcaSize; }
            else if (e.key.type == NcmContentMetaType_Patch        && (contentMask & 2)) { selection.push_back(i); requiredBytes += e.totalNcaSize; }
            else if (e.key.type == NcmContentMetaType_AddOnContent && (contentMask & 4)) { selection.push_back(i); requiredBytes += e.totalNcaSize; }
        }
        if (selection.empty()) {
            inst::util::deinitInstallServices();
            s_servicesUp = false;
            return;
        }

        auto titles = EnumerateTitles();
        std::string titleName = titles.empty() ? "Gamecard" : titles[0].name;
        std::vector<std::uint8_t> icon = titles.empty() ? std::vector<std::uint8_t>{} : titles[0].icon;

        RunInstall(storageChoice, selection, titleName, requiredBytes, icon);
    }
}
