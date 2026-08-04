#include <filesystem>
#include "ui/MainApplication.hpp"
#include "ui/mainPage.hpp"
#include "ui/hddInstPage.hpp"
#include "hddInstall.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/lang.hpp"
#include "ui/bottomHint.hpp"

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {
    extern MainApplication *mainApp;

    namespace {
        std::filesystem::path normalizeRootPath(const std::filesystem::path& path) {
            std::filesystem::path normalized = path;
            const auto pathStr = normalized.string();
            if (!pathStr.empty() && pathStr.back() == ':') {
                normalized /= "";
            }
            return normalized;
        }
    }

    hddInstPage::hddInstPage() : Layout::Layout() {
        if (inst::config::oledMode) {
            this->SetBackgroundColor(COLOR("#000000FF"));
        } else {
            this->SetBackgroundColor(COLOR(inst::config::colorBackground));
            if (std::filesystem::exists(inst::config::appDir + "/background.png")) this->SetBackgroundImage(inst::config::appDir + "/background.png");
            else this->SetBackgroundImage("romfs:/images/background.jpg");
        }
        const auto infoColor = inst::config::oledMode ? COLOR("#000000FF") : COLOR(inst::config::colorBotBar);
        const auto botColor = inst::config::oledMode ? COLOR("#000000FF") : COLOR(inst::config::colorBotBar);
        this->infoRect = Rectangle::New(0, 75, 1280, 60, infoColor);
        this->botRect = Rectangle::New(0, 660, 1280, 60, botColor);
        this->pageInfoText = TextBlock::New(10, 89, "inst.hdd.top_info"_lang, 30);
        this->pageInfoText->SetColor(COLOR("#FFFFFFFF"));
        this->butText = TextBlock::New(10, 678, "inst.hdd.buttons"_lang, 20);
        this->butText->SetColor(COLOR("#FFFFFFFF"));
        this->bottomHintSegments = BuildBottomHintSegments("inst.hdd.buttons"_lang, 10, 20);
        this->menu = pu::ui::elm::Menu::New(0, 136, 1280, COLOR("#FFFFFF00"), 50, 10);
        if (inst::config::oledMode) {
            this->menu->SetOnFocusColor(COLOR("#FFFFFF33"));
            this->menu->SetScrollbarColor(COLOR("#FFFFFF66"));
        } else {
            this->menu->SetOnFocusColor(COLOR(inst::config::colorTileHighlight));
            this->menu->SetScrollbarColor(COLOR(inst::config::colorBotBar));
        }
        this->statusBar = StatusBar::New(StatusBar::Mode::Full, "main.menu.hdd"_lang); this->statusBar->Attach(this);
        this->Add(this->infoRect);
        this->Add(this->botRect);
        this->Add(this->butText);
        this->Add(this->pageInfoText);
        this->Add(this->menu);
    }

    void hddInstPage::drawMenuItems(bool clearItems, std::filesystem::path ourPath) {
        if (clearItems) this->selectedTitles = {};
        const auto normalizedPath = normalizeRootPath(ourPath);
        const auto normalizedParent = normalizeRootPath(normalizedPath.parent_path());
        const bool isRootPath = (normalizedParent == normalizedPath);
        if (this->rootDir.empty() || (clearItems && isRootPath)) {
            this->rootDir = normalizedPath;
        }
        this->currentDir = normalizedPath;
        this->menu->ClearItems();
        try {
            this->ourDirectories = util::getDirsAtPath(this->currentDir);
            this->ourFiles = util::getDirectoryFiles(this->currentDir, {".nsp", ".nsz", ".xci", ".xcz"});
        } catch (std::exception& e) {
            this->drawMenuItems(false, this->currentDir.parent_path());
            return;
        }
        if (this->currentDir != this->rootDir) {
            std::string itm = "..";
            auto ourEntry = pu::ui::elm::MenuItem::New(itm);
            ourEntry->SetColor(COLOR("#FFFFFFFF"));
            ourEntry->SetIcon("romfs:/images/icons/folder-upload.png");
            this->menu->AddItem(ourEntry);
        }
        for (auto& file: this->ourDirectories) {
            if (file == "..") break;
            std::string itm = file.filename().string();
            auto ourEntry = pu::ui::elm::MenuItem::New(itm);
            ourEntry->SetColor(COLOR("#FFFFFFFF"));
            ourEntry->SetIcon("romfs:/images/icons/folder.png");
            this->menu->AddItem(ourEntry);
        }
        for (auto& file: this->ourFiles) {
            std::string itm = file.filename().string();
            auto ourEntry = pu::ui::elm::MenuItem::New(itm);
            ourEntry->SetColor(COLOR("#FFFFFFFF"));
            ourEntry->SetIcon("romfs:/images/icons/checkbox-blank-outline.png");
            for (long unsigned int i = 0; i < this->selectedTitles.size(); i++) {
                if (this->selectedTitles[i] == file) {
                    ourEntry->SetIcon("romfs:/images/icons/check-box-outline.png");
                }
            }
            this->menu->AddItem(ourEntry);
        }
    }

    void hddInstPage::followDirectory() {
        int selectedIndex = this->menu->GetSelectedIndex();
        int dirListSize = this->ourDirectories.size();
        if (this->currentDir != this->rootDir) {
            dirListSize++;
            selectedIndex--;
        }
        if (selectedIndex < dirListSize) {
            if (this->menu->GetItems()[this->menu->GetSelectedIndex()]->GetName() == ".." && this->menu->GetSelectedIndex() == 0) {
                this->drawMenuItems(true, this->currentDir.parent_path());
            } else {
                this->drawMenuItems(true, this->ourDirectories[selectedIndex]);
            }
            this->menu->SetSelectedIndex(0);
        }
    }

    void hddInstPage::selectNsp(int selectedIndex, bool redraw) {

        if (selectedIndex < 0 ||
            selectedIndex >= (int)this->menu->GetItems().size()) {
            return;
        }

        const std::string icon =
            this->menu->GetItems()[selectedIndex]->GetIcon();

        if (icon != "romfs:/images/icons/check-box-outline.png" &&
            icon != "romfs:/images/icons/checkbox-blank-outline.png") {

            this->followDirectory();
            return;
        }

        int dirListSize = this->ourDirectories.size();

        if (this->currentDir != this->rootDir)
            dirListSize++;

        int fileIndex = selectedIndex - dirListSize;

        if (fileIndex < 0 ||
            fileIndex >= (int)this->ourFiles.size()) {
            return;
        }

        const auto &selectedFile = this->ourFiles[fileIndex];

        if (icon == "romfs:/images/icons/check-box-outline.png") {

            for (size_t i = 0; i < this->selectedTitles.size(); i++) {

                if (this->selectedTitles[i] == selectedFile) {
                    this->selectedTitles.erase(
                        this->selectedTitles.begin() + i);
                    break;
                }
            }
        }
        else {

            this->selectedTitles.push_back(selectedFile);
        }

        if (redraw) {
            this->drawMenuItems(false, currentDir);
        }
    }

    void hddInstPage::startInstall() {
        int dialogResult = -1;
        if (this->selectedTitles.size() == 1) {
            dialogResult = mainApp->CreateShowDialog("inst.target.desc0"_lang + inst::util::shortenString(std::filesystem::path(this->selectedTitles[0]).filename().string(), 32, true) + "inst.target.desc1"_lang, "common.cancel_desc"_lang, {"inst.target.opt0"_lang, "inst.target.opt1"_lang}, false);
        } else dialogResult = mainApp->CreateShowDialog("inst.target.desc00"_lang + std::to_string(this->selectedTitles.size()) + "inst.target.desc01"_lang, "common.cancel_desc"_lang, {"inst.target.opt0"_lang, "inst.target.opt1"_lang}, false);
        if (dialogResult == -1) return;
        hddInstStuff::installNspFromFile(this->selectedTitles, dialogResult);
        this->selectedTitles.clear();

        this->drawMenuItems(true, this->currentDir);

        if (this->menu->GetItems().size() > 0) {
            this->menu->SetSelectedIndex(0);
        }
    }

    void hddInstPage::onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos) {
        (void)Held;
        int bottomTapX = 0;
        if (DetectBottomHintTap(Pos, this->bottomHintTouch, 668, 52, bottomTapX)) {
            Down |= FindBottomHintButton(this->bottomHintSegments, bottomTapX);
        }
        inst::util::playNavigationClickIfNeeded(Down);

        if (!Pos.IsEmpty()) {
            if (!this->touchTapActive) {
                this->touchTapActive = true;
                this->touchTapMoved = false;
                this->touchTapStartX = Pos.X;
                this->touchTapStartY = Pos.Y;
            } else {
                int dx = Pos.X - this->touchTapStartX;
                int dy = Pos.Y - this->touchTapStartY;
                if (dx < 0) dx = -dx;
                if (dy < 0) dy = -dy;
                if (dx > 12 || dy > 12) this->touchTapMoved = true;
            }
        } else if (this->touchTapActive) {
            if (!this->touchTapMoved) {
                const int tappedIndex = this->menu->GetSelectedIndex();
                const auto now = std::chrono::steady_clock::now();
                bool doubleTap = false;
                if (this->hasLastTap && (this->lastTapIndex == tappedIndex)) {
                    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->lastTapTime).count();
                    if (ms <= 350) doubleTap = true;
                }

                if (doubleTap) {
                    this->selectNsp(tappedIndex, true);
                    if (this->ourFiles.size() == 1 && this->selectedTitles.size() == 1) {
                        this->startInstall();
                    }
                    this->hasLastTap = false;
                    this->lastTapIndex = -1;
                } else {
                    this->hasLastTap = true;
                    this->lastTapIndex = tappedIndex;
                    this->lastTapTime = now;
                }
            }
            this->touchTapActive = false;
            this->touchTapMoved = false;
        }

        if (Down & HidNpadButton_B) {
            mainApp->LoadLayout(mainApp->mainPage);
        }
        if (Down & HidNpadButton_A) {
            this->selectNsp(this->menu->GetSelectedIndex(), true);
            if (this->ourFiles.size() == 1 && this->selectedTitles.size() == 1) {
                this->startInstall();
            }
            this->hasLastTap = false;
            this->lastTapIndex = -1;
        }
        if ((Down & HidNpadButton_Y)) {

            if (this->selectedTitles.size() == this->ourFiles.size()) {

                this->drawMenuItems(true, currentDir);
            }
            else {

                int topDir = 0;

                if (this->currentDir != this->rootDir)
                    topDir++;

                const int startIndex =
                    (int)this->ourDirectories.size() + topDir;

                const int endIndex =
                    (int)this->menu->GetItems().size();

                for (int i = startIndex; i < endIndex; i++) {

                    if (i < 0 || i >= (int)this->menu->GetItems().size())
                        continue;

                    if (this->menu->GetItems()[i]->GetIcon()
                        == "romfs:/images/icons/check-box-outline.png") {
                        continue;
                    }

                    this->selectNsp(i, false);
                }

                this->drawMenuItems(false, currentDir);
            }
        }
        
        if ((Down & HidNpadButton_X)) {
            inst::ui::mainApp->CreateShowDialog("inst.hdd.help.title"_lang, "inst.hdd.help.desc"_lang, {"common.ok"_lang}, true);
        }
        if (Down & HidNpadButton_ZL) {
            int selectedIndex = this->menu->GetSelectedIndex();
            int dirOffset = (this->currentDir != this->rootDir) ? 1 : 0;
            int dirListSize = (int)this->ourDirectories.size() + dirOffset;
            bool isFolder = selectedIndex < dirListSize && !(this->menu->GetItems()[selectedIndex]->GetName() == "..");
            if (isFolder) {
                std::string folderName = this->ourDirectories[selectedIndex - dirOffset].filename().string();
                std::string folderPath = this->ourDirectories[selectedIndex - dirOffset].string();
                std::string confirmDesc = inst::util::formatString("inst.archivebit.confirm_desc"_lang, folderName);
                int confirmResult = inst::ui::mainApp->CreateShowDialog("inst.archivebit.confirm_title"_lang, confirmDesc, {"common.ok"_lang, "common.cancel"_lang}, true);
                if (confirmResult == 0) {
                    if (inst::util::setArchiveBit(folderPath)) {
                        this->drawMenuItems(false, this->currentDir);
                        inst::ui::mainApp->CreateShowDialog("inst.archivebit.title"_lang, "inst.archivebit.desc"_lang, {"common.ok"_lang}, true);
                    } else {
                        inst::ui::mainApp->CreateShowDialog("inst.archivebit.title"_lang, "inst.archivebit.error_desc"_lang, {"common.ok"_lang}, true);
                    }
                }
            }
        }
        if (Down & HidNpadButton_Plus) {
            if (this->selectedTitles.size() == 0 && this->menu->GetItems()[this->menu->GetSelectedIndex()]->GetIcon() == "romfs:/images/icons/checkbox-blank-outline.png") {
                this->selectNsp(this->menu->GetSelectedIndex());
            }
            if (this->selectedTitles.size() > 0) this->startInstall();
        }
    }
}

