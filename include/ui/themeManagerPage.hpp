#pragma once
#include "ui/statusBar.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <pu/Plutonium>
#include "util/config.hpp"
#include "ui/bottomHint.hpp"

using namespace pu::ui::elm;

namespace inst::ui {

    class themeManagerPage : public pu::ui::Layout {
    public:
        themeManagerPage();
        PU_SMART_CTOR(themeManagerPage)

        void startManager();
        void onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos);

        StatusBar::Ref statusBar;
        Rectangle::Ref  m_botRect;

    private:
        void buildThemeMenu();
        void exportCurrentTheme();
        void deleteTheme(int index);
        void applyTheme(int index);
        void setButtonsText(const std::string& text);

        void startImportBrowse();
        void cancelImportBrowse();
        void drawImportBrowseItems(std::filesystem::path dir);
        void handleImportBrowseSelect();
        void importThemeFile(const std::filesystem::path& src);
        void showInfoDialog();

        std::vector<inst::config::SavedTheme> m_themes;

        TextBlock::Ref          m_butText;
        pu::ui::elm::Menu::Ref  m_menu;

        BottomHintTouchState            m_bottomHintTouch;
        std::vector<BottomHintSegment>  m_bottomHintSegments;

        bool m_importBrowsing = false;
        std::filesystem::path m_browseDir;
        std::vector<std::filesystem::path> m_browseDirs;
        std::vector<std::filesystem::path> m_browseFiles;
    };

}
