#pragma once
#include "ui/statusBar.hpp"
#include "util/title_manage.hpp"
#include <pu/Plutonium>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "ui/bottomHint.hpp"

using namespace pu::ui::elm;
namespace inst::ui {
    class optionsPage : public pu::ui::Layout
    {
        public:
            optionsPage();
            PU_SMART_CTOR(optionsPage)
            void onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos);
            static void askToUpdate(std::vector<std::string> updateInfo);
            void setSection(int section);
            void setInCustomTheme(bool v) { this->inCustomTheme = v; }

            void focusUpdateCheck();

        private:
            BottomHintTouchState bottomHintTouch;
            std::vector<BottomHintSegment> bottomHintSegments;
            TextBlock::Ref butText;
            Rectangle::Ref botRect;
            Rectangle::Ref sideNavRect;
            StatusBar::Ref statusBar;
            pu::ui::elm::Menu::Ref menu;
            bool touchActive = false;
            bool touchMoved = false;
            int touchStartX = 0;
            int touchStartY = 0;
            int touchRegion = 0;
            int selectedSection = 0;
            bool tabsFocused = false;
            bool inCustomTheme = false;
            bool inTitleFixPicker = false;
            struct FixPickerEntry {
                std::uint64_t appId;
                std::string name;
                std::string iconPath;
            };
            std::vector<FixPickerEntry> fixPickerTitles;

            bool inTitleManagePicker = false;
            bool inTitleManageDetail = false;
            std::vector<FixPickerEntry> managePickerTitles;
            std::uint64_t manageDetailAppId = 0;
            std::string manageDetailTitleName;
            std::string manageDetailIconPath;
            std::vector<inst::util::InstalledContentPiece> manageDetailPieces;
            int lockedMenuIndex = 0;
            std::vector<int> sectionMenuIndices;
            std::vector<TextBlock::Ref> sectionTexts;
            std::vector<Rectangle::Ref> sectionHighlights;
            void setSectionNavText();
            void setSettingsMenuText();
            void refreshOptions(bool resetSelection = false);
            void rememberCurrentSectionMenuIndex();
            void restoreSelectedSectionMenuIndex();
            void setSelectedSectionAndRefresh(int newSection);
            int getSectionFromTouch(int x, int y) const;
            std::string getMenuOptionIcon(bool ourBool);
            std::string getMenuLanguage(int ourLangCode);
            void startTitleFixPicker();
            void startTitleManagePicker();
            void enterTitleManageDetail(int titleIdx);
            void refreshTitleManageDetail();
            bool warnIfAtmosphereOverride(std::uint64_t appId);
            void runDisableCheckCountdown(const std::string& langPrefix, bool& configFlag);
            void confirmDisableEmummcCheck();
            void confirmDisableSigPatchCheck();
    };
}
