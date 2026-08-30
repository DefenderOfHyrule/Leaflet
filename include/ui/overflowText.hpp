#pragma once

#include <pu/Plutonium>
#include <string>
#include <vector>

namespace inst::ui {

// reusable single-line text presenter with:
// clipped idle state + right fade hint when overflowing
// selected-only marquee scrolling (pause/scroll/fade/reset loop)
// intended to be drop-in for UI labels that can overflow localized text.
class OverflowText {
    public:
        OverflowText(int fontSize, pu::ui::Color textColor);
        PU_SMART_CTOR(OverflowText)

        static std::string NormalizeSingleLineText(const std::string& text);
        static std::string ClipSingleLineText(const std::string& text, pu::ui::elm::TextBlock::Ref probeText, int width, int height, bool* overflow = nullptr);
        static std::string ClipSingleLinePrefixWithSuffix(const std::string& prefix, const std::string& suffix, pu::ui::elm::TextBlock::Ref probeText, int width, int height, bool* overflow = nullptr);

        void Attach(pu::ui::Layout* layout);
        void SetBounds(int x, int y, int width, int height);
        void SetBackgroundColor(pu::ui::Color color);
        void SetText(const std::string& text);
        void SetSelected(bool selected, bool forceReset = false);
        void SetVisible(bool visible);
        void Update(bool forceReset = false);
        bool IsOverflowing() const;

    private:
        pu::ui::elm::TextBlock::Ref baseText;
        pu::ui::elm::TextBlock::Ref probeText;
        pu::ui::elm::TextBlock::Ref marqueeText;
        pu::ui::elm::Element::Ref clipBegin;
        pu::ui::elm::Element::Ref clipEnd;
        pu::ui::elm::Rectangle::Ref fadeHint0;
        pu::ui::elm::Rectangle::Ref fadeHint1;
        pu::ui::elm::Rectangle::Ref fadeHint2;
        pu::ui::elm::Rectangle::Ref marqueeFadeRect;

        std::string fullText;
        std::string clippedText;
        pu::ui::Color textColor;
        pu::ui::Color backgroundColor;
        int fontSize = 22;
        int singleLineHeight = 1;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool attached = false;
        bool visible = true;
        bool selected = false;
        bool overflowing = false;
        bool clipEnabled = false;
        int clipX = 0;
        int clipY = 0;
        int clipW = 0;
        int clipH = 0;
        int marqueeOffset = 0;
        int marqueeMaxOffset = 0;
        u64 marqueeLastTick = 0;
        u64 marqueePauseUntilTick = 0;
        u64 marqueeEndPauseUntilTick = 0;
        u64 marqueeSpeedRemainder = 0;
        u64 marqueeFadeStartTick = 0;
        int marqueePhase = 0;
        int marqueeFadeAlpha = 0;

        static std::vector<std::size_t> BuildUtf8Boundaries(const std::string& text);
        std::string buildClippedText(const std::string& text, bool& overflow) const;
        void updateStaticLabel();
        void updateFadeHintRects();
        void hideMarquee(bool resetState);
};

}
