#pragma once
#include "ui/statusBar.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <pu/Plutonium>
#include "ui/bottomHint.hpp"

namespace inst::ui {
    class fileBrowserPage : public pu::ui::Layout
    {
        public:
            fileBrowserPage();
            PU_SMART_CTOR(fileBrowserPage)
            void onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos);
            void openDriveMenu();

            pu::ui::elm::Menu::Ref menu;
            StatusBar::Ref statusBar;
            pu::ui::elm::TextBlock::Ref pageInfoText;
            pu::ui::elm::Rectangle::Ref botRect;

        private:
            void drawMenuItems(std::filesystem::path path);
            void navigateUp();
            void tryArchiveBit(int index);
            void tryCopy(int index);
            void tryPaste();
            void tryDelete(int index);
            std::filesystem::path selectedPath(int index) const;
            bool isDirectoryEntry(int index) const;
            int  directoryIndexFor(int index) const;

            std::filesystem::path currentDir;
            std::filesystem::path rootDir;
            std::vector<std::filesystem::path> ourDirectories;
            std::vector<std::filesystem::path> ourFiles;
            std::filesystem::path clipboardPath;
            bool clipboardHasItem = false;

            pu::ui::elm::TextBlock::Ref butText;
            std::vector<BottomHintSegment> bottomHintSegments;
            BottomHintTouchState bottomHintTouch;

            pu::ui::elm::Rectangle::Ref copyOverlayBg;
            pu::ui::elm::TextBlock::Ref copyOverlayText;
            pu::ui::elm::ProgressBar::Ref copyOverlayBar;
            void showCopyOverlay(bool visible);
            bool deleteWithProgress(const std::filesystem::path& target);
            void updateCopyOverlay(const std::string& text, double progress);
    };
}
