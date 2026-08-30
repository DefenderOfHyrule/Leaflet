#include <algorithm>

extern char** __system_argv; // path of the running NRO
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
#include <switch.h>

#include "ui/MainApplication.hpp"
#include "ui/fileBrowserPage.hpp"
#include "ui/netInstPage.hpp"
#include "ui/gcInstPage.hpp"
#include "ui/mainPage.hpp"
#include "ui/instPage.hpp"
#include "ui/optionsPage.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/curl.hpp"
#include "util/unzip.hpp"
#include "util/lang.hpp"
#include "util/avm_maintenance.hpp"
#include "util/title_util.hpp"
#include "util/title_manage.hpp"
#include "ui/instPage.hpp"
#include "ui/bottomHint.hpp"
#include "quark/quark_id.hpp"
#include "quark/quark_usb.hpp"

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {
    extern MainApplication *mainApp;

    static constexpr s32 kOptionsMenuItemHeight = 68;
    static constexpr s32 kOptionsMenuItemsToShow = 9;

    static pu::ui::Color WithAlpha(pu::ui::Color c, u8 a) {
        return pu::ui::Color(c.R, c.G, c.B, a);
    }

    std::vector<std::string> languageStrings = {"English", "日本語", "Français", "Deutsch", "Italiano", "Español", "Português", "한국어", "Русский", "簡体中文","繁體中文"};

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

    static std::string GetOrCacheTitleIconPath(std::uint64_t appId) {
        char idStr[32] = {};
        std::snprintf(idStr, sizeof(idStr), "%016lX", appId);
        const std::string cacheDir = inst::config::appDir + "/icon_cache";
        const std::string path = cacheDir + "/" + idStr + ".jpg";

        struct stat st = {};
        if (stat(path.c_str(), &st) == 0 && st.st_size > 0)
            return path;

        NsApplicationControlData controlData = {};
        std::size_t controlDataSize = 0;
        Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, appId,
            &controlData, sizeof(controlData), &controlDataSize);
        if (R_FAILED(rc) || controlDataSize <= sizeof(NacpStruct)) return "";

        const std::size_t iconSize = controlDataSize - sizeof(NacpStruct);
        if (iconSize == 0) return "";

        std::error_code ec;
        std::filesystem::create_directories(cacheDir, ec);

        std::ofstream ofs(path, std::ios::binary);
        if (!ofs) return "";
        ofs.write(reinterpret_cast<const char*>(controlData.icon), static_cast<std::streamsize>(iconSize));
        if (!ofs) return "";
        return path;
    }

    struct ThemePreset {
        const char* name;
        const char* background;
        const char* topBar;
        const char* botBar;
        const char* tileHighlight;
        const char* menuHighlight;
        const char* gradA;
        const char* gradB;
        const char* gradType;
        int          gradAngle;
    };
    
    // generic ass theme names, courtesy of google
    static const ThemePreset kThemes[] = {
        { "Leaflet",          "#670000FF", "#8D00FFBF", "#17090980", "#FF3B00A0", "#FF3B00A0", "#FF00FFFF", "#00FFFFFF", "linear", 45 },
        { "Midnight Blue",    "#001840FF", "#0A2A6EBF", "#030D2280", "#4488FFA0", "#4488FFA0", "#4488FFFF", "#00CCFFFF", "linear", 45 },
        { "Forest",           "#0A2A00FF", "#1A5200BF", "#0A1A0080", "#44FF88A0", "#44FF88A0", "#44FF88FF", "#00FF44FF", "linear", 45 },
        { "Obsidian",         "#0A0A0AFF", "#1A1A1ABF", "#0F0F0F80", "#888888A0", "#888888A0", "#888888FF", "#444444FF", "linear", 45 },
        { "Sakura",           "#2A0018FF", "#6E0A3ABF", "#220A1880", "#FF88CCA0", "#FF88CCA0", "#FF88CCFF", "#FF44AAFF", "linear", 45 },
        { "Sunset",           "#2A1400FF", "#8A3A00BF", "#1A0E0080", "#FF8844A0", "#FF8844A0", "#FF8844FF", "#FF4400FF", "linear", 45 },
        { "Arctic",           "#001A2AFF", "#003A5ABF", "#00111A80", "#88CCFFA0", "#88CCFFA0", "#88CCFFFF", "#44AAFFFF", "linear", 45 },
    };

    namespace {
        std::vector<std::string> WrapDialogText(const std::string& text, std::size_t maxLineChars)
        {
            std::vector<std::string> lines;
            if (text.empty()) { lines.push_back("No changelog available."); return lines; }
            std::stringstream paragraphs(text);
            std::string paragraph;
            while (std::getline(paragraphs, paragraph)) {
                if (paragraph.empty()) { lines.push_back(""); continue; }
                std::stringstream words(paragraph);
                std::string word, line;
                while (words >> word) {
                    const std::string candidate = line.empty() ? word : (line + " " + word);
                    if (candidate.size() <= maxLineChars) { line = candidate; continue; }
                    if (!line.empty()) { lines.push_back(line); line.clear(); }
                    if (word.size() <= maxLineChars) { line = word; continue; }
                    std::size_t start = 0;
                    while (start < word.size()) { lines.push_back(word.substr(start, maxLineChars)); start += maxLineChars; }
                }
                if (!line.empty()) lines.push_back(line);
            }
            if (lines.empty()) lines.push_back("No changelog available.");
            return lines;
        }

        void ShowPagedTextDialog(const std::string& title, const std::string& text)
        {
            auto lines = WrapDialogText(text, 68);
            static constexpr int kLinesPerPage = 18;
            const int totalPages = std::max(1, static_cast<int>((lines.size() + kLinesPerPage - 1) / kLinesPerPage));
            int page = 0;
            while (true) {
                const int start = page * kLinesPerPage;
                const int end = std::min<int>(static_cast<int>(lines.size()), start + kLinesPerPage);
                std::string body;
                for (int i = start; i < end; i++) { if (!body.empty()) body.push_back('\n'); body += lines[static_cast<std::size_t>(i)]; }
                body += "\n\nPage " + std::to_string(page + 1) + "/" + std::to_string(totalPages);
                std::vector<std::string> options; std::vector<int> actions;
                if (page > 0) { options.push_back("Previous"); actions.push_back(-1); }
                if (page + 1 < totalPages) { options.push_back("Next"); actions.push_back(1); }
                options.push_back("Close"); actions.push_back(0);
                const int choice = mainApp->CreateShowDialog(title, body, options, false);
                if (choice < 0 || choice >= static_cast<int>(actions.size()) || actions[choice] == 0) break;
                page = std::max(0, std::min(totalPages - 1, page + actions[choice]));
            }
        }
    }

    optionsPage::optionsPage() : Layout::Layout() {
        this->SetBackgroundColor(COLOR(inst::config::colorBackground));
        if (std::filesystem::exists(inst::config::appDir + "/background.png")) this->SetBackgroundImage(inst::config::appDir + "/background.png");
        else this->SetBackgroundImage("romfs:/images/background.jpg");
        const auto botColor  = COLOR(inst::config::colorBotBar);
        this->botRect     = Rectangle::New(0, 660, 1280, 60, botColor);
        this->sideNavRect = Rectangle::New(0, 46, 300, 613, COLOR(inst::config::colorBotBar));
        const std::string optionsHintText = " Select/Change     Section     Back";
        this->butText = TextBlock::New(10, 678, optionsHintText, 20);
        this->butText->SetColor(COLOR("#FFFFFFFF"));
        this->bottomHintSegments = BuildBottomHintSegments(optionsHintText, 10, 20);
        this->menu = pu::ui::elm::Menu::New(301, 46, 979, COLOR("#FFFFFF00"), kOptionsMenuItemHeight, kOptionsMenuItemsToShow, 20);
        this->menu->SetOnFocusColor(COLOR(inst::config::menuHighlightColorCapped()));
        this->menu->SetScrollbarColor(COLOR(inst::config::colorBotBar));
        this->statusBar = StatusBar::New(StatusBar::Mode::Slim, "options.title"_lang); this->statusBar->Attach(this); this->Add(this->botRect);
        this->Add(this->sideNavRect);this->Add(this->butText);
        for (int i = 0; i < 3; i++) {
            auto sectionHighlight = Rectangle::New(30, 62 + (i * 56), 240, 48, COLOR("#FFFFFF00"), 10);
            this->sectionHighlights.push_back(sectionHighlight);
            this->Add(sectionHighlight);
            auto sectionText = TextBlock::New(40, 80 + (i * 56), "", 26);
            sectionText->SetColor(COLOR("#FFFFFFFF"));
            this->sectionTexts.push_back(sectionText);
            this->Add(sectionText);
        }
        this->sectionMenuIndices.assign(this->sectionTexts.size(), 0);
        this->refreshOptions(true);
        this->Add(this->menu);
    }

    void optionsPage::askToUpdate(std::vector<std::string> updateInfo) {
        const std::string version     = updateInfo.empty()       ? std::string() : updateInfo[0];
        const std::string downloadUrl = updateInfo.size() > 1   ? updateInfo[1] : std::string();
        const std::string releaseNotes= updateInfo.size() > 2   ? updateInfo[2] : "No changelog available.";
        while (true) {
            int choice = mainApp->CreateShowDialog(
                "options.update.title"_lang,
                "options.update.desc0"_lang + version + "options.update.desc1"_lang,
                {"options.update.opt0"_lang, "View Changelog", "common.cancel"_lang}, false);
            if (choice == 1) { ShowPagedTextDialog("Changelog " + version, releaseNotes); continue; }
            if (choice != 0) break;
            inst::ui::instPage::loadInstallScreen();
            inst::ui::instPage::setTopInstInfoText("options.update.top_info"_lang + version);
            inst::ui::instPage::setInstBarPerc(0);
            inst::ui::instPage::setInstInfoText("options.update.bot_info"_lang + version);
            try {
                    romfsExit();
                    if (downloadUrl.size() >= 4 && downloadUrl.substr(downloadUrl.size() - 4) == ".nro") {
                        // overwrite the running NRO in place
                        const std::string dest = (__system_argv && __system_argv[0])
                            ? std::string(__system_argv[0]) : std::string();
                        if (dest.empty()) throw std::runtime_error("NRO path unavailable");
                        inst::curl::downloadFile(downloadUrl, dest.c_str(), 0, true);
                    } else {
                        const std::string tempZip = inst::config::appDir + "/temp_download.zip";
                        inst::curl::downloadFile(downloadUrl, tempZip.c_str(), 0, true);
                        inst::ui::instPage::setInstInfoText("options.update.bot_info2"_lang + version);
                        inst::zip::extractFile(tempZip, "sdmc:/");
                        std::filesystem::remove(tempZip);
                    }
                    mainApp->CreateShowDialog("options.update.complete"_lang, "options.update.end_desc"_lang, {"common.ok"_lang}, false);
                } catch (...) {
                    mainApp->CreateShowDialog("options.update.failed"_lang, "options.update.end_desc"_lang, {"common.ok"_lang}, false);
                }
            inst::ui::instPage::loadMainMenu();
            mainApp->FadeOut();
            mainApp->Close();
            break;
        }
    }

    std::string optionsPage::getMenuOptionIcon(bool ourBool) {
        return ourBool ? "romfs:/images/icons/check-box-outline.png" : "romfs:/images/icons/checkbox-blank-outline.png";
    }

    std::string optionsPage::getMenuLanguage(int ourLangCode) {
        switch (ourLangCode) {
            case 1: case 12: return languageStrings[0];
            case 0:          return languageStrings[1];
            case 2: case 13: return languageStrings[2];
            case 3:          return languageStrings[3];
            case 4:          return languageStrings[4];
            case 5: case 14: return languageStrings[5];
            case 9:          return languageStrings[6];
            case 7:          return languageStrings[7];
            case 10:         return languageStrings[8];
            case 6:          return languageStrings[9];
            case 11:         return languageStrings[10];
            default:         return "options.language.system_language"_lang;
        }
    }

    void optionsPage::setSectionNavText() {
        static const std::vector<std::string> sectionLabels = {"General", "options.appearance.section"_lang, "System"};
        for (size_t i = 0; i < this->sectionTexts.size() && i < sectionLabels.size(); i++) {
            const bool selected = static_cast<int>(i) == this->selectedSection;
            this->sectionHighlights[i]->SetColor(selected
                ? (this->tabsFocused
                    ? COLOR("#FFFFFF66")
                    : COLOR("#FFFFFF40"))
                : COLOR("#FFFFFF00"));
            this->sectionTexts[i]->SetText(sectionLabels[i]);
            this->sectionTexts[i]->SetColor(selected ? COLOR("#FFFFFFFF") : (this->tabsFocused ? COLOR("#FFFFFFCC") : COLOR("#FFFFFF99")));
        }
        const auto highlight = COLOR(inst::config::menuHighlightColorCapped());
        const auto dimHighlight = WithAlpha(highlight, static_cast<u8>(highlight.A * 0.31f));
        this->menu->SetOnFocusColor(this->tabsFocused ? dimHighlight : highlight);
    }

    void optionsPage::setSettingsMenuText() {
        this->menu->ClearItems();
        if (this->inTitleFixPicker || this->inTitleManagePicker || this->inTitleManageDetail) {
            constexpr s32 kPickerAreaHeight = 613;
            constexpr s32 kPickerRows = 4;
            this->menu->SetItemSize(kPickerAreaHeight / kPickerRows);
            this->menu->SetNumberOfItemsToShow(kPickerRows);
        } else {
            this->menu->SetItemSize(kOptionsMenuItemHeight);
            this->menu->SetNumberOfItemsToShow(kOptionsMenuItemsToShow);
        }
        auto addItem = [this](const std::string &label, bool toggle, bool value) {
            auto item = pu::ui::elm::MenuItem::New(label);
            item->SetColor(COLOR("#FFFFFFFF"));
            if (toggle) item->SetIcon(this->getMenuOptionIcon(value));
            this->menu->AddItem(item);
        };

        if (this->inTitleManageDetail) {
            for (auto& piece : this->manageDetailPieces) {
                if (piece.isBase) continue;
                std::string label = piece.label + "  (" + FormatSize(piece.sizeBytes) + ", " +
                    (piece.storageId == NcmStorageId_SdCard ? "options.title_manage.sd_card"_lang
                                                             : "options.title_manage.internal"_lang) + ")";
                auto item = pu::ui::elm::MenuItem::New(label);
                item->SetColor(COLOR("#FFFFFFFF"));
                item->SetIcon(!this->manageDetailIconPath.empty() ? this->manageDetailIconPath
                                                                   : "romfs:/images/icons/settings.png");
                this->menu->AddItem(item);
            }
            {
                auto item = pu::ui::elm::MenuItem::New("options.title_manage.delete_all"_lang);
                item->SetColor(COLOR("#FF6B6BFF"));
                item->SetIcon(!this->manageDetailIconPath.empty() ? this->manageDetailIconPath
                                                                   : "romfs:/images/icons/settings.png");
                this->menu->AddItem(item);
            }
            return;
        }

        if (this->inTitleManagePicker) {
            if (this->managePickerTitles.empty()) {
                auto item = pu::ui::elm::MenuItem::New("options.fix_update_flags_picker.empty"_lang);
                item->SetColor(COLOR("#FFFFFF99"));
                this->menu->AddItem(item);
            } else {
                for (auto& t : this->managePickerTitles) {
                    auto item = pu::ui::elm::MenuItem::New(t.name);
                    item->SetColor(COLOR("#FFFFFFFF"));
                    item->SetIcon(!t.iconPath.empty() ? t.iconPath : "romfs:/images/icons/settings.png");
                    this->menu->AddItem(item);
                }
            }
            return;
        }

        if (this->inTitleFixPicker) {
            if (this->fixPickerTitles.empty()) {
                auto item = pu::ui::elm::MenuItem::New("options.fix_update_flags_picker.empty"_lang);
                item->SetColor(COLOR("#FFFFFF99"));
                this->menu->AddItem(item);
            } else {
                for (auto& t : this->fixPickerTitles) {
                    auto item = pu::ui::elm::MenuItem::New(t.name);
                    item->SetColor(COLOR("#FFFFFFFF"));
                    item->SetIcon(!t.iconPath.empty() ? t.iconPath : "romfs:/images/icons/settings.png");
                    this->menu->AddItem(item);
                }
            }
            return;
        }

        if (this->selectedSection == 0) {
            addItem("options.menu_items.ignore_firm"_lang,  true,  inst::config::ignoreReqVers);
            addItem("options.menu_items.verbose_logging"_lang, true,  inst::config::verboseInstallLogging);
            addItem("options.menu_items.ask_delete"_lang,   true,  inst::config::deletePrompt);
            addItem("options.menu_items.sound"_lang,         true,  inst::config::soundEnabled);
            addItem("options.menu_items.auto_skip_reinstall"_lang, true,  inst::config::autoSkipReinstall);
            addItem("options.menu_items.cancel_queue_on_error"_lang, true,  inst::config::cancelQueueOnError);
            addItem("options.menu_items.screen_dim_disable"_lang, true,  inst::config::installDimDisable);
            addItem("options.menu_items.screen_dim_delay"_lang + ": " + std::to_string(inst::config::installDimDelay) + "s",
                    false, false);
            addItem("options.menu_items.emummc_check"_lang, true, !inst::config::emummcSafetyDisabled);
            addItem("options.menu_items.sigpatch_check"_lang, true, inst::config::sigPatchCheckDisabled);
        } else if (this->selectedSection == 1) {
            if (this->inCustomTheme) {
                
                addItem("options.appearance.bg_color"_lang + ":  "  + inst::config::colorBackground,    false, false);
                addItem("options.appearance.panel_color"_lang + ":  "        + inst::config::colorTopBar,        false, false);
                addItem("options.appearance.bar_color"_lang + ":  "          + inst::config::colorBotBar,        false, false);
                addItem("options.appearance.highlight_color"_lang + ":  "    + inst::config::colorTileHighlight, false, false);
                addItem("options.appearance.menu_highlight_color"_lang + ":  " + inst::config::colorMenuHighlight, false, false);
                addItem("options.appearance.dialog_bg_color"_lang + ":  "     + inst::config::colorDialogBackground, false, false);
                addItem("options.appearance.dialog_border_color"_lang + ":  " + inst::config::colorDialogBorder,     false, false);
                addItem("options.appearance.gradient_start"_lang + ":  "     + inst::config::gradientColorA,     false, false);
                addItem("options.appearance.gradient_end"_lang + ":  "       + inst::config::gradientColorB,     false, false);
                addItem("options.appearance.gradient_type"_lang + ":  "      +
                    (inst::config::gradientType == "radial" ? "options.appearance.gradient_type_radial"_lang
                                                              : "options.appearance.gradient_type_linear"_lang),
                    false, false);
                addItem("options.appearance.gradient_angle"_lang + ":  "     + std::to_string(inst::config::gradientAngle) + "°", false, false);
                addItem("options.appearance.regen_gradient"_lang,                            false, false);
            } else {
                addItem("options.appearance.select_preset"_lang, false, false);
                addItem("options.appearance.custom_theme"_lang,  false, false);
                addItem("options.appearance.import_theme"_lang,  false, false);
                addItem("options.appearance.reset_theme"_lang,   false, false);
            }
        } else {
            addItem("options.menu_items.language"_lang + this->getMenuLanguage(inst::config::languageSetting), false, false);
            addItem("Console ID:  " + inst::config::consoleId +
                    (inst::config::consoleIdIsCustom ? "  (custom)" : ""), false, false);
            addItem("options.menu_items.check_update"_lang, false, false);
            addItem("options.menu_items.credits"_lang,       false, false);
            addItem("options.menu_items.fix_update_flags"_lang, false, false);
            addItem("options.menu_items.manage_titles"_lang, false, false);
        }

        if (!this->menu->GetItems().empty())
            this->menu->SetSelectedIndex(0);
    }

    void optionsPage::setSection(int section) {
        this->selectedSection = section;
        this->setSectionNavText();
        this->refreshOptions(true);
    }

    void optionsPage::refreshOptions(bool resetSelection) {
        this->setSectionNavText();
        this->setSettingsMenuText();
        if (resetSelection) this->menu->SetSelectedIndex(0);
    }

    void optionsPage::startTitleFixPicker() {
        inst::util::initInstallServices();
        auto ids = inst::util::ListDigitallyInstalledAppIds();

        this->fixPickerTitles.clear();
        for (auto id : ids) {
            FixPickerEntry entry;
            entry.appId   = id;
            entry.name    = leaf::util::GetBaseTitleName(id);
            entry.iconPath = GetOrCacheTitleIconPath(id);
            this->fixPickerTitles.push_back(entry);
        }
        inst::util::deinitInstallServices();

        std::sort(this->fixPickerTitles.begin(), this->fixPickerTitles.end(),
            [](const auto& a, const auto& b) { return a.name < b.name; });

        this->inTitleFixPicker = true;
        this->refreshOptions(true);
    }

    void optionsPage::startTitleManagePicker() {
        inst::util::initInstallServices();
        auto ids = inst::util::ListDigitallyInstalledAppIds();

        this->managePickerTitles.clear();
        for (auto id : ids) {
            FixPickerEntry entry;
            entry.appId    = id;
            entry.name     = leaf::util::GetBaseTitleName(id);
            entry.iconPath = GetOrCacheTitleIconPath(id);
            this->managePickerTitles.push_back(entry);
        }
        inst::util::deinitInstallServices();

        std::sort(this->managePickerTitles.begin(), this->managePickerTitles.end(),
            [](const auto& a, const auto& b) { return a.name < b.name; });

        this->inTitleManagePicker = true;
        this->refreshOptions(true);
    }

    void optionsPage::enterTitleManageDetail(int titleIdx) {
        if (titleIdx < 0 || titleIdx >= static_cast<int>(this->managePickerTitles.size())) return;
        auto& picked = this->managePickerTitles[titleIdx];

        this->manageDetailAppId    = picked.appId;
        this->manageDetailTitleName = picked.name;
        this->manageDetailIconPath  = picked.iconPath;

        inst::util::initInstallServices();
        this->manageDetailPieces = inst::util::GetInstalledContentForTitle(this->manageDetailAppId);
        inst::util::deinitInstallServices();

        this->inTitleManagePicker = false;
        this->inTitleManageDetail = true;
        this->refreshOptions(true);
    }

    void optionsPage::refreshTitleManageDetail() {
        inst::util::initInstallServices();
        this->manageDetailPieces = inst::util::GetInstalledContentForTitle(this->manageDetailAppId);
        inst::util::deinitInstallServices();
        this->refreshOptions(true);
    }

    bool optionsPage::warnIfAtmosphereOverride(std::uint64_t appId) {
        if (!inst::util::IsCurrentTakeoverTarget(appId)) return true;

        const std::string title = "options.title_manage.override_warning.title_prefix"_lang +
            this->manageDetailTitleName + ")";

        int choice = mainApp->CreateShowDialog(
            title, "options.title_manage.override_warning.desc"_lang,
            {"options.title_manage.override_warning.confirm"_lang, "common.cancel"_lang}, false);
        return choice == 0;
    }

    void optionsPage::runDisableCheckCountdown(const std::string& langPrefix, bool& configFlag) {
        int choice = mainApp->CreateShowDialog(
            Language::LanguageEntry(langPrefix + ".title"),
            Language::LanguageEntry(langPrefix + ".desc"),
            {Language::LanguageEntry(langPrefix + ".begin"), "common.cancel"_lang}, false);
        if (choice != 0) return;

        const std::string countdownSuffix = Language::LanguageEntry(langPrefix + ".countdown_suffix");
        const std::string originalHint = this->butText->GetText();
        for (int remaining = 10; remaining > 0; remaining--) {
            this->butText->SetText(std::to_string(remaining) + countdownSuffix);
            mainApp->CallForRender();
            svcSleepThread(1'000'000'000ULL);
        }
        this->butText->SetText(originalHint);
        mainApp->CallForRender();

        int finalChoice = mainApp->CreateShowDialog(
            Language::LanguageEntry(langPrefix + ".title"),
            Language::LanguageEntry(langPrefix + ".final_desc"),
            {Language::LanguageEntry(langPrefix + ".confirm"), "common.cancel"_lang}, false);
        if (finalChoice == 0) {
            configFlag = true;
            inst::config::setConfig();
        }
        this->refreshOptions();
    }

    void optionsPage::confirmDisableEmummcCheck() {
        this->runDisableCheckCountdown("options.emummc_check.disable_warn", inst::config::emummcSafetyDisabled);
    }

    void optionsPage::confirmDisableSigPatchCheck() {
        this->runDisableCheckCountdown("options.sigpatch_check.disable_warn", inst::config::sigPatchCheckDisabled);
    }

    void optionsPage::rememberCurrentSectionMenuIndex() {
        if (this->selectedSection < 0) return;
        if (this->selectedSection >= static_cast<int>(this->sectionMenuIndices.size()))
            this->sectionMenuIndices.resize(this->sectionTexts.size(), 0);
        int selected = this->menu->GetSelectedIndex();
        if (selected < 0) selected = 0;
        this->sectionMenuIndices[this->selectedSection] = selected;
    }

    void optionsPage::restoreSelectedSectionMenuIndex() {
        if (this->selectedSection < 0 || this->selectedSection >= static_cast<int>(this->sectionMenuIndices.size())) return;
        const int itemCount = static_cast<int>(this->menu->GetItems().size());
        if (itemCount <= 0) return;
        int selected = this->sectionMenuIndices[this->selectedSection];
        if (selected < 0) selected = 0;
        if (selected >= itemCount) selected = itemCount - 1;
        this->sectionMenuIndices[this->selectedSection] = selected;
        this->menu->SetSelectedIndex(selected);
    }

    void optionsPage::setSelectedSectionAndRefresh(int newSection) {
        if (this->sectionTexts.empty()) return;
        const int sectionCount = static_cast<int>(this->sectionTexts.size());
        if (newSection < 0) newSection = sectionCount - 1;
        if (newSection >= sectionCount) newSection = 0;
        this->rememberCurrentSectionMenuIndex();
        this->selectedSection = newSection;
        this->refreshOptions(false);
        this->restoreSelectedSectionMenuIndex();
        this->lockedMenuIndex = this->menu->GetSelectedIndex();
    }

    int optionsPage::getSectionFromTouch(int x, int y) const {
        const int navX = this->sideNavRect->GetProcessedX();
        const int navY = this->sideNavRect->GetProcessedY();
        const int navW = this->sideNavRect->GetWidth();
        const int navH = this->sideNavRect->GetHeight();
        const bool inNav = (x >= navX) && (x <= navX + navW) && (y >= navY) && (y <= navY + navH);
        if (!inNav) return -1;
        for (size_t i = 0; i < this->sectionTexts.size(); i++) {
            const int secY = this->sectionTexts[i]->GetProcessedY();
            if (y >= secY - 14 && y <= secY + 42) return static_cast<int>(i);
        }
        int nearestIdx = -1, nearestDist = 1 << 30;
        for (size_t i = 0; i < this->sectionTexts.size(); i++) {
            int dist = y - (this->sectionTexts[i]->GetProcessedY() + 14);
            if (dist < 0) dist = -dist;
            if (dist < nearestDist) { nearestDist = dist; nearestIdx = static_cast<int>(i); }
        }
        return (nearestDist <= 90) ? nearestIdx : -1;
    }

    void optionsPage::onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos) {
        (void)Up; (void)Held;
        int bottomTapX = 0;
        if (DetectBottomHintTap(Pos, this->bottomHintTouch, 668, 52, bottomTapX))
            Down |= FindBottomHintButton(this->bottomHintSegments, bottomTapX);
        inst::util::playNavigationClickIfNeeded(Down);

        if (this->inTitleManageDetail) {
            if (Down & HidNpadButton_B) {
                this->inTitleManageDetail = false;
                this->inTitleManagePicker = true;
                this->refreshOptions(true);
                return;
            }
            if (Down & HidNpadButton_A) {
                std::vector<inst::util::InstalledContentPiece> deletable;
                for (auto& p : this->manageDetailPieces) if (!p.isBase) deletable.push_back(p);

                int idx = this->menu->GetSelectedIndex();
                if (idx >= 0 && idx < static_cast<int>(deletable.size())) {
                    auto piece = deletable[idx];
                    if (!this->warnIfAtmosphereOverride(this->manageDetailAppId)) return;
                    std::string label = piece.label + " (" + FormatSize(piece.sizeBytes) + ")";
                    int confirm = mainApp->CreateShowDialog(
                        "options.title_manage.delete_piece_dialog.title"_lang,
                        label + "options.title_manage.delete_piece_dialog.desc_suffix"_lang,
                        {"options.title_manage.delete_piece_dialog.confirm"_lang, "common.cancel"_lang}, false);
                    if (confirm == 0) {
                        inst::util::initInstallServices();
                        inst::util::DeleteInstalledContentPiece(this->manageDetailAppId, piece);
                        inst::util::deinitInstallServices();
                        this->refreshTitleManageDetail();
                        mainApp->CreateShowDialog(
                            "options.title_manage.piece_deleted.title"_lang,
                            "options.title_manage.piece_deleted.desc"_lang,
                            {"common.ok"_lang}, false);
                    }
                } else if (idx == static_cast<int>(deletable.size())) {
                    if (!this->warnIfAtmosphereOverride(this->manageDetailAppId)) return;
                    int confirm = mainApp->CreateShowDialog(
                        "options.title_manage.delete_all_dialog.title"_lang,
                        "\"" + this->manageDetailTitleName + "\"" + "options.title_manage.delete_all_dialog.desc_suffix"_lang,
                        {"options.title_manage.delete_all_dialog.confirm"_lang, "common.cancel"_lang}, false);
                    if (confirm == 0) {
                        inst::util::initInstallServices();
                        bool ok = inst::util::DeleteTitleCompletely(this->manageDetailAppId);
                        inst::util::deinitInstallServices();

                        this->inTitleManageDetail = false;
                        this->startTitleManagePicker();

                        mainApp->CreateShowDialog(
                            ok ? "options.title_manage.title_deleted.title"_lang : "options.title_manage.delete_failed.title"_lang,
                            ok ? "options.title_manage.title_deleted.desc"_lang : "options.title_manage.delete_failed.desc"_lang,
                            {"common.ok"_lang}, false);
                    }
                }
                return;
            }
            return;
        }

        if (this->inTitleManagePicker) {
            if (Down & HidNpadButton_B) {
                this->inTitleManagePicker = false;
                this->refreshOptions(true);
                return;
            }
            if (Down & HidNpadButton_A) {
                this->enterTitleManageDetail(this->menu->GetSelectedIndex());
                return;
            }
            return;
        }

        if (this->inTitleFixPicker) {
            if (Down & HidNpadButton_B) {
                this->inTitleFixPicker = false;
                this->refreshOptions(true);
                return;
            }
            if (Down & HidNpadButton_A) {
                int idx = this->menu->GetSelectedIndex();
                if (idx >= 0 && idx < static_cast<int>(this->fixPickerTitles.size())) {
                    auto picked = this->fixPickerTitles[idx];
                    inst::util::initInstallServices();
                    inst::util::FixAvmFloorForTitle(picked.appId);
                    inst::util::deinitInstallServices();

                    this->inTitleFixPicker = false;
                    this->refreshOptions(true);

                    mainApp->CreateShowDialog(
                        "options.fix_update_flags_done.title"_lang,
                        "\"" + picked.name + "\"" + "options.fix_update_flags_done_one.desc_suffix"_lang,
                        {"common.ok"_lang}, false);
                }
                return;
            }
            return;
        }

        if (Down & HidNpadButton_B) {
            if (this->inCustomTheme) {
                this->inCustomTheme = false;
                this->refreshOptions();
            } else {
                mainApp->LoadLayout(mainApp->mainPage);
            }
        }

        const bool leftPressed  = (Down & (HidNpadButton_Left  | HidNpadButton_StickLLeft))  != 0;
        const bool rightPressed = (Down & (HidNpadButton_Right | HidNpadButton_StickLRight)) != 0;
        const bool upPressed    = (Down & (HidNpadButton_Up    | HidNpadButton_StickLUp))    != 0;
        const bool downPressed  = (Down & (HidNpadButton_Down  | HidNpadButton_StickLDown))  != 0;

        if (leftPressed && !this->tabsFocused) {
            this->tabsFocused = true;
            this->rememberCurrentSectionMenuIndex();
            this->lockedMenuIndex = this->menu->GetSelectedIndex();
            this->setSectionNavText();
        } else if (rightPressed && this->tabsFocused) {
            this->tabsFocused = false;
            this->restoreSelectedSectionMenuIndex();
            this->setSectionNavText();
        }
        if (Down & HidNpadButton_L) { this->tabsFocused = true; this->setSelectedSectionAndRefresh(this->selectedSection - 1); }
        if (Down & HidNpadButton_R) { this->tabsFocused = true; this->setSelectedSectionAndRefresh(this->selectedSection + 1); }
        if (this->tabsFocused) {
            if (upPressed && !downPressed) this->setSelectedSectionAndRefresh(this->selectedSection - 1);
            else if (downPressed)          this->setSelectedSectionAndRefresh(this->selectedSection + 1);
            this->menu->SetSelectedIndex(this->lockedMenuIndex);
        } else {
            this->lockedMenuIndex = this->menu->GetSelectedIndex();
            this->rememberCurrentSectionMenuIndex();
        }

        bool touchSelect = false;
        if (!Pos.IsEmpty()) {
            if (!this->touchActive) {
                this->touchActive = true; this->touchMoved = false;
                this->touchStartX = Pos.X; this->touchStartY = Pos.Y;
                const bool inMenu = (Pos.X >= this->menu->GetProcessedX()) && (Pos.X <= this->menu->GetProcessedX() + this->menu->GetWidth()) &&
                                    (Pos.Y >= this->menu->GetProcessedY()) && (Pos.Y <= this->menu->GetProcessedY() + this->menu->GetHeight());
                const bool inSideNav = this->getSectionFromTouch(Pos.X, Pos.Y) >= 0;
                this->touchRegion = inSideNav ? 1 : (inMenu ? 2 : 0);
            } else {
                int dx = Pos.X - this->touchStartX; if (dx < 0) dx = -dx;
                int dy = Pos.Y - this->touchStartY; if (dy < 0) dy = -dy;
                if (dx > 12 || dy > 12) this->touchMoved = true;
            }
        } else if (this->touchActive) {
            if (!this->touchMoved) {
                if (this->touchRegion == 1) {
                    this->tabsFocused = true;
                    int touchedSection = this->getSectionFromTouch(this->touchStartX, this->touchStartY);
                    if (touchedSection >= 0 && touchedSection != this->selectedSection) this->setSelectedSectionAndRefresh(touchedSection);
                    else this->setSectionNavText();
                } else if (this->touchRegion == 2) {
                    this->tabsFocused = false;
                    this->restoreSelectedSectionMenuIndex();
                    this->setSectionNavText();
                    touchSelect = true;
                }
            }
            this->touchActive = false; this->touchMoved = false; this->touchRegion = 0;
        }

        bool tabAcceptOnly = false;
        if ((Down & HidNpadButton_A) && this->tabsFocused) {
            this->tabsFocused = false;
            this->restoreSelectedSectionMenuIndex();
            this->setSectionNavText();
            tabAcceptOnly = true;
        }

        if ((((Down & HidNpadButton_A) && !this->tabsFocused) && !tabAcceptOnly) || touchSelect) {
            int selectedIndex = this->menu->GetSelectedIndex();
            std::vector<std::string> languageList;
            int rc;

        if (this->selectedSection == 0) {
                switch (selectedIndex) {
                    case 0:
                        inst::config::ignoreReqVers = !inst::config::ignoreReqVers;
                        inst::config::setConfig(); this->refreshOptions(); break;
                    case 1:
                        inst::config::verboseInstallLogging = !inst::config::verboseInstallLogging;
                        inst::config::setConfig(); this->refreshOptions(); break;
                    case 2:
                        inst::config::deletePrompt = !inst::config::deletePrompt;
                        inst::config::setConfig(); this->refreshOptions(); break;
                    case 3:
                        inst::config::soundEnabled = !inst::config::soundEnabled;
                        inst::config::setConfig(); this->refreshOptions(); break;
                    case 4:
                        inst::config::autoSkipReinstall = !inst::config::autoSkipReinstall;
                        inst::config::setConfig(); this->refreshOptions(); break;
                    case 5:
                        inst::config::cancelQueueOnError = !inst::config::cancelQueueOnError;
                        inst::config::setConfig(); this->refreshOptions(); break;
                    case 6:
                        inst::config::installDimDisable = !inst::config::installDimDisable;
                        inst::config::setConfig(); this->refreshOptions(); break;
                    case 7: {
                        std::vector<std::string> delayOptions = Language::LanguageArray("options.dim_delay.options");
                        int choice = mainApp->CreateShowDialog(
                            "options.dim_delay.title"_lang,
                            "options.dim_delay.desc"_lang,
                            delayOptions, false);
                        static const int delayValues[] = { 15, 30, 60, 120, 300, 600, -1 };
                        if (choice >= 0 && choice < (int)(sizeof(delayValues)/sizeof(delayValues[0]))) {
                            if (delayValues[choice] < 0) {
                                inst::config::installDimDisable = true;
                            } else {
                                inst::config::installDimDisable = false;
                                inst::config::installDimDelay   = delayValues[choice];
                            }
                            inst::config::setConfig();
                            this->refreshOptions();
                        }
                        break;
                    }
                    case 8:
                        if (inst::config::emummcSafetyDisabled) {
                            inst::config::emummcSafetyDisabled = false;
                            inst::config::setConfig();
                            this->refreshOptions();
                        } else {
                            this->confirmDisableEmummcCheck();
                        }
                        break;
                    case 9:
                        if (inst::config::sigPatchCheckDisabled) {
                            inst::config::sigPatchCheckDisabled = false;
                            inst::config::setConfig();
                            this->refreshOptions();
                        } else {
                            this->confirmDisableSigPatchCheck();
                        }
                        break;
                    default: break;
                }
            } else if (this->selectedSection == 1) {
                auto promptColor = [&](const std::string& label, const std::string& current) -> std::string {
                    std::string result = inst::util::softwareKeyboard(
                        label + " (#RRGGBBAA)", current, 9);
                    bool valid = (result.size() == 9 && result[0] == '#');
                    if (valid) {
                        for (int i = 1; i < 9; i++) {
                            char c = result[i];
                            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                                valid = false;
                                break;
                            }
                        }
                    }
                    if (!result.empty() && !valid) {
                        mainApp->CreateShowDialog("options.appearance.invalid_color.title"_lang,
                            "options.appearance.invalid_color.desc"_lang,
                            {"common.ok"_lang}, false);
                        return "";
                    }
                    return result.empty() ? "" : result;
                };

                auto applyColor = [&](std::string& target, const std::string& newVal,
                                       const std::string& colorName) {
                    target = newVal;
                    inst::config::setConfig();
                    mainApp->CreateShowDialog("options.appearance.color_applied.title"_lang,
                        colorName + "options.appearance.color_applied.desc0"_lang + newVal +
                            "options.appearance.color_applied.desc1"_lang,
                        {"OK"}, false);
                    mainApp->rebuildLayouts(1);
                    mainApp->optionspage->setInCustomTheme(true);
                    mainApp->optionspage->setSection(1);
                };

                if (this->inCustomTheme) {
                        switch (selectedIndex) {
                        case 0: { auto c = promptColor("options.appearance.bg_color"_lang,     inst::config::colorBackground);    if (!c.empty()) applyColor(inst::config::colorBackground,    c, "options.appearance.bg_color"_lang);     break; }
                        case 1: { auto c = promptColor("options.appearance.panel_color"_lang,          inst::config::colorTopBar);        if (!c.empty()) applyColor(inst::config::colorTopBar,        c, "options.appearance.panel_color"_lang);          break; }
                        case 2: { auto c = promptColor("options.appearance.bar_color"_lang,    inst::config::colorBotBar);        if (!c.empty()) applyColor(inst::config::colorBotBar,        c, "options.appearance.bar_color"_lang);            break; }
                        case 3: { auto c = promptColor("options.appearance.highlight_color"_lang,      inst::config::colorTileHighlight); if (!c.empty()) applyColor(inst::config::colorTileHighlight, c, "options.appearance.highlight_color"_lang);      break; }
                        case 4: { auto c = promptColor("options.appearance.menu_highlight_color"_lang, inst::config::colorMenuHighlight); if (!c.empty()) applyColor(inst::config::colorMenuHighlight, c, "options.appearance.menu_highlight_color"_lang); break; }
                        case 5: { auto c = promptColor("options.appearance.dialog_bg_color"_lang,     inst::config::colorDialogBackground); if (!c.empty()) applyColor(inst::config::colorDialogBackground, c, "options.appearance.dialog_bg_color"_lang);     break; }
                        case 6: { auto c = promptColor("options.appearance.dialog_border_color"_lang, inst::config::colorDialogBorder);     if (!c.empty()) applyColor(inst::config::colorDialogBorder,     c, "options.appearance.dialog_border_color"_lang); break; }
                        case 7: {
                            auto c = promptColor("options.appearance.gradient_start"_lang, inst::config::gradientColorA);
                            if (!c.empty()) {
                                inst::config::gradientColorA = c;
                                inst::config::setConfig();
                                mainApp->CreateShowDialog("options.appearance.gradient_start_set.title"_lang,
                                    "options.appearance.gradient_start_set.desc0"_lang + c +
                                        "options.appearance.gradient_start_set.desc1"_lang,
                                    {"OK"}, false);
                                this->refreshOptions();
                            }
                            break;
                        }
                        case 8: {
                            auto c = promptColor("options.appearance.gradient_end"_lang, inst::config::gradientColorB);
                            if (!c.empty()) {
                                inst::config::gradientColorB = c;
                                inst::config::setConfig();
                                mainApp->CreateShowDialog("options.appearance.gradient_end_set.title"_lang,
                                    "options.appearance.gradient_end_set.desc0"_lang + c +
                                        "options.appearance.gradient_end_set.desc1"_lang,
                                    {"OK"}, false);
                                this->refreshOptions();
                            }
                            break;
                        }
                        case 9: {
                            std::vector<std::string> typeOptions = {
                                "options.appearance.gradient_type_linear"_lang,
                                "options.appearance.gradient_type_radial"_lang
                            };
                            int choice = mainApp->CreateShowDialog(
                                "options.appearance.gradient_type"_lang,
                                "options.appearance.gradient_type_dialog.desc"_lang,
                                typeOptions, false);
                            if (choice == 0 || choice == 1) {
                                inst::config::gradientType = (choice == 1) ? "radial" : "linear";
                                inst::config::setConfig();
                                mainApp->CreateShowDialog("options.appearance.gradient_type_set.title"_lang,
                                    typeOptions[choice] + "options.appearance.gradient_type_set.desc_suffix"_lang,
                                    {"OK"}, false);
                                this->refreshOptions();
                            }
                            break;
                        }
                        case 10: {
                            std::string result = inst::util::softwareKeyboard(
                                "options.appearance.gradient_angle"_lang + " (0-359)",
                                std::to_string(inst::config::gradientAngle), 3);
                            if (!result.empty()) {
                                bool valid = true;
                                for (char c : result) {
                                    if (c < '0' || c > '9') { valid = false; break; }
                                }
                                int angle = -1;
                                if (valid) {
                                    try { angle = std::stoi(result); } catch (...) { valid = false; }
                                    if (angle < 0 || angle > 359) valid = false;
                                }
                                if (valid) {
                                    inst::config::gradientAngle = angle;
                                    inst::config::setConfig();
                                    mainApp->CreateShowDialog("options.appearance.gradient_angle_set.title"_lang,
                                        std::to_string(angle) + "°" + "options.appearance.gradient_angle_set.desc_suffix"_lang,
                                        {"OK"}, false);
                                    this->refreshOptions();
                                } else {
                                    mainApp->CreateShowDialog("options.appearance.invalid_angle.title"_lang,
                                        "options.appearance.invalid_angle.desc"_lang,
                                        {"common.ok"_lang}, false);
                                }
                            }
                            break;
                        }
                        case 11:
                            inst::util::regenerateBackground(true);
                            mainApp->CreateShowDialog("options.appearance.gradient_regen.title"_lang,
                                "options.appearance.gradient_regen.desc"_lang,
                                {"OK"}, false);
                            break;
                        default: break;
                    }
                } else {
                        switch (selectedIndex) {
                        case 0: {
                            std::vector<std::string> themeNames;
                            for (const auto& t : kThemes) themeNames.push_back(t.name);
                            int choice = mainApp->CreateShowDialog("options.appearance.select_theme_dialog.title"_lang,
                                "options.appearance.select_theme_dialog.desc"_lang,
                                themeNames, false);
                            if (choice >= 0 && choice < (int)(sizeof(kThemes)/sizeof(kThemes[0]))) {
                                const auto& t = kThemes[choice];
                                inst::config::colorBackground    = t.background;
                                inst::config::colorTopBar        = t.topBar;
                                inst::config::colorBotBar        = t.botBar;
                                inst::config::colorTileHighlight = t.tileHighlight;
                                inst::config::colorMenuHighlight = t.menuHighlight;
                                inst::config::colorDialogBackground = inst::config::kDefaultColorDialogBackground;
                                inst::config::colorDialogBorder     = inst::config::kDefaultColorDialogBorder;
                                inst::config::gradientColorA     = t.gradA;
                                inst::config::gradientColorB     = t.gradB;
                                inst::config::gradientType       = t.gradType;
                                inst::config::gradientAngle      = t.gradAngle;
                                inst::config::colorTileBase      = "";
                                inst::config::setConfig();
                                inst::util::regenerateBackground(true);
                                mainApp->CreateShowDialog("options.appearance.theme_applied.title"_lang,
                                    "\"" + std::string(t.name) + "\"" + "options.appearance.theme_applied.desc_suffix"_lang,
                                    {"OK"}, false);
                                mainApp->rebuildLayouts();
                            }
                            break;
                        }
                        case 1:
                            this->inCustomTheme = true;
                            this->refreshOptions();
                            break;
                        case 2:
                            mainApp->m_themePage->startManager();
                            break;
                        case 3:
                            inst::config::colorBackground    = inst::config::kDefaultColorBackground;
                            inst::config::colorTopBar        = inst::config::kDefaultColorTopBar;
                            inst::config::colorBotBar        = inst::config::kDefaultColorBotBar;
                            inst::config::colorTileHighlight = inst::config::kDefaultColorTileHighlight;
                            inst::config::colorMenuHighlight = inst::config::kDefaultColorMenuHighlight;
                            inst::config::colorDialogBackground = inst::config::kDefaultColorDialogBackground;
                            inst::config::colorDialogBorder     = inst::config::kDefaultColorDialogBorder;
                            inst::config::gradientColorA     = inst::config::kDefaultGradientColorA;
                            inst::config::gradientColorB     = inst::config::kDefaultGradientColorB;
                            inst::config::gradientType        = inst::config::kDefaultGradientType;
                            inst::config::gradientAngle       = inst::config::kDefaultGradientAngle;
                            inst::config::setConfig();
                            inst::util::regenerateBackground(true);
                            mainApp->CreateShowDialog("options.appearance.theme_reset.title"_lang,
                                "options.appearance.theme_reset.desc"_lang,
                                {"OK"}, false);
                            mainApp->rebuildLayouts();
                            break;
                        default: break;
                    }
                }
            } else {
                switch (selectedIndex) {
                    case 0:
                        languageList = languageStrings;
                        languageList.push_back("options.language.system_language"_lang);
                        rc = inst::ui::mainApp->CreateShowDialog("options.language.title"_lang, "options.language.desc"_lang, languageList, false);
                        if (rc == -1) break;
                        switch (rc) {
                            case 0: inst::config::languageSetting = 1;  break;
                            case 1: inst::config::languageSetting = 0;  break;
                            case 2: inst::config::languageSetting = 2;  break;
                            case 3: inst::config::languageSetting = 3;  break;
                            case 4: inst::config::languageSetting = 4;  break;
                            case 5: inst::config::languageSetting = 14; break;
                            case 6: inst::config::languageSetting = 9;  break;
                            case 7: inst::config::languageSetting = 7;  break;
                            case 8: inst::config::languageSetting = 10; break;
                            case 9: inst::config::languageSetting = 6;  break;
                            case 10:inst::config::languageSetting = 11; break;
                            default:inst::config::languageSetting = 99; break;
                        }
                        inst::config::setConfig();
                        mainApp->FadeOut(); mainApp->Close(); break;
                    case 1: {
                        std::vector<std::string> idOptions = {
                            "Set custom ID", "Generate new random ID", "common.cancel"_lang
                        };
                        int choice = mainApp->CreateShowDialog(
                            "Console ID",
                            "Current ID: " + inst::config::consoleId +
                            "\n\nThis identifies your console to Quark over USB and network. "
                            "It's generated automatically on first boot and\nstays the same "
                            "afterwards, unless you set your own here.\n\n"
                            "Changing it will briefly disconnect USB if it's connected.",
                            idOptions, false);

                        auto applyUsbIdChange = [&]() {
                            quark::usb::Finalize();
                            quark::usb::Initialize();
                            this->refreshOptions();
                        };

                        if (choice == 0) {
                            std::string result = inst::util::softwareKeyboard(
                                "Console ID (letters, numbers, spaces, - _ . only)",
                                inst::config::consoleId,
                                static_cast<int>(inst::config::kConsoleIdMaxLen));
                            if (!result.empty()) {
                                if (quark::SetCustomConsoleId(result)) {
                                    applyUsbIdChange();
                                } else {
                                    mainApp->CreateShowDialog("Invalid Console ID",
                                        "Console IDs can only contain letters, numbers, spaces, "
                                        "hyphens, underscores, and periods, and must be 1-" +
                                        std::to_string(inst::config::kConsoleIdMaxLen) + "\ncharacters long.",
                                        {"common.ok"_lang}, false);
                                }
                            }
                        } else if (choice == 1) {
                            quark::RegenerateConsoleId();
                            applyUsbIdChange();
                        }
                        break;
                    }
                    case 2: {
                        if (inst::util::getIPAddress() == "1.0.0.127") {
                            inst::ui::mainApp->CreateShowDialog("No network", "Connect to a network to check for updates.", {"common.ok"_lang}, true);
                            break;
                        }
                        std::vector<std::string> updateUrl = inst::util::checkForAppUpdate();
                        if (!updateUrl.size()) {
                            mainApp->CreateShowDialog("options.update.title_check_fail"_lang, "options.update.desc_check_fail"_lang, {"common.ok"_lang}, false);
                            break;
                        }
                        this->askToUpdate(updateUrl);
                        break;
                    }
                    case 3:
                        inst::ui::mainApp->CreateShowDialog("options.credits.title"_lang, "options.credits.desc"_lang, {"common.close"_lang}, true); break;
                    case 4: {
                        int choice = mainApp->CreateShowDialog(
                            "options.menu_items.fix_update_flags"_lang,
                            "options.fix_update_flags_dialog.desc"_lang,
                            {"options.fix_update_flags_dialog.confirm_all"_lang,
                             "options.fix_update_flags_dialog.confirm_one"_lang,
                             "common.cancel"_lang}, false);
                        if (choice == 0) {
                            inst::util::initInstallServices();
                            int fixedCount = inst::util::FixAllAvmFloors();
                            inst::util::deinitInstallServices();

                            mainApp->CreateShowDialog(
                                "options.fix_update_flags_done.title"_lang,
                                std::to_string(fixedCount) + "options.fix_update_flags_done.desc_suffix"_lang,
                                {"common.ok"_lang}, false);
                        } else if (choice == 1) {
                            this->startTitleFixPicker();
                        }
                        break;
                    }
                    case 5:
                        this->startTitleManagePicker();
                        break;
                    default: break;
                }
            }
        }
    }
}

namespace inst::ui {

    void optionsPage::focusUpdateCheck() {
        
        this->setSection(2);
        this->menu->SetSelectedIndex(2);
    }
}
