#pragma once
#include "ui/statusBar.hpp"
#include <cstdint>
#include <chrono>
#include <pu/Plutonium>
#include "ui/bottomHint.hpp"

using namespace pu::ui::elm;
namespace inst::ui {
    class instPage : public pu::ui::Layout
    {
        public:
            instPage();
            PU_SMART_CTOR(instPage)
            void onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos);
            StatusBar::Ref statusBar;
            TextBlock::Ref pageInfoText;
            TextBlock::Ref installInfoText;
            pu::ui::elm::Rectangle::Ref installPanel;
            pu::ui::elm::ProgressBar::Ref installBar;
            Image::Ref leafImage;
            Image::Ref installIconImage;
            TextBlock::Ref hintText;
            TextBlock::Ref consoleIdText;
            int consoleIdReservedW = 0;
            TextBlock::Ref progressText;
            TextBlock::Ref progressDetailText;
            static void setTopInstInfoText(std::string ourText);
            static void setInstInfoText(std::string ourText);
            static void setInstBarPerc(double ourPercent);
            static void setProgressDetailText(const std::string& ourText);
            static void clearProgressDetailText();
            static void setInstallIconFromTitleId(u64 titleId);
            static void setInstallIcon(const std::string& imagePath);
            static void setInstallIconData(const void* imageData, std::uint32_t imageSize);
            static void clearInstallIcon();
            static void loadMainMenu();
            static void loadInstallScreen();
            static void requestInstallCancel();
            static bool isInstallCancelRequested();
            static void clearInstallCancel();
        private:
            bool touchWakeActive = false;
            Rectangle::Ref infoRect;
            Rectangle::Ref botRect;
            BottomHintTouchState bottomHintTouch;
            std::vector<BottomHintSegment> bottomHintSegments;
    };
}
