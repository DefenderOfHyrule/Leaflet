#include <algorithm>

extern char** __system_argv; // path of the running NRO
#include <filesystem>
#include <stdexcept>
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
#include "ui/instPage.hpp"
#include "ui/bottomHint.hpp"

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {
    extern MainApplication *mainApp;

    std::vector<std::string> languageStrings = {"English", "日本語", "Français", "Deutsch", "Italiano", "Español", "Português", "한국어", "Русский", "簡体中文","繁體中文"};

    struct ThemePreset {
        const char* name;
        const char* background;
        const char* topBar;
        const char* botBar;
        const char* tileHighlight;
        const char* gradA;
        const char* gradB;
    };
    
    // generic ass theme names, courtesy of google
    static const ThemePreset kThemes[] = {
        { "Leaflet",          "#670000FF", "#8D00FFBF", "#17090980", "#FF3B00A0", "#FF00FFFF", "#00FFFFFF" },
        { "Midnight Blue",    "#001840FF", "#0A2A6EBF", "#030D2280", "#4488FFA0", "#4488FFFF", "#00CCFFFF" },
        { "Forest",           "#0A2A00FF", "#1A5200BF", "#0A1A0080", "#44FF88A0", "#44FF88FF", "#00FF44FF" },
        { "Obsidian",         "#0A0A0AFF", "#1A1A1ABF", "#0F0F0F80", "#888888A0", "#888888FF", "#444444FF" },
        { "Sakura",           "#2A0018FF", "#6E0A3ABF", "#220A1880", "#FF88CCA0", "#FF88CCFF", "#FF44AAFF" },
        { "Sunset",           "#2A1400FF", "#8A3A00BF", "#1A0E0080", "#FF8844A0", "#FF8844FF", "#FF4400FF" },
        { "Arctic",           "#001A2AFF", "#003A5ABF", "#00111A80", "#88CCFFA0", "#88CCFFFF", "#44AAFFFF" },
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
        if (inst::config::oledMode) {
            this->SetBackgroundColor(COLOR("#000000FF"));
        } else {
            this->SetBackgroundColor(COLOR(inst::config::colorBackground));
            if (std::filesystem::exists(inst::config::appDir + "/background.png")) this->SetBackgroundImage(inst::config::appDir + "/background.png");
            else this->SetBackgroundImage("romfs:/images/background.jpg");
        }
        const auto botColor  = inst::config::oledMode ? COLOR("#000000FF") : COLOR(inst::config::colorBotBar);
        this->botRect     = Rectangle::New(0, 660, 1280, 60, botColor);
        this->sideNavRect = Rectangle::New(0, 46, 300, 613, inst::config::oledMode ? COLOR("#FFFFFF18") : COLOR(inst::config::colorBotBar));
        const std::string optionsHintText = " Select/Change     Section     Back";
        this->butText = TextBlock::New(10, 678, optionsHintText, 20);
        this->butText->SetColor(COLOR("#FFFFFFFF"));
        this->bottomHintSegments = BuildBottomHintSegments(optionsHintText, 10, 20);
        this->menu = pu::ui::elm::Menu::New(301, 46, 979, COLOR("#FFFFFF00"), 72, (612 / 72), 20);
        if (inst::config::oledMode) {
            this->menu->SetOnFocusColor(COLOR("#FFFFFF33"));
            this->menu->SetScrollbarColor(COLOR("#FFFFFF66"));
        } else {
            this->menu->SetOnFocusColor(COLOR(inst::config::colorTileHighlight));
            this->menu->SetScrollbarColor(COLOR(inst::config::colorBotBar));
        }
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
                    ? (inst::config::oledMode ? COLOR("#FFFFFF55") : COLOR("#FFFFFF66"))
                    : (inst::config::oledMode ? COLOR("#FFFFFF33") : COLOR("#FFFFFF40")))
                : COLOR("#FFFFFF00"));
            this->sectionTexts[i]->SetText(sectionLabels[i]);
            this->sectionTexts[i]->SetColor(selected ? COLOR("#FFFFFFFF") : (this->tabsFocused ? COLOR("#FFFFFFCC") : COLOR("#FFFFFF99")));
        }
        if (inst::config::oledMode)
            this->menu->SetOnFocusColor(this->tabsFocused ? COLOR("#FFFFFF18") : COLOR("#FFFFFF66"));
        else
            this->menu->SetOnFocusColor(this->tabsFocused ? COLOR("#00000022") : COLOR("#00000070"));
    }

    void optionsPage::setSettingsMenuText() {
        this->menu->ClearItems();
        auto addItem = [this](const std::string &label, bool toggle, bool value) {
            auto item = pu::ui::elm::MenuItem::New(label);
            item->SetColor(COLOR("#FFFFFFFF"));
            if (toggle) item->SetIcon(this->getMenuOptionIcon(value));
            this->menu->AddItem(item);
        };

        if (this->selectedSection == 0) {
            addItem("options.menu_items.ignore_firm"_lang,  true,  inst::config::ignoreReqVers);
            addItem("Verbose install logs",                  true,  inst::config::verboseInstallLogging);
            addItem("options.menu_items.ask_delete"_lang,   true,  inst::config::deletePrompt);
            addItem("options.menu_items.sound"_lang,         true,  inst::config::soundEnabled);
            addItem("options.menu_items.oled"_lang,          true,  inst::config::oledMode);
            addItem("Auto-skip reinstall prompts",              true,  inst::config::autoSkipReinstall);
            addItem("Cancel queue on install error",            true,  inst::config::cancelQueueOnError);
        } else if (this->selectedSection == 1) {
            if (this->inCustomTheme) {
                
                addItem("options.appearance.bg_color"_lang + ":  "  + inst::config::colorBackground,    false, false);
                addItem("options.appearance.panel_color"_lang + ":  "        + inst::config::colorTopBar,        false, false);
                addItem("options.appearance.bar_color"_lang + ":  "          + inst::config::colorBotBar,        false, false);
                addItem("options.appearance.highlight_color"_lang + ":  "    + inst::config::colorTileHighlight, false, false);
                addItem("options.appearance.gradient_start"_lang + ":  "     + inst::config::gradientColorA,     false, false);
                addItem("options.appearance.gradient_end"_lang + ":  "       + inst::config::gradientColorB,     false, false);
                addItem("options.appearance.regen_gradient"_lang,                            false, false);
            } else {
                addItem("options.appearance.select_preset"_lang, false, false);
                addItem("options.appearance.custom_theme"_lang,  false, false);
                addItem("options.appearance.import_theme"_lang,  false, false);
                addItem("options.appearance.reset_theme"_lang,   false, false);
            }
        } else {
            addItem("options.menu_items.language"_lang + this->getMenuLanguage(inst::config::languageSetting), false, false);
            addItem("options.menu_items.check_update"_lang, false, false);
            addItem("options.menu_items.credits"_lang,       false, false);
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
                        inst::config::oledMode = !inst::config::oledMode;
                        inst::config::setConfig();
                        mainApp->rebuildLayouts(0);
                        break;
                    case 5:
                        inst::config::autoSkipReinstall = !inst::config::autoSkipReinstall;
                        inst::config::setConfig(); this->refreshOptions(); break;
                    case 6:
                        inst::config::cancelQueueOnError = !inst::config::cancelQueueOnError;
                        inst::config::setConfig(); this->refreshOptions(); break;
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
                        "options.appearance.color_applied.desc_suffix"_lang,
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
                        case 4: {
                            auto c = promptColor("options.appearance.gradient_start"_lang, inst::config::gradientColorA);
                            if (!c.empty()) {
                                inst::config::gradientColorA = c;
                                inst::config::setConfig();
                                mainApp->CreateShowDialog("options.appearance.gradient_start_set.title"_lang,
                                    "options.appearance.gradient_start_set.desc_suffix"_lang,
                                    {"OK"}, false);
                                this->refreshOptions();
                            }
                            break;
                        }
                        case 5: {
                            auto c = promptColor("options.appearance.gradient_end"_lang, inst::config::gradientColorB);
                            if (!c.empty()) {
                                inst::config::gradientColorB = c;
                                inst::config::setConfig();
                                mainApp->CreateShowDialog("options.appearance.gradient_end_set.title"_lang,
                                    "options.appearance.gradient_end_set.desc_suffix"_lang,
                                    {"OK"}, false);
                                this->refreshOptions();
                            }
                            break;
                        }
                        case 6:
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
                                inst::config::gradientColorA     = t.gradA;
                                inst::config::gradientColorB     = t.gradB;
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
                            inst::config::gradientColorA     = inst::config::kDefaultGradientColorA;
                            inst::config::gradientColorB     = inst::config::kDefaultGradientColorB;
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
                    case 2:
                        inst::ui::mainApp->CreateShowDialog("options.credits.title"_lang, "options.credits.desc"_lang, {"common.close"_lang}, true); break;
                    default: break;
                }
            }
        }
    }
}

namespace inst::ui {

    void optionsPage::focusUpdateCheck() {
        
        this->setSection(2);
        this->menu->SetSelectedIndex(1);
    }
}
