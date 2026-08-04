#include "ui/themeManagerPage.hpp"
#include "ui/MainApplication.hpp"
#include "util/json.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/lang.hpp"

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {

    extern MainApplication *mainApp;

    themeManagerPage::themeManagerPage() : Layout::Layout() {
        const bool oled = inst::config::oledMode;

        if (oled) {
            this->SetBackgroundColor(COLOR("#000000FF"));
        } else {
            this->SetBackgroundColor(COLOR(inst::config::colorBackground));
            if (std::filesystem::exists(inst::config::appDir + "/background.png"))
                this->SetBackgroundImage(inst::config::appDir + "/background.png");
            else
                this->SetBackgroundImage("romfs:/images/background.jpg");
        }

        const auto botColor  = oled ? COLOR("#000000FF") : COLOR(inst::config::colorBotBar);

        m_botRect  = Rectangle::New(0, 660, 1280,  60, botColor);



        m_butText = TextBlock::New(10, 678, "", 20);
        m_butText->SetColor(COLOR("#FFFFFFFF"));

        m_menu = pu::ui::elm::Menu::New(0, 60, 1280, COLOR("#FFFFFF00"), 50, 12);
        if (oled) {
            m_menu->SetOnFocusColor(COLOR("#FFFFFF33"));
            m_menu->SetScrollbarColor(COLOR("#FFFFFF66"));
        } else {
            m_menu->SetOnFocusColor(COLOR(inst::config::colorTileHighlight));
            m_menu->SetScrollbarColor(COLOR(inst::config::colorBotBar));
        }

        this->statusBar = StatusBar::New(StatusBar::Mode::Slim, "Theme Manager"); this->statusBar->Attach(this);
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

        if (Down & HidNpadButton_B) {
            mainApp->LoadLayout(mainApp->optionspage);
            mainApp->optionspage->setSection(1);
            return;
        }

        const int idx = m_menu->GetSelectedIndex();
        const bool isExportItem = (idx == (int)m_themes.size());

        if (Down & HidNpadButton_A) {
            if (isExportItem) exportCurrentTheme();
            else applyTheme(idx);
            return;
        }

        if (Down & HidNpadButton_Y) {
            exportCurrentTheme();
            return;
        }

        if ((Down & HidNpadButton_X) && !isExportItem) {
            deleteTheme(idx);
            return;
        }
    }

}
