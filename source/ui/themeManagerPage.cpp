#include "ui/themeManagerPage.hpp"
#include "ui/MainApplication.hpp"
#include "util/json.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/lang.hpp"
#include <algorithm>

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {

    extern MainApplication *mainApp;

    themeManagerPage::themeManagerPage() : Layout::Layout() {
        this->SetBackgroundColor(COLOR(inst::config::colorBackground));
        if (std::filesystem::exists(inst::config::appDir + "/background.png"))
            this->SetBackgroundImage(inst::config::appDir + "/background.png");
        else
            this->SetBackgroundImage("romfs:/images/background.jpg");

        const auto botColor  = COLOR(inst::config::colorBotBar);

        m_botRect  = Rectangle::New(0, 660, 1280,  60, botColor);



        m_butText = TextBlock::New(10, 678, "", 20);
        m_butText->SetColor(COLOR("#FFFFFFFF"));

        m_menu = pu::ui::elm::Menu::New(0, 60, 1280, COLOR("#FFFFFF00"), 50, 12);
        m_menu->SetOnFocusColor(COLOR(inst::config::menuHighlightColorCapped()));
        m_menu->SetScrollbarColor(COLOR(inst::config::colorBotBar));

        this->statusBar = StatusBar::New(StatusBar::Mode::Slim, "options.theme_manager.page_title"_lang); this->statusBar->Attach(this);
        this->Add(m_botRect);
        this->Add(m_butText);
        this->Add(m_menu);
    }

    void themeManagerPage::setButtonsText(const std::string& text) {
        m_butText->SetText(text);
        m_bottomHintSegments = BuildBottomHintSegments(text, 10, 20);
    }

    void themeManagerPage::buildThemeMenu() {
        m_themes = inst::config::loadThemes();
        m_menu->ClearItems();

        setButtonsText("options.theme_manager.buttons"_lang);

        for (const auto& t : m_themes) {
            auto item = pu::ui::elm::MenuItem::New(t.name);
            item->SetColor(COLOR("#FFFFFFFF"));
            item->SetIcon("romfs:/images/icons/settings.png");
            m_menu->AddItem(item);
        }

        // add-item at the bottom, same pattern as the network hosts page.
        auto addItem = pu::ui::elm::MenuItem::New("options.theme_manager.export_item"_lang);
        addItem->SetColor(COLOR("#FFFFFFFF"));
        addItem->SetIcon("romfs:/images/icons/plus.png");
        m_menu->AddItem(addItem);

        auto importItem = pu::ui::elm::MenuItem::New("options.theme_manager.import_item"_lang);
        importItem->SetColor(COLOR("#FFFFFFFF"));
        importItem->SetIcon("romfs:/images/icons/micro-sd.png");
        m_menu->AddItem(importItem);

        if (!m_menu->GetItems().empty())
            m_menu->SetSelectedIndex(0);
    }

    void themeManagerPage::exportCurrentTheme() {
        std::string name = inst::util::softwareKeyboard("options.theme_manager.export_dialog.name_prompt"_lang, "My Theme", 32);
        if (name.empty()) return;
        const std::string path = inst::config::exportTheme(name);
        if (!path.empty()) {
            mainApp->CreateShowDialog("options.theme_manager.export_dialog.title"_lang,
                "Saved to:\n" + path + "\n\n"
                "Share this .leaflet.theme file with others! \n"
                "They can place it in " + inst::config::themesDir +
                "\nand apply it from this menu.",
                {"OK"}, false);
            buildThemeMenu();
            const int newIdx = static_cast<int>(m_themes.size()) - 1;
            if (newIdx >= 0)
                m_menu->SetSelectedIndex(newIdx);
        } else {
            mainApp->CreateShowDialog("options.theme_manager.export_failed.title"_lang,
                "options.theme_manager.export_failed.desc"_lang,
                {"OK"}, false);
        }
    }

    void themeManagerPage::deleteTheme(int index) {
        if (index < 0 || index >= (int)m_themes.size()) return;
        const std::string name = m_themes[index].name;
        const int confirm = mainApp->CreateShowDialog(
            "options.theme_manager.delete_dialog.title"_lang,
            "\"" + name + "\"" + "options.theme_manager.delete_dialog.desc_suffix"_lang,
            {"common.ok"_lang, "common.cancel"_lang}, true);
        if (confirm != 0) return;

        if (std::filesystem::exists(inst::config::themesDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(inst::config::themesDir)) {
                if (entry.path().extension() != ".theme") continue;
                if (entry.path().stem().extension() != ".leaflet") continue;
                try {
                    std::ifstream f(entry.path());
                    nlohmann::json j;
                    f >> j;
                    if (j.value("name", "") == name) {
                        f.close();
                        std::filesystem::remove(entry.path());
                        break;
                    }
                } catch (...) {}
            }
        }

        buildThemeMenu();
        const int newCount = static_cast<int>(m_menu->GetItems().size());
        m_menu->SetSelectedIndex(std::min(index, newCount - 1));
    }

    void themeManagerPage::applyTheme(int index) {
        if (index < 0 || index >= (int)m_themes.size()) return;
        inst::config::applyTheme(m_themes[index]);
        inst::config::setConfig();
        inst::util::regenerateBackground(true);
        mainApp->CreateShowDialog("options.theme_manager.theme_applied.title"_lang,
            "\"" + m_themes[index].name + "\"" + "options.theme_manager.theme_applied.desc_suffix"_lang,
            {"common.ok"_lang}, false);
        mainApp->rebuildLayouts(1);
    }

    void themeManagerPage::showInfoDialog() {
        mainApp->CreateShowDialog(
            "options.theme_manager.info_dialog.title"_lang,
            "options.theme_manager.info_dialog.desc_prefix"_lang + inst::config::themesDir +
            "options.theme_manager.info_dialog.desc_suffix"_lang,
            {"common.ok"_lang}, false);
    }

    void themeManagerPage::startImportBrowse() {
        m_importBrowsing = true;
        this->statusBar->SetTitle("options.theme_manager.import_browse.title"_lang);
        setButtonsText("options.theme_manager.import_browse.buttons"_lang);
        drawImportBrowseItems("sdmc:/");
    }

    void themeManagerPage::cancelImportBrowse() {
        m_importBrowsing = false;
        this->statusBar->SetTitle("options.theme_manager.page_title"_lang);
        buildThemeMenu();
    }

    void themeManagerPage::drawImportBrowseItems(std::filesystem::path dir) {
        if (dir.string().empty() || dir.string() == "sdmc:") dir = "sdmc:/";
        m_browseDir = dir;
        m_menu->ClearItems();

        try {
            m_browseDirs  = inst::util::getDirsAtPath(m_browseDir.string());
            m_browseFiles = inst::util::getDirectoryFiles(m_browseDir.string(), {".theme"});
        } catch (...) {
            drawImportBrowseItems(m_browseDir.parent_path());
            return;
        }

        m_browseFiles.erase(std::remove_if(m_browseFiles.begin(), m_browseFiles.end(),
            [](const std::filesystem::path& p) { return p.stem().extension() != ".leaflet"; }),
            m_browseFiles.end());

        if (m_browseDir != "sdmc:/") {
            auto up = pu::ui::elm::MenuItem::New("..");
            up->SetColor(COLOR("#FFFFFFFF"));
            up->SetIcon("romfs:/images/icons/folder-upload.png");
            m_menu->AddItem(up);
        }
        for (auto& d : m_browseDirs) {
            auto item = pu::ui::elm::MenuItem::New(d.filename().string());
            item->SetColor(COLOR("#FFFFFFFF"));
            item->SetIcon("romfs:/images/icons/folder.png");
            m_menu->AddItem(item);
        }
        for (auto& f : m_browseFiles) {
            auto item = pu::ui::elm::MenuItem::New(f.filename().string());
            item->SetColor(COLOR("#FFFFFFFF"));
            item->SetIcon("romfs:/images/icons/settings.png");
            m_menu->AddItem(item);
        }
        if (m_browseDirs.empty() && m_browseFiles.empty()) {
            auto empty = pu::ui::elm::MenuItem::New("options.theme_manager.import_browse.empty"_lang);
            empty->SetColor(COLOR("#FFFFFF99"));
            empty->SetIcon("romfs:/images/icons/settings.png");
            m_menu->AddItem(empty);
        }
        m_menu->SetSelectedIndex(0);
    }

    void themeManagerPage::handleImportBrowseSelect() {
        const int idx = m_menu->GetSelectedIndex();
        if (idx < 0 || idx >= (int)m_menu->GetItems().size()) return;
        const std::string name = m_menu->GetItems()[idx]->GetName();

        if (name == "..") {
            drawImportBrowseItems(m_browseDir.parent_path());
            return;
        }
        if (name == "options.theme_manager.import_browse.empty"_lang) return;

        const int dirCount = (int)m_browseDirs.size();
        const int offset   = (m_browseDir != "sdmc:/") ? 1 : 0;
        const int relIdx   = idx - offset;

        if (relIdx >= 0 && relIdx < dirCount) {
            drawImportBrowseItems(m_browseDirs[relIdx]);
            return;
        }
        const int fileIdx = relIdx - dirCount;
        if (fileIdx >= 0 && fileIdx < (int)m_browseFiles.size())
            importThemeFile(m_browseFiles[fileIdx]);
    }

    void themeManagerPage::importThemeFile(const std::filesystem::path& src) {
        bool valid = false;
        std::string themeName = src.stem().stem().string();
        try {
            std::ifstream f(src);
            nlohmann::json j;
            f >> j;
            valid = true;
            if (j.contains("name") && j["name"].is_string() && !j["name"].get<std::string>().empty())
                themeName = j["name"].get<std::string>();
        } catch (...) { valid = false; }

        if (!valid) {
            mainApp->CreateShowDialog("options.theme_manager.import_invalid.title"_lang,
                "options.theme_manager.import_invalid.desc"_lang, {"common.ok"_lang}, true);
            return;
        }

        std::error_code ec;
        std::filesystem::create_directories(inst::config::themesDir, ec);

        std::filesystem::path dst = std::filesystem::path(inst::config::themesDir) / src.filename();
        if (std::filesystem::exists(dst)) {
            const std::string stem = src.filename().stem().stem().string();
            int n = 2;
            std::filesystem::path candidate;
            do {
                candidate = std::filesystem::path(inst::config::themesDir) / (stem + " (" + std::to_string(n) + ").leaflet.theme");
                n++;
            } while (std::filesystem::exists(candidate));
            dst = candidate;
        }

        ec.clear();
        std::filesystem::rename(src, dst, ec);
        if (ec) {
            ec.clear();
            std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
            if (!ec) std::filesystem::remove(src, ec);
        }

        if (ec) {
            mainApp->CreateShowDialog("options.theme_manager.import_failed.title"_lang,
                "options.theme_manager.import_failed.desc"_lang, {"common.ok"_lang}, true);
            return;
        }

        m_importBrowsing = false;
        this->statusBar->SetTitle("options.theme_manager.page_title"_lang);
        buildThemeMenu();

        int selectIdx = -1;
        for (size_t i = 0; i < m_themes.size(); i++) {
            if (m_themes[i].name == themeName) { selectIdx = (int)i; break; }
        }
        if (selectIdx >= 0) m_menu->SetSelectedIndex(selectIdx);

        mainApp->CreateShowDialog("options.theme_manager.import_done.title"_lang,
            "\"" + themeName + "\"" + "options.theme_manager.import_done.desc_suffix"_lang,
            {"common.ok"_lang}, false);
    }

    void themeManagerPage::startManager() {
        mainApp->LoadLayout(mainApp->m_themePage);
        buildThemeMenu();
    }

    void themeManagerPage::onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos) {
        (void)Up; (void)Held;

        int bottomTapX = 0;
        if (DetectBottomHintTap(Pos, m_bottomHintTouch, 668, 52, bottomTapX))
            Down |= FindBottomHintButton(m_bottomHintSegments, bottomTapX);

        inst::util::playNavigationClickIfNeeded(Down);

        if (m_importBrowsing) {
            if (Down & HidNpadButton_B) {
                cancelImportBrowse();
                return;
            }
            if (Down & HidNpadButton_A) {
                handleImportBrowseSelect();
                return;
            }
            return;
        }

        if (Down & HidNpadButton_B) {
            mainApp->LoadLayout(mainApp->optionspage);
            mainApp->optionspage->setSection(1);
            return;
        }

        const int idx = m_menu->GetSelectedIndex();
        const bool isExportItem   = (idx == (int)m_themes.size());
        const bool isImportItem   = (idx == (int)m_themes.size() + 1);
        const bool isSentinelItem = isExportItem || isImportItem;

        if (Down & HidNpadButton_A) {
            if (isExportItem) exportCurrentTheme();
            else if (isImportItem) startImportBrowse();
            else applyTheme(idx);
            return;
        }

        if (Down & HidNpadButton_Y) {
            exportCurrentTheme();
            return;
        }

        if (Down & HidNpadButton_X) {
            if (isSentinelItem) showInfoDialog();
            else deleteTheme(idx);
            return;
        }
    }

}
