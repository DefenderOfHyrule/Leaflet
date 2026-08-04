#include <filesystem>
#include <algorithm>
#include <fstream>
#include <chrono>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include "ui/MainApplication.hpp"
#include "ui/mainPage.hpp"
#include "ui/fileBrowserPage.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/lang.hpp"
#include "ui/bottomHint.hpp"
#include "nx/usbhdd.h"

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {

    constexpr size_t kCopyBufSize = 1024 * 1024;

    static bool isActualDirectory(const std::filesystem::path& p) {
        DIR* dp = opendir(p.c_str());
        if (dp) { closedir(dp); return true; }
        return false;
    }

    static uint64_t getPathSizeDir(const std::filesystem::path& p) {
        uint64_t total = 0;
        std::error_code ec;
        DIR* dp = opendir(p.c_str());
        if (!dp) return std::filesystem::file_size(p, ec);
        while (true) {
            struct dirent* dt = readdir(dp);
            if (!dt) break;
            const std::string name = dt->d_name;
            if (name == "." || name == "..") continue;
            const std::filesystem::path child = p / name;
            if (dt->d_type & DT_DIR) {
                total += getPathSizeDir(child);
            } else {
                total += std::filesystem::file_size(child, ec);
            }
        }
        closedir(dp);
        return total;
    }

    static uint64_t getPathSize(const std::filesystem::path& p) {
        std::error_code ec;
        if (isActualDirectory(p)) return getPathSizeDir(p);
        return std::filesystem::file_size(p, ec);
    }
    extern MainApplication *mainApp;

    using ProgressCallback = std::function<void(size_t)>;

    static std::string g_copyError;

    static bool copyFileProgress(const std::filesystem::path& src, const std::filesystem::path& dst, ProgressCallback prog_cb) {
        static uint8_t buf[kCopyBufSize];
        g_copyError.clear();
        std::FILE* in  = std::fopen(src.c_str(), "rb");
        if (!in) {
            if (errno == EIO || errno == EISDIR) {
                g_copyError = src.filename().string() + " cannot be copied (it may be a split archive folder with the archive bit set, these cannot be opened as raw files).";
            } else {
                g_copyError = std::string("Cannot open src: ") + src.string() + " (" + strerror(errno) + ")";
            }
            return false;
        }
        std::FILE* out = std::fopen(dst.c_str(), "wb");
        if (!out) { std::fclose(in); g_copyError = std::string("Cannot open dst: ") + dst.string() + " (" + strerror(errno) + ")"; return false; }
        bool ok = true;
        while (!std::feof(in)) {
            size_t r = std::fread(buf, 1, kCopyBufSize, in);
            if (r == 0) break;
            size_t w = std::fwrite(buf, 1, r, out);
            if (w != r) { ok = false; break; }
            if (prog_cb) prog_cb(r);
        }
        std::fclose(out);
        std::fclose(in);
        if (!ok) std::remove(dst.c_str());
        return ok;
    }

    static bool copyDirectoryProgress(const std::filesystem::path& src, const std::filesystem::path& dst, ProgressCallback prog_cb) {
        DIR* dp = opendir(src.c_str());
        if (!dp) return false;
        std::error_code ec;
        std::filesystem::create_directories(dst, ec);
        bool ok = true;
        while (true) {
            struct dirent* dt = readdir(dp);
            if (!dt) break;
            const std::string name = dt->d_name;
            if (name == "." || name == "..") continue;
            const std::filesystem::path srcEntry = src / name;
            const std::filesystem::path dstEntry = dst / name;
            if (dt->d_type & DT_DIR) {
                if (!copyDirectoryProgress(srcEntry, dstEntry, prog_cb)) ok = false;
            } else {
                if (!copyFileProgress(srcEntry, dstEntry, prog_cb)) ok = false;
            }
        }
        closedir(dp);
        return ok;
    }

    fileBrowserPage::fileBrowserPage() : Layout::Layout() {
        if (inst::config::oledMode) {
            this->SetBackgroundColor(COLOR("#000000FF"));
        } else {
            this->SetBackgroundColor(COLOR(inst::config::colorBackground));
            if (std::filesystem::exists(inst::config::appDir + "/background.png"))
                this->SetBackgroundImage(inst::config::appDir + "/background.png");
            else
                this->SetBackgroundImage("romfs:/images/background.jpg");
        }
        const auto botColor  = inst::config::oledMode ? COLOR("#000000FF") : COLOR(inst::config::colorBotBar);
        this->botRect   = pu::ui::elm::Rectangle::New(0, 660, 1280, 60, botColor);
        this->pageInfoText = pu::ui::elm::TextBlock::New(10, 61, "inst.browser.top_info"_lang, 30);
        this->pageInfoText->SetColor(COLOR("#FFFFFFFF"));
        this->butText = pu::ui::elm::TextBlock::New(10, 678, "inst.browser.buttons"_lang, 20);
        this->butText->SetColor(COLOR("#FFFFFFFF"));
        this->bottomHintSegments = BuildBottomHintSegments("inst.browser.buttons"_lang, 10, 20);
        this->menu = pu::ui::elm::Menu::New(0, 108, 1280, COLOR("#FFFFFF00"), 50, 11);
        if (inst::config::oledMode) {
            this->menu->SetOnFocusColor(COLOR("#FFFFFF33"));
            this->menu->SetScrollbarColor(COLOR("#FFFFFF66"));
        } else {
            this->menu->SetOnFocusColor(COLOR("#00000033"));
            this->menu->SetScrollbarColor(COLOR("#17090980"));
        }
        this->statusBar = StatusBar::New(StatusBar::Mode::Slim, "main.menu.browser"_lang); this->statusBar->Attach(this);
        this->Add(this->botRect);
        this->Add(this->butText);
        this->Add(this->pageInfoText);
        this->Add(this->menu);

        constexpr int kOverlayX = 790;
        constexpr int kOverlayY = 548;
        constexpr int kOverlayW = 480;
        constexpr int kOverlayH = 108;
        constexpr int kBarX  = kOverlayX + 16;
        constexpr int kBarY  = kOverlayY + 66;
        constexpr int kBarW  = kOverlayW - 32;
        constexpr int kBarH  = 28;
        constexpr int kTxtX  = kOverlayX + 16;
        constexpr int kTxtY  = kOverlayY + 12;
        const auto tileColor = inst::config::oledMode ? COLOR("#1A1A1ACC") : COLOR(inst::config::colorTopBar);
        this->copyOverlayBg = pu::ui::elm::Rectangle::New(kOverlayX, kOverlayY, kOverlayW, kOverlayH, tileColor, 10);
        this->copyOverlayText = pu::ui::elm::TextBlock::New(kTxtX, kTxtY, "", 18);
        this->copyOverlayText->SetColor(COLOR("#FFFFFFFF"));
        this->copyOverlayBar = pu::ui::elm::ProgressBar::New(kBarX, kBarY, kBarW, kBarH, 100.0f);
        this->copyOverlayBar->SetColor(COLOR("#FFFFFF22"));
        this->copyOverlayBar->SetProgressColor(COLOR("#4CD964FF"));
        this->copyOverlayBg->SetVisible(false);
        this->copyOverlayText->SetVisible(false);
        this->copyOverlayBar->SetVisible(false);
        this->Add(this->copyOverlayBg);
        this->Add(this->copyOverlayText);
        this->Add(this->copyOverlayBar);
    }

    void fileBrowserPage::openDriveMenu() {
        this->menu->ClearItems();
        this->currentDir.clear();
        this->rootDir.clear();
        this->ourDirectories.clear();
        this->ourFiles.clear();

        auto sdEntry = pu::ui::elm::MenuItem::New("inst.browser.drive.sd"_lang);
        sdEntry->SetColor(COLOR("#FFFFFFFF"));
        sdEntry->SetIcon("romfs:/images/icons/micro-sd.png");
        this->menu->AddItem(sdEntry);

        u32 driveCount = nx::hdd::count();
        for (u32 i = 0; i < driveCount; i++) {
            const char* path = nx::hdd::rootPath(i);
            if (!path) continue;
            const std::string drivePath = std::string(path) + "/";
            auto entry = pu::ui::elm::MenuItem::New(std::string("USB Drive ") + std::to_string(i + 1) + " [" + drivePath + "]");
            entry->SetColor(COLOR("#FFFFFFFF"));
            entry->SetIcon("romfs:/images/icons/usb-install.png");
            this->menu->AddItem(entry);
        }
        this->pageInfoText->SetText("inst.browser.top_info"_lang);
    }

    void fileBrowserPage::drawMenuItems(std::filesystem::path path) {
        const std::string pathStr = path.string();
        if (!pathStr.empty() && pathStr.back() == ':')
            path = std::filesystem::path(pathStr + "/");
        this->currentDir = path;
        if (this->rootDir.empty()) this->rootDir = path;
        this->menu->ClearItems();
        this->ourDirectories.clear();
        this->ourFiles.clear();
        {
            const std::string dirStr = this->currentDir.string();
            DIR* dp = opendir(dirStr.c_str());
            if (!dp) {
                this->navigateUp();
                return;
            }
            while (true) {
                struct dirent* dt = readdir(dp);
                if (!dt) break;
                const std::string name = dt->d_name;
                if (name == "." || name == "..") continue;
                const std::string childStr = dirStr + (dirStr.back() == '/' ? "" : "/") + name;
                const std::filesystem::path child = std::filesystem::path(childStr);
                if (dt->d_type & DT_DIR) {
                    this->ourDirectories.push_back(child);
                } else {
                    this->ourFiles.push_back(child);
                }
            }
            closedir(dp);
            std::sort(this->ourDirectories.begin(), this->ourDirectories.end());
            std::sort(this->ourFiles.begin(), this->ourFiles.end());
        }
        if (this->currentDir != this->rootDir) {
            auto up = pu::ui::elm::MenuItem::New("..");
            up->SetColor(COLOR("#FFFFFFFF"));
            up->SetIcon("romfs:/images/icons/folder-upload.png");
            this->menu->AddItem(up);
        }
        for (auto& dir : this->ourDirectories) {
            auto e = pu::ui::elm::MenuItem::New(dir.filename().string());
            e->SetColor(COLOR("#FFFFFFFF"));
            e->SetIcon("romfs:/images/icons/folder.png");
            this->menu->AddItem(e);
        }
        for (auto& file : this->ourFiles) {
            auto e = pu::ui::elm::MenuItem::New(file.filename().string());
            e->SetColor(COLOR("#FFFFFFFF"));
            e->SetIcon("romfs:/images/icons/checkbox-blank-outline.png");
            this->menu->AddItem(e);
        }
        this->pageInfoText->SetText(this->currentDir.string());
    }

    void fileBrowserPage::navigateUp() {
        if (this->currentDir.empty()) {
            mainApp->LoadLayout(mainApp->mainPage);
        } else if (this->currentDir == this->rootDir) {
            this->openDriveMenu();
            this->menu->SetSelectedIndex(0);
        } else {
            this->drawMenuItems(this->currentDir.parent_path());
            this->menu->SetSelectedIndex(0);
        }
    }

    bool fileBrowserPage::isDirectoryEntry(int index) const {
        const int dirOffset = (this->currentDir != this->rootDir && !this->currentDir.empty()) ? 1 : 0;
        return index < ((int)this->ourDirectories.size() + dirOffset);
    }

    int fileBrowserPage::directoryIndexFor(int index) const {
        const int dirOffset = (this->currentDir != this->rootDir && !this->currentDir.empty()) ? 1 : 0;
        return index - dirOffset;
    }

    std::filesystem::path fileBrowserPage::selectedPath(int index) const {
        if (this->currentDir.empty()) return {};
        if (this->isDirectoryEntry(index)) {
            int di = this->directoryIndexFor(index);
            if (di < 0 || di >= (int)this->ourDirectories.size()) return {};
            return this->ourDirectories[di];
        } else {
            const int dirOffset = (this->currentDir != this->rootDir && !this->currentDir.empty()) ? 1 : 0;
            int fi = index - (int)this->ourDirectories.size() - dirOffset;
            if (fi < 0 || fi >= (int)this->ourFiles.size()) return {};
            return this->ourFiles[fi];
        }
    }

    void fileBrowserPage::tryArchiveBit(int index) {
        if (this->currentDir.empty()) return;
        if (!this->isDirectoryEntry(index)) return;
        if (this->menu->GetItems()[index]->GetName() == "..") return;
        int dirIdx = this->directoryIndexFor(index);
        if (dirIdx < 0 || dirIdx >= (int)this->ourDirectories.size()) return;
        const std::string folderName = this->ourDirectories[dirIdx].filename().string();
        const std::string folderPath = this->ourDirectories[dirIdx].string();
        const std::string confirmDesc = inst::util::formatString("inst.archivebit.confirm_desc"_lang, folderName);
        int result = mainApp->CreateShowDialog("inst.archivebit.confirm_title"_lang, confirmDesc, {"common.ok"_lang, "common.cancel"_lang}, true);
        if (result == 0) {
            if (inst::util::setArchiveBit(folderPath)) {
                const int archivePrevIdx = this->menu->GetSelectedIndex();
                this->drawMenuItems(this->currentDir);
                const int archiveNewCount = static_cast<int>(this->menu->GetItems().size());
                this->menu->SetSelectedIndex(archiveNewCount > 0 ? std::min(archivePrevIdx, archiveNewCount - 1) : 0);
                mainApp->CreateShowDialog("inst.archivebit.title"_lang, "inst.archivebit.desc"_lang, {"common.ok"_lang}, true);
            } else {
                mainApp->CreateShowDialog("inst.archivebit.title"_lang, "inst.archivebit.error_desc"_lang, {"common.ok"_lang}, true);
            }
        }
    }

    void fileBrowserPage::tryCopy(int index) {
        if (this->currentDir.empty()) return;
        if (this->menu->GetItems()[index]->GetName() == "..") return;
        const std::filesystem::path src = this->selectedPath(index);
        if (src.empty()) return;
        const std::string name = src.filename().string();
        int result = mainApp->CreateShowDialog(
            "inst.browser.copy.title"_lang,
            inst::util::formatString("inst.browser.copy.desc"_lang, name),
            {"common.ok"_lang, "common.cancel"_lang}, true);
        if (result == 0) {
            this->clipboardPath = src;
            this->clipboardHasItem = true;
        }
    }

    void fileBrowserPage::showCopyOverlay(bool visible) {
        this->copyOverlayBg->SetVisible(visible);
        this->copyOverlayText->SetVisible(visible);
        this->copyOverlayBar->SetVisible(visible);
        if (!visible) this->pageInfoText->SetText(this->currentDir.string());
    }

    void fileBrowserPage::updateCopyOverlay(const std::string& text, double progress) {
        this->copyOverlayText->SetText(text);
        this->copyOverlayBar->SetProgress(progress);
    }

    void fileBrowserPage::tryPaste() {
        if (!this->clipboardHasItem || this->currentDir.empty()) return;
        const std::string srcName = this->clipboardPath.filename().string();
        const std::filesystem::path dest = this->currentDir / srcName;
        const std::filesystem::path srcPath = this->clipboardPath;
        if (dest == srcPath) {
            mainApp->CreateShowDialog("inst.browser.paste.error_title"_lang, "inst.browser.paste.error_same"_lang, {"common.ok"_lang}, true);
            return;
        }
        if (std::filesystem::exists(dest)) {
            int overwrite = mainApp->CreateShowDialog(
                "inst.browser.paste.overwrite_title"_lang,
                inst::util::formatString("inst.browser.paste.overwrite_desc"_lang, srcName),
                {"common.ok"_lang, "common.cancel"_lang}, true);
            if (overwrite != 0) return;
        }
        int result = mainApp->CreateShowDialog(
            "inst.browser.paste.confirm_title"_lang,
            inst::util::formatString("inst.browser.paste.confirm_desc"_lang, srcName),
            {"common.ok"_lang, "common.cancel"_lang}, true);
        if (result != 0) return;

        {
            const std::string destStr = dest.string();
            const auto colonPos = destStr.find(':');
            const std::string destMount = (colonPos != std::string::npos) ? destStr.substr(0, colonPos) : "";
            constexpr uint64_t fat32Limit = 0xFFFFFFFFULL;
            const uint64_t fileSize = getPathSize(srcPath);
            bool isFat32 = (destMount == "sdmc");
            if (!isFat32 && !destMount.empty()) {
                const u8 fsType = nx::hdd::getFsType(destMount);
                isFat32 = (fsType == 3);
            }
            if (isFat32 && fileSize > fat32Limit) {
                mainApp->CreateShowDialog(
                    "inst.browser.copy.fat32_title"_lang,
                    inst::util::formatString("inst.browser.copy.fat32_desc"_lang, srcName),
                    {"common.ok"_lang}, true);
                return;
            }
        }

        const uint64_t totalBytes = getPathSize(srcPath);
        uint64_t bytesDone = 0;
        auto lastTp = std::chrono::steady_clock::now();

        this->showCopyOverlay(true);
        this->copyOverlayBar->SetProgress(0.0f);
        this->pageInfoText->SetText("Copying " + srcName + "...");
        mainApp->CallForRender();

        auto prog_cb = [&](size_t chunkSize) {
            bytesDone += chunkSize;
            const auto now = std::chrono::steady_clock::now();
            const double ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTp).count();
            lastTp = now;
            char line1[128];
            char line2[64];
            const double doneMB = static_cast<double>(bytesDone) / (1024.0 * 1024.0);
            double pct = 0.0;
            if (totalBytes > 0) {
                const double totalMB = static_cast<double>(totalBytes) / (1024.0 * 1024.0);
                pct = 100.0 * static_cast<double>(bytesDone) / static_cast<double>(totalBytes);
                std::snprintf(line1, sizeof(line1), "%.1f / %.1f MB", doneMB, totalMB);
            } else {
                std::snprintf(line1, sizeof(line1), "%.1f MB", doneMB);
            }
            if (ms > 0.0) {
                const double speedMBs = (static_cast<double>(chunkSize) / (1024.0 * 1024.0)) / (ms / 1000.0);
                std::snprintf(line2, sizeof(line2), "%.1f MB/s", speedMBs);
            } else {
                line2[0] = 0;
            }
            const std::string overlayText = std::string(line1) + (line2[0] ? std::string("  ") + line2 : "");
            this->updateCopyOverlay(overlayText, pct);
            mainApp->CallForRender();
        };

        bool copyOk = false;
        std::error_code ec;
        if (isActualDirectory(srcPath)) {
            copyOk = copyDirectoryProgress(srcPath, dest, prog_cb);
        } else {
            copyOk = copyFileProgress(srcPath, dest, prog_cb);
        }

        this->showCopyOverlay(false);
        const int pasteItemCount = static_cast<int>(this->menu->GetItems().size());
        const int pastePrevIndex = this->menu->GetSelectedIndex();
        if (copyOk) {
            this->clipboardHasItem = false;
            this->clipboardPath.clear();
            this->drawMenuItems(this->currentDir);
            const int pasteNewCount = static_cast<int>(this->menu->GetItems().size());
            this->menu->SetSelectedIndex(pasteNewCount > 0 ? std::min(pastePrevIndex, pasteNewCount - 1) : 0);
            mainApp->CreateShowDialog("inst.browser.paste.done_title"_lang, inst::util::formatString("inst.browser.paste.done_desc"_lang, srcName), {"common.ok"_lang}, true);
        } else {
            this->drawMenuItems(this->currentDir);
            this->menu->SetSelectedIndex(pasteItemCount > 0 ? std::min(pastePrevIndex, pasteItemCount - 1) : 0);
            const std::string failMsg = g_copyError.empty() ? std::string("inst.browser.paste.fail_desc"_lang) : g_copyError;
            mainApp->CreateShowDialog("inst.browser.paste.fail_title"_lang, failMsg, {"common.ok"_lang}, true);
        }
    }

    static bool deleteDirectoryRecursive(const std::string& path) {
        DIR* dp = opendir(path.c_str());
        if (!dp) return false;
        bool ok = true;
        while (true) {
            struct dirent* dt = readdir(dp);
            if (!dt) break;
            const std::string name = dt->d_name;
            if (name == "." || name == "..") continue;
            const std::string child = path + "/" + name;
            if (dt->d_type & DT_DIR) {
                if (!deleteDirectoryRecursive(child)) ok = false;
            } else {
                std::remove(child.c_str());
            }
        }
        closedir(dp);
        rmdir(path.c_str());
        return ok;
    }

    bool fileBrowserPage::deleteWithProgress(const std::filesystem::path& target) {
        const std::string name = target.filename().string();
        std::error_code ec;
        const std::string targetStr = target.string();
        const auto colonPos = targetStr.find(':');
        const std::string mountName = (colonPos != std::string::npos) ? targetStr.substr(0, colonPos) : "";

        if (!isActualDirectory(target)) {
            this->pageInfoText->SetText("Deleting " + name + "...");
            mainApp->CallForRender();
            const bool ok = (std::remove(target.c_str()) == 0);
            if (!mountName.empty()) fsdevCommitDevice(mountName.c_str());
            return ok;
        }
        std::vector<std::string> files;
        std::vector<std::string> dirs;
        {
            std::vector<std::string> pending;
            pending.push_back(target.string());
            while (!pending.empty()) {
                const std::string path = std::move(pending.back());
                pending.pop_back();
                dirs.push_back(path);
                DIR* dp = opendir(path.c_str());
                if (!dp) continue;
                while (true) {
                    struct dirent* dt = readdir(dp);
                    if (!dt) break;
                    const std::string n = dt->d_name;
                    if (n == "." || n == "..") continue;
                    const std::string child = path + "/" + n;
                    if (dt->d_type & DT_DIR) {
                        pending.push_back(child);
                    } else {
                        files.push_back(child);
                    }
                }
                closedir(dp);
            }
        }
        const size_t total = files.size();
        size_t done = 0;
        for (auto& f : files) {
            std::remove(f.c_str());
            done++;
            if (total > 0) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "Deleting %s: %zu / %zu files", name.c_str(), done, total);
                this->pageInfoText->SetText(buf);
            }
            mainApp->CallForRender();
        }
        for (auto it = dirs.rbegin(); it != dirs.rend(); ++it)
            rmdir(it->c_str());
        if (!mountName.empty()) fsdevCommitDevice(mountName.c_str());
        this->pageInfoText->SetText(this->currentDir.string());
        return true;
    }

    void fileBrowserPage::tryDelete(int index) {
        if (this->currentDir.empty()) return;
        if (this->menu->GetItems()[index]->GetName() == "..") return;
        const std::filesystem::path target = this->selectedPath(index);
        if (target.empty()) return;
        const std::string name = target.filename().string();
        int result = mainApp->CreateShowDialog(
            "inst.browser.delete.confirm_title"_lang,
            inst::util::formatString("inst.browser.delete.confirm_desc"_lang, name),
            {"common.ok"_lang, "common.cancel"_lang}, true);
        if (result != 0) return;
        const int prevIndex = this->menu->GetSelectedIndex();
        const bool deleteOk = this->deleteWithProgress(target);
        if (deleteOk) {
            this->drawMenuItems(this->currentDir);
            const int itemCount = static_cast<int>(this->menu->GetItems().size());
            const int newIndex = (itemCount == 0) ? 0 : std::min(prevIndex, itemCount - 1);
            this->menu->SetSelectedIndex(newIndex);
            mainApp->CreateShowDialog("inst.browser.delete.done_title"_lang, inst::util::formatString("inst.browser.delete.done_desc"_lang, name), {"common.ok"_lang}, true);
        } else {
            mainApp->CreateShowDialog("inst.browser.delete.fail_title"_lang, "inst.browser.delete.fail_desc"_lang, {"common.ok"_lang}, true);
        }
    }

    void fileBrowserPage::onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos) {
        (void)Up; (void)Held;
        int bottomTapX = 0;
        if (DetectBottomHintTap(Pos, this->bottomHintTouch, 668, 52, bottomTapX))
            Down |= FindBottomHintButton(this->bottomHintSegments, bottomTapX);
        inst::util::playNavigationClickIfNeeded(Down);

        if (Down & HidNpadButton_B) {
            this->navigateUp();
            return;
        }

        const int idx = this->menu->GetSelectedIndex();

        if (Down & HidNpadButton_A) {
            if (this->currentDir.empty()) {
                if (idx == 0) {
                    this->rootDir.clear();
                    this->drawMenuItems("sdmc:/");
                } else {
                    u32 driveCount = nx::hdd::count();
                    int driveIdx = idx - 1;
                    if (driveIdx >= 0 && (u32)driveIdx < driveCount) {
                        const char* path = nx::hdd::rootPath(static_cast<u32>(driveIdx));
                        if (path) {
                            this->rootDir.clear();
                            std::string drivePath = std::string(path);
                            if (!drivePath.empty() && drivePath.back() != '/') drivePath += "/";
                            this->drawMenuItems(std::filesystem::path(drivePath));
                        }
                    }
                }
                this->menu->SetSelectedIndex(0);
                return;
            }
            if (this->menu->GetItems()[idx]->GetName() == "..") {
                this->navigateUp();
                return;
            }
            if (this->isDirectoryEntry(idx)) {
                int dirIdx = this->directoryIndexFor(idx);
                if (dirIdx >= 0 && dirIdx < (int)this->ourDirectories.size()) {
                    this->drawMenuItems(this->ourDirectories[dirIdx]);
                    this->menu->SetSelectedIndex(0);
                }
            }
            return;
        }

        if (Down & HidNpadButton_ZL) {
            this->tryArchiveBit(idx);
            return;
        }

        if (Down & HidNpadButton_R) {
            this->tryCopy(idx);
            return;
        }

        if (Down & HidNpadButton_ZR) {
            this->tryPaste();
            return;
        }

        if (Down & HidNpadButton_Y) {
            this->tryDelete(idx);
            return;
        }

        if (Down & HidNpadButton_X) {
            mainApp->CreateShowDialog("inst.browser.help.title"_lang, "inst.browser.help.desc"_lang, {"common.ok"_lang}, true);
            return;
        }
    }
}
