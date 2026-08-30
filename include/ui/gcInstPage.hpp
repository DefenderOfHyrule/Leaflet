#pragma once
#include "ui/statusBar.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <pu/Plutonium>
#include "gcInstall.hpp"
#include "ui/bottomHint.hpp"

using namespace pu::ui::elm;
namespace inst::ui {
    class gcInstPage : public pu::ui::Layout {
    public:
        gcInstPage();
        PU_SMART_CTOR(gcInstPage)
        void onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos);
        void updateGamecardState();


    private:
        Rectangle::Ref botRect;
        StatusBar::Ref statusBar;
        TextBlock::Ref butText;

        Rectangle::Ref gcInfoPanel;
        Image::Ref     gcIcon;
        TextBlock::Ref gcTitleText;
        TextBlock::Ref gcAuthorText;
        TextBlock::Ref gcAppIdText;
        TextBlock::Ref gcVersionText;
        TextBlock::Ref gcSizeText;
        TextBlock::Ref gcDlcText;
        TextBlock::Ref gcStatusText;

        // right panel shared menu used for both main view and selection sub-menu
        pu::ui::elm::Menu::Ref menu;

        bool gcInserted      = false;
        bool gcMounted       = false;
        bool inSelectionMenu = false;
        bool inToolsMenu     = false;
        std::vector<inst::gc::GameCardTitle> gcTitles;

        // bit 0 = base, bit 1 = update, bit 2 = DLC
        uint32_t contentMask = 0;

        // counts down after insertion to re-query NS for the display version.
        int versionRefreshTicksRemaining = 0;

        // maps each selection menu item to its contentMask bit.
        std::vector<uint32_t> selectionItemMasks;

        BottomHintTouchState bottomHintTouch;
        std::vector<BottomHintSegment> bottomHintSegments;

        void refreshGamecardInfo();
        void clearGamecardInfo();
        void buildMainMenu();
        void buildSelectionMenu();
        void buildToolsMenu();
        void refreshSelectionItemLabel(int idx);
        void startInstall(int storageChoice);
        void showGamecardDetails();
        void fixUpdatePrompt();
    };
}
