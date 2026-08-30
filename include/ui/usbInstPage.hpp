#pragma once
#include "ui/statusBar.hpp"
#include <chrono>
#include <string>
#include <vector>
#include <pu/Plutonium>
#include "ui/bottomHint.hpp"

using namespace pu::ui::elm;

namespace inst::ui {

    class usbInstPage : public pu::ui::Layout {
    public:
        usbInstPage();
        PU_SMART_CTOR(usbInstPage)

        void startUsb();
        void startInstall();
        void onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos);

        StatusBar::Ref statusBar;
        TextBlock::Ref  pageInfoText;
        Rectangle::Ref          m_infoRect;
        Rectangle::Ref          m_botRect;

    private:
        std::vector<std::string> m_menuEntries;
        std::vector<std::string> m_menuPaths;
        std::vector<bool>        m_isDirectory;
        std::vector<std::string> m_selectedPaths;
        std::string              m_currentPath;
        std::vector<std::string> m_pathStack;

        bool        m_touchTapActive  = false;
        bool        m_touchTapMoved   = false;
        int         m_touchTapStartX  = 0;
        int         m_touchTapStartY  = 0;
        bool        m_hasLastTap      = false;
        int         m_lastTapIndex    = -1;
        std::chrono::steady_clock::time_point m_lastTapTime{};

        TextBlock::Ref          m_butText;
        TextBlock::Ref          m_consoleIdText;
        int m_consoleIdReservedW = 0;
        pu::ui::elm::Menu::Ref  m_menu;
        Image::Ref              m_infoImage;

        BottomHintTouchState            m_bottomHintTouch;
        std::vector<BottomHintSegment>  m_bottomHintSegments;

        void setButtonsText(const std::string &text);
        void drawMenuItems(bool clearSelection);
        void toggleSelection(int index);
        void navigateTo(const std::string &remotePath, bool pushHistory);
        void navigateBack();
        void refreshCurrentDirectory();
        void buildRootList();
        bool isDirectory(int index) const;
    };

}
