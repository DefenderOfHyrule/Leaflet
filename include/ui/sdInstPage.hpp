#pragma once
#include "ui/statusBar.hpp"
#include <filesystem>
#include <chrono>
#include <pu/Plutonium>
#include "ui/bottomHint.hpp"

using namespace pu::ui::elm;
namespace inst::ui {
    class sdInstPage : public pu::ui::Layout
    {
        public:
            sdInstPage();
            PU_SMART_CTOR(sdInstPage)
            pu::ui::elm::Menu::Ref menu;
            void startInstall();
            void onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos);
            StatusBar::Ref statusBar;
            TextBlock::Ref pageInfoText;
            void drawMenuItems(bool clearItems, std::filesystem::path ourPath);
        private:
            std::vector<std::filesystem::path> ourDirectories;
            std::vector<std::filesystem::path> ourFiles;
            std::vector<std::filesystem::path> selectedTitles;
            std::filesystem::path currentDir;
            BottomHintTouchState bottomHintTouch;
            std::vector<BottomHintSegment> bottomHintSegments;
            TextBlock::Ref butText;
            Rectangle::Ref infoRect;
            Rectangle::Ref botRect;
            bool touchTapActive = false;
            bool touchTapMoved = false;
            int touchTapStartX = 0;
            int touchTapStartY = 0;
            bool hasLastTap = false;
            int lastTapIndex = -1;
            std::chrono::steady_clock::time_point lastTapTime{};
            void followDirectory();
            void selectNsp(int selectedIndex, bool redraw = true);
    };
}
