#include <filesystem>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <thread>
#include <chrono>

#include "ui/MainApplication.hpp"
#include "ui/gcInstPage.hpp"
#include "gcInstall.hpp"
#include "gc_direct_install.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/lang.hpp"
#include "ui/instPage.hpp"
#include "ui/bottomHint.hpp"

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {
    extern MainApplication *mainApp;

    static constexpr uint32_t MASK_BASE   = 1;
    static constexpr uint32_t MASK_UPDATE = 2;
    static constexpr uint32_t MASK_DLC    = 4;

    static std::string FormatSize(std::uint64_t bytes) {
        char buf[64] = {};
        double gib = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
        if (gib >= 1.0)
            std::snprintf(buf, sizeof(buf), "%.2f GiB", gib);
        else
            std::snprintf(buf, sizeof(buf), "%.2f MiB",
                          static_cast<double>(bytes) / (1024.0 * 1024.0));
        return buf;
    }

    // build the label for a toggle item, prefixed with a check/uncheck glyph.
    static std::string ToggleLabel(const std::string& label, bool selected) {
        // \uE14B = filled checkbox, \uE14C = empty checkbox (Nintendo glyphs)
        return (selected ? "\uE14B  " : "\uE14C  ") + label;
    }

    gcInstPage::gcInstPage() : Layout::Layout() {
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

        this->botRect  = Rectangle::New(0, 660, 1280,  60, botColor);




        this->butText = TextBlock::New(10, 678, "", 20);
        this->butText->SetColor(COLOR("#FFFFFFFF"));

        // left panel gamecard info
        this->gcInfoPanel = Rectangle::New(30, 90, 400, 550,
            inst::config::oledMode ? COLOR("#1A1A1ACC") : COLOR(inst::config::colorTopBar), 12);

        this->gcIcon = Image::New(102, 110, "romfs:/images/icons/gamecard_placeholder.png");
        this->gcIcon->SetWidth(256);
        this->gcIcon->SetHeight(256);

        this->gcTitleText   = TextBlock::New(50, 380, "inst.gc.no_card_text"_lang, 22);
        this->gcTitleText->SetColor(COLOR("#FFFFFFFF"));
        this->gcAuthorText  = TextBlock::New(50, 410, "Insert a gamecard to view its contents", 18);
        this->gcAuthorText->SetColor(COLOR("#CCCCCCFF"));
        this->gcAppIdText   = TextBlock::New(50, 440, "", 16);
        this->gcAppIdText->SetColor(COLOR("#AAAAAAFF"));
        this->gcVersionText = TextBlock::New(50, 460, "", 16);
        this->gcVersionText->SetColor(COLOR("#AAAAAAFF"));
        this->gcSizeText    = TextBlock::New(50, 480, "", 16);
        this->gcSizeText->SetColor(COLOR("#AAAAAAFF"));
        this->gcDlcText     = TextBlock::New(50, 500, "", 16);
        this->gcDlcText->SetColor(COLOR("#AAAAAAFF"));
        this->gcStatusText  = TextBlock::New(50, 525, "inst.gc.status.no_card"_lang, 16);
        this->gcStatusText->SetColor(COLOR("#FF6666FF"));

        // right panel menu (shared between main view and selection view)
        this->menu = pu::ui::elm::Menu::New(460, 90, 790, COLOR("#FFFFFF00"), 60, 9);
        if (inst::config::oledMode) {
            this->menu->SetOnFocusColor(COLOR("#FFFFFF33"));
            this->menu->SetScrollbarColor(COLOR("#FFFFFF66"));
        } else {
            this->menu->SetOnFocusColor(COLOR("#00000033"));
            this->menu->SetScrollbarColor(COLOR("#17090980"));
        }

        this->statusBar = StatusBar::New(StatusBar::Mode::Full, "Gamecard Reader"); this->statusBar->Attach(this);
        this->Add(this->botRect);
        this->Add(this->butText);
        this->Add(this->gcInfoPanel);
        this->Add(this->gcIcon);
        this->Add(this->gcTitleText);
        this->Add(this->gcAuthorText);
        this->Add(this->gcAppIdText);
        this->Add(this->gcVersionText);
        this->Add(this->gcSizeText);
        this->Add(this->gcDlcText);
        this->Add(this->gcStatusText);
        this->Add(this->menu);
        this->buildMainMenu();

        this->AddThread([this]() { this->updateGamecardState(); });
    }

    // gamecard polling
    void gcInstPage::updateGamecardState() {
        static u64 lastPollTick = 0;
        const u64 now  = armGetSystemTick();
        const u64 freq = armGetSystemTickFreq();
        if (lastPollTick != 0 && (now - lastPollTick) < (freq / 2)) return;
        lastPollTick = now;

        // don't poll while in the selection sub menu avoids a spurious
        // removal event from briefly disturbing the card during install prep.
        if (this->inSelectionMenu) return;

        bool inserted = inst::gc::IsInserted();

        if (inserted != this->gcInserted) {
            if (inserted) {
                this->gcInserted = true;
                this->gcStatusText->SetText("inst.gc.status.detecting"_lang);
                this->gcStatusText->SetColor(COLOR("#FFCC00FF"));
                if (inst::gc::Mount()) {
                    this->gcMounted = true;
                    this->refreshGamecardInfo();
                    this->gcStatusText->SetText("inst.gc.status.ready"_lang);
                    this->gcStatusText->SetColor(COLOR("#66FF66FF"));
                    this->versionRefreshTicksRemaining = 6;
                } else {
                    this->gcStatusText->SetText("inst.gc.status.failed"_lang);
                    this->gcStatusText->SetColor(COLOR("#FF6666FF"));
                }
            } else {
                this->gcInserted = false;
                this->gcMounted  = false;
                this->versionRefreshTicksRemaining = 0;
                inst::gc::Unmount();
                this->clearGamecardInfo();
                this->gcStatusText->SetText("inst.gc.status.no_card"_lang);
                this->gcStatusText->SetColor(COLOR("#FF6666FF"));
            }
        } else if (this->gcMounted && this->versionRefreshTicksRemaining > 0) {
            this->versionRefreshTicksRemaining--;
            auto newTitles = inst::gc::EnumerateTitles();
            if (!newTitles.empty() && !this->gcTitles.empty()) {
                const auto& fresh   = newTitles[0];
                const auto& current = this->gcTitles[0];
                if (!fresh.displayVersion.empty() &&
                    fresh.displayVersion != current.displayVersion) {
                    this->gcTitles = newTitles;
                    std::string verStr;
                    if (fresh.hasBase) verStr += "Base 1.0.0";
                    if (fresh.hasUpdate) {
                        if (!verStr.empty()) verStr += "   \uE0B5   ";
                        verStr += "Update " + fresh.displayVersion;
                    }
                    this->gcVersionText->SetText(verStr);
                    this->buildMainMenu();
                    this->versionRefreshTicksRemaining = 0;
                }
            }
        }
    }

    // main menu (install + selection entry)
    static constexpr int MAIN_IDX_SD         = 0;
    static constexpr int MAIN_IDX_NAND       = 1;
    static constexpr int MAIN_IDX_SELECT     = 2;

    void gcInstPage::buildMainMenu() {
        this->inSelectionMenu = false;
        this->menu->ClearItems();

        const std::string hint =
            "\uE0E0 Install to SD    \uE0E2 Install to NAND    \uE0E3 Select content    \uE0E1 Back";
        this->butText->SetText(hint);
        this->bottomHintSegments = BuildBottomHintSegments(hint, 10, 20);

        if (!this->gcMounted || this->gcTitles.empty()) return;

        std::string selSummary;
        {
            std::vector<std::string> parts;
            if (this->contentMask & MASK_BASE)   parts.push_back("Base");
            if (this->contentMask & MASK_UPDATE)  parts.push_back("Update");
            if (this->contentMask & MASK_DLC)     parts.push_back("DLC");
            for (size_t i = 0; i < parts.size(); i++) {
                if (i) selSummary += " + ";
                selSummary += parts[i];
            }
            if (selSummary.empty()) selSummary = "inst.gc.nothing_selected_label"_lang;
        }

        {
            auto item = pu::ui::elm::MenuItem::New("inst.gc.install_sd"_lang);
            item->SetColor(COLOR("#FFFFFFFF"));
            item->SetIcon("romfs:/images/icons/micro-sd.png");
            this->menu->AddItem(item);
        }
        {
            auto item = pu::ui::elm::MenuItem::New("inst.gc.install_nand"_lang);
            item->SetColor(COLOR("#FFFFFFFF"));
            item->SetIcon("romfs:/images/icons/install_internal_storage.png");
            this->menu->AddItem(item);
        }
        {
            auto item = pu::ui::elm::MenuItem::New("Select content to install...   [" + selSummary + "]");
            item->SetColor(COLOR("#FFFFFFFF"));
            this->menu->AddItem(item);
        }
        if (!this->menu->GetItems().empty())
            this->menu->SetSelectedIndex(0);
    }

    // selection sub menu (toggle list)
    void gcInstPage::buildSelectionMenu() {
        if (this->gcTitles.empty()) return;

        this->inSelectionMenu = true;
        this->menu->ClearItems();
        this->selectionItemMasks.clear();

        const std::string hint =
            "\uE0E0 Toggle    \uE0E3 Select all / None    \uE0E1 Back";
        this->butText->SetText(hint);
        this->bottomHintSegments = BuildBottomHintSegments(hint, 10, 20);

        const auto& title = this->gcTitles[0];

        if (title.hasBase) {
            auto item = pu::ui::elm::MenuItem::New(
                ToggleLabel("Base game  1.0.0", this->contentMask & MASK_BASE));
            item->SetColor(COLOR("#FFFFFFFF"));
            this->menu->AddItem(item);
            this->selectionItemMasks.push_back(MASK_BASE);
        }
        if (title.hasUpdate) {
            std::string label = "Update data";
            if (!title.displayVersion.empty()) label += "  " + title.displayVersion;
            auto item = pu::ui::elm::MenuItem::New(
                ToggleLabel(label, this->contentMask & MASK_UPDATE));
            item->SetColor(COLOR("#FFFFFFFF"));
            this->menu->AddItem(item);
            this->selectionItemMasks.push_back(MASK_UPDATE);
        }
        if (title.dlcCount > 0) {
            auto item = pu::ui::elm::MenuItem::New(
                ToggleLabel("DLC  (" + std::to_string(title.dlcCount) + " pack(s))",
                            this->contentMask & MASK_DLC));
            item->SetColor(COLOR("#FFFFFFFF"));
            this->menu->AddItem(item);
            this->selectionItemMasks.push_back(MASK_DLC);
        }
        this->menu->SetSelectedIndex(0);
    }

    void gcInstPage::refreshSelectionItemLabel(int /*idx*/) {
        this->buildSelectionMenu();
    }

    // info panel
    void gcInstPage::refreshGamecardInfo() {
        this->gcTitles = inst::gc::EnumerateTitles();
        this->selectionItemMasks.clear();

        if (this->gcTitles.empty()) {
            this->gcTitleText->SetText("Gamecard detected");
            this->gcAuthorText->SetText("Could not read title information");
            this->gcAppIdText->SetText("");
            this->gcVersionText->SetText("");
            this->gcSizeText->SetText("");
            this->gcDlcText->SetText("");
            this->menu->ClearItems();
            auto item = pu::ui::elm::MenuItem::New("inst.gc.no_content"_lang);
            item->SetColor(COLOR("#AAAAAAFF"));
            this->menu->AddItem(item);
            return;
        }

        const auto& title = this->gcTitles[0];

        this->gcTitleText->SetText(title.name);
        this->gcAuthorText->SetText(title.author);

        if (title.titleId != 0) {
            char idBuf[32] = {};
            std::snprintf(idBuf, sizeof(idBuf), "App ID: %016lX", title.titleId);
            this->gcAppIdText->SetText(idBuf);
        } else {
            this->gcAppIdText->SetText("");
        }

        {
            std::string verStr;
            if (title.hasBase)
                verStr += "Base 1.0.0";
            if (title.hasUpdate) {
                if (!verStr.empty()) verStr += "   \uE0B5   ";
                verStr += "Update";
                if (!title.displayVersion.empty()) verStr += " " + title.displayVersion;
            }
            this->gcVersionText->SetText(verStr);
        }

        this->gcSizeText->SetText("Size: " + FormatSize(title.totalSize));
        this->gcDlcText->SetText(title.dlcCount > 0
            ? "DLC: " + std::to_string(title.dlcCount) + " pack(s)"
            : "");

        if (!title.icon.empty()) {
            this->gcIcon->SetJpegImage(
                const_cast<void*>(static_cast<const void*>(title.icon.data())),
                static_cast<s32>(title.icon.size()));
            this->gcIcon->SetWidth(256);
            this->gcIcon->SetHeight(256);
        }

        // default selection: everything present on the card
        this->contentMask = 0;
        if (title.hasBase)       this->contentMask |= MASK_BASE;
        if (title.hasUpdate)     this->contentMask |= MASK_UPDATE;
        if (title.dlcCount > 0)  this->contentMask |= MASK_DLC;

        this->buildMainMenu();
    }

    void gcInstPage::clearGamecardInfo() {
        this->gcTitles.clear();
        this->selectionItemMasks.clear();
        this->contentMask = 0;
        this->gcTitleText->SetText("inst.gc.no_card_text"_lang);
        this->gcAuthorText->SetText("Insert a gamecard to view its contents");
        this->gcAppIdText->SetText("");
        this->gcVersionText->SetText("");
        this->gcSizeText->SetText("");
        this->gcDlcText->SetText("");
        this->gcIcon->SetImage("romfs:/images/icons/gamecard_placeholder.png");
        this->gcIcon->SetWidth(256);
        this->gcIcon->SetHeight(256);
        this->buildMainMenu();
    }

    // install
    void gcInstPage::startInstall(int storageChoice) {
        if (!this->gcMounted || this->gcTitles.empty()) {
            mainApp->CreateShowDialog("inst.gc.install_dialog.title"_lang,
                "inst.gc.install_dialog.no_content_desc"_lang,
                {"common.ok"_lang}, true);
            return;
        }

        if (this->contentMask == 0) {
            mainApp->CreateShowDialog("inst.gc.nothing_selected_dialog.title"_lang,
                "inst.gc.nothing_selected_dialog.desc"_lang,
                {"common.ok"_lang}, true);
            return;
        }

        // warn if update data is on the card but not selected.
        // some titles require the update to launch without a "necessary data" popup.
        const auto& title = this->gcTitles[0];
        if (title.hasUpdate && !(this->contentMask & MASK_UPDATE)) {
            int warn = mainApp->CreateShowDialog(
                "inst.gc.update_not_selected.title"_lang,
                "inst.gc.update_not_selected.desc"_lang,
                {"inst.gc.update_not_selected.continue"_lang, "common.cancel"_lang}, false);
            if (warn != 0) return;
        }

        if (!inst::util::checkSigPatches()) {
            mainApp->CreateShowDialog(
                "inst.gc.sigpatches.title"_lang,
                "inst.gc.sigpatches.desc"_lang,
                {"common.ok"_lang, "common.cancel"_lang}, true);
            return;
        }

        const std::string& titleName = this->gcTitles[0].name;
        const std::string destStr = (storageChoice == 0) ? "SD Card" : "System Memory (NAND)";

        std::vector<std::string> parts;
        if (this->contentMask & MASK_BASE)   parts.push_back("base game");
        if (this->contentMask & MASK_UPDATE)  parts.push_back("update data");
        if (this->contentMask & MASK_DLC)     parts.push_back("DLC");
        std::string contentDesc;
        for (size_t i = 0; i < parts.size(); i++) {
            if (i > 0) contentDesc += (i == parts.size() - 1) ? " and " : ", ";
            contentDesc += parts[i];
        }

        int confirm = mainApp->CreateShowDialog(
            "Install from Gamecard",
            "Install " + contentDesc + " from\n" + titleName + "\nto " + destStr + "?",
            {"Install", "common.cancel"_lang}, false);
        if (confirm != 0) return;

        inst::gc::InstallSelectedFromGamecard(storageChoice, this->contentMask);
    }

    void gcInstPage::showGamecardDetails() {
        if (!this->gcMounted || this->gcTitles.empty()) {
            mainApp->CreateShowDialog("inst.gc.details.title"_lang,
                "No gamecard is mounted.", {"common.ok"_lang}, true);
            return;
        }
        const auto& title = this->gcTitles[0];
        std::string versionInfo;
        if (title.hasBase)   versionInfo += "Base:   1.0.0\n";
        if (title.hasUpdate) {
            versionInfo += "Update: ";
            versionInfo += title.displayVersion.empty() ? "(unknown)" : title.displayVersion;
            versionInfo += "\n";
        }
        if (title.dlcCount)  versionInfo += "DLC packs: " + std::to_string(title.dlcCount) + "\n";
        char buf[512] = {};
        std::snprintf(buf, sizeof(buf),
            "Title:  %s\nAuthor: %s\nApp ID: %016lX\n\n%sKey generation: %u\nTotal size: %s",
            title.name.c_str(), title.author.c_str(), title.titleId,
            versionInfo.c_str(),
            static_cast<unsigned>(title.keyGeneration),
            FormatSize(title.totalSize).c_str());
        mainApp->CreateShowDialog("inst.gc.details.title"_lang, buf, {"common.ok"_lang}, true);
    }

    // input handling
    void gcInstPage::onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos) {
        (void)Up; (void)Held;

        int bottomTapX = 0;
        if (DetectBottomHintTap(Pos, this->bottomHintTouch, 668, 52, bottomTapX))
            Down |= FindBottomHintButton(this->bottomHintSegments, bottomTapX);

        inst::util::playNavigationClickIfNeeded(Down);

        // selection sub menu
        if (this->inSelectionMenu) {
            if (Down & HidNpadButton_B) {
                this->buildMainMenu();
                return;
            }
            if (Down & HidNpadButton_A) {
                // toggle the focused item
                int idx = this->menu->GetSelectedIndex();
                if (idx >= 0 && idx < static_cast<int>(this->selectionItemMasks.size())) {
                    this->contentMask ^= this->selectionItemMasks[idx];
                    this->buildSelectionMenu();
                }
                return;
            }
            if (Down & HidNpadButton_Y) {
                const auto& title = this->gcTitles[0];
                uint32_t fullMask = 0;
                if (title.hasBase)      fullMask |= MASK_BASE;
                if (title.hasUpdate)    fullMask |= MASK_UPDATE;
                if (title.dlcCount > 0) fullMask |= MASK_DLC;
                this->contentMask = (this->contentMask == fullMask) ? 0 : fullMask;
                this->buildSelectionMenu();
                return;
            }
            return;
        }

        // main menu
        if (Down & HidNpadButton_B) {
            inst::gc::Unmount();
            this->gcInserted = false;
            this->gcMounted  = false;
            this->clearGamecardInfo();
            mainApp->LoadLayout(mainApp->mainPage);
            return;
        }

        if (!this->gcMounted || this->gcTitles.empty()) return;

        if (Down & HidNpadButton_A) {
            int idx = this->menu->GetSelectedIndex();
            if (idx == MAIN_IDX_SD)
                this->startInstall(0);
            else if (idx == MAIN_IDX_NAND)
                this->startInstall(1);
            else if (idx == MAIN_IDX_SELECT) {
                this->inSelectionMenu = true;
                this->selectionItemMasks.clear();
                this->buildSelectionMenu();
            }
        }

        if (Down & HidNpadButton_X)
            this->startInstall(1);

        if (Down & HidNpadButton_Y)
            this->showGamecardDetails();
    }
}
