#pragma once
#include <chrono>
#include <atomic>
#include <array>
#include <pu/Plutonium>
#include <pu/ui/extras/extras_Toast.hpp>
#include "ui/bottomHint.hpp"

using namespace pu::ui::elm;
namespace inst::ui {
    class MainPage : public pu::ui::Layout
    {
        public:
            MainPage();
            ~MainPage();
            PU_SMART_CTOR(MainPage)
            void installMenuItem_Click();
            void netInstallMenuItem_Click();
            void usbInstallMenuItem_Click();
            void hddInstallMenuItem_Click();
            void gcInstallMenuItem_Click();
            void backupSaveDataMenuItem_Click();
            void fileBrowserMenuItem_Click();
            void settingsMenuItem_Click();
            void exitMenuItem_Click();
            void onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos);

            void showUpdateBanner(const std::vector<std::string>& updateInfo);
            void simulateUpdate(const std::string& fakeVersion = "99.0.0");

            bool m_isSimulated{false};
            std::vector<std::string> m_pendingUpdateInfo;
            pu::ui::extras::Toast::Ref m_updateToast;
            std::atomic<bool> m_showUpdateToast{false};
            std::chrono::steady_clock::time_point m_toastExpiry{};
            std::atomic<bool> m_triggerUpdate{false};

            TextBlock::Ref appVersionText;
            Rectangle::Ref botRect;
            std::vector<Rectangle::Ref> mainCards;
            Rectangle::Ref mainCardRing;
            std::vector<Image::Ref> mainCardIcons;
            std::vector<TextBlock::Ref> mainCardLabels;
            Rectangle::Ref detailPane;
            Rectangle::Ref detailAccent;
            TextBlock::Ref detailTitle;
            std::vector<TextBlock::Ref> detailLines;
        private:
            bool appletFinished;
            bool updateFinished;
            bool touchActive = false;
            bool touchMoved = false;
            int touchStartX = 0;
            int touchStartY = 0;
            BottomHintTouchState bottomHintTouch;
            std::vector<BottomHintSegment> bottomHintSegments;
            TextBlock::Ref butText;
            Rectangle::Ref backupUserPickerRect;
            TextBlock::Ref backupUserPickerTitle;
            TextBlock::Ref backupUserPickerHint;
            pu::ui::elm::Menu::Ref optionMenu;
            pu::ui::elm::MenuItem::Ref installMenuItem;
            pu::ui::elm::MenuItem::Ref netInstallMenuItem;
            pu::ui::elm::MenuItem::Ref usbInstallMenuItem;
            pu::ui::elm::MenuItem::Ref hddInstallMenuItem;
            pu::ui::elm::MenuItem::Ref gcInstallMenuItem;
            pu::ui::elm::MenuItem::Ref backupSaveDataMenuItem;
            pu::ui::elm::MenuItem::Ref settingsMenuItem;
            pu::ui::elm::MenuItem::Ref exitMenuItem;
            int selectedMainIndex = 0;
            std::array<float, 6> cardAnim{};
            std::array<int, 6> cardLayoutX{};
            std::array<int, 6> cardLayoutY{};
            std::array<int, 6> cardLayoutW{};
            std::array<int, 6> cardLayoutH{};
            std::array<bool, 6> cardLabelHasTwoLines{};
            std::array<std::string, 6> cardLabelNormal{};
            std::array<std::string, 6> cardLabelSelected{};
            pu::ui::Color cardColorNormal;
            pu::ui::Color cardColorSelected;
            pu::ui::Color cardTextColor;
            pu::ui::Color paneTextColor;
            pu::ui::Color paneTextDimColor;
            void layoutCards();
            void updateMainGridSelection();
            void refreshDetailPane();
            int getMainGridIndexFromTouch(int x, int y) const;
            int promptBackupUserSelection(const std::vector<std::string>& userLabels, int preferredIndex);
            void setBackupUserPickerVisible(bool visible);
            void activateSelectedMainItem();
            void showSelectedMainInfo();
    };
}
