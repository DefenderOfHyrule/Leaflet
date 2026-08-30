#include "ui/bottomHint.hpp"
#include <switch.h>

namespace inst::ui {
    namespace {
        std::vector<std::string> splitSegments(const std::string& text) {
            std::vector<std::string> segments;
            size_t i = 0;
            while (i < text.size()) {
                size_t sep = text.find("    ", i);
                std::string segment;
                if (sep == std::string::npos) {
                    segment = text.substr(i);
                    i = text.size();
                } else {
                    segment = text.substr(i, sep - i);
                    i = sep;
                    while (i < text.size() && text[i] == ' ') {
                        i++;
                    }
                }
                size_t start = segment.find_first_not_of(' ');
                if (start == std::string::npos)
                    continue;
                size_t end = segment.find_last_not_of(' ');
                segments.push_back(segment.substr(start, end - start + 1));
            }
            return segments;
        }

        int measureWidth(const std::string& text, int fontSize) {
            auto measure = pu::ui::elm::TextBlock::New(0, 0, text, fontSize);
            return measure->GetTextWidth();
        }

        std::uint64_t buttonFromGlyph(char32_t glyph) {
            switch (glyph) {
                case 0xE0E0: return HidNpadButton_A;
                case 0xE0E1: return HidNpadButton_B;
                case 0xE0E2: return HidNpadButton_X;
                case 0xE0E3: return HidNpadButton_Y;
                case 0xE0E4: return HidNpadButton_L;
                case 0xE0E5: return HidNpadButton_R;
                case 0xE0E6: return HidNpadButton_ZL;
                case 0xE0E7: return HidNpadButton_ZR;
                case 0xE0C4: return HidNpadButton_StickL;
                case 0xE0C5: return HidNpadButton_StickR;
                case 0xE0E8: return HidNpadButton_StickR;
                case 0xE0E9: return HidNpadButton_StickR;
                case 0xE0EA: return HidNpadButton_StickR;
                case 0xE0EF: return HidNpadButton_Plus;
                case 0xE0F0: return HidNpadButton_Minus;
                case 0xE085: return HidNpadButton_ZL;
                case 0xE086: return HidNpadButton_ZR;
                default: return 0;
            }
        }

        void addUniqueButton(std::vector<std::uint64_t>& buttons, std::uint64_t button) {
            if (!button) return;
            for (auto existing : buttons) {
                if (existing == button) return;
            }
            buttons.push_back(button);
        }

        std::vector<std::uint64_t> extractButtons(const std::string& segment) {
            std::vector<std::uint64_t> buttons;
            size_t i = 0;
            while (i < segment.size()) {
                unsigned char c = static_cast<unsigned char>(segment[i]);
                char32_t cp = 0;
                if (c < 0x80) {
                    cp = c;
                    i += 1;
                } else if ((c & 0xE0) == 0xC0 && i + 1 < segment.size()) {
                    cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(segment[i + 1]) & 0x3F);
                    i += 2;
                } else if ((c & 0xF0) == 0xE0 && i + 2 < segment.size()) {
                    cp = ((c & 0x0F) << 12)
                         | ((static_cast<unsigned char>(segment[i + 1]) & 0x3F) << 6)
                         | (static_cast<unsigned char>(segment[i + 2]) & 0x3F);
                    i += 3;
                } else if ((c & 0xF8) == 0xF0 && i + 3 < segment.size()) {
                    cp = ((c & 0x07) << 18)
                         | ((static_cast<unsigned char>(segment[i + 1]) & 0x3F) << 12)
                         | ((static_cast<unsigned char>(segment[i + 2]) & 0x3F) << 6)
                         | (static_cast<unsigned char>(segment[i + 3]) & 0x3F);
                    i += 4;
                } else {
                    i += 1;
                }

                std::uint64_t button = buttonFromGlyph(cp);
                if (button) addUniqueButton(buttons, button);
            }
            return buttons;
        }
    }

    std::vector<BottomHintSegment> BuildBottomHintSegments(const std::string& text, int startX, int fontSize) {
        std::vector<BottomHintSegment> segments;
        if (text.empty()) return segments;

        const int spacerWidth = measureWidth("    ", fontSize);
        int x = startX;
        for (const auto& segment : splitSegments(text)) {
            int width = measureWidth(segment, fontSize);
            auto buttons = extractButtons(segment);
            std::uint64_t primary = 0;
            std::uint64_t secondary = 0;
            if (!buttons.empty()) {
                primary = buttons[0];
                if (buttons.size() > 1) secondary = buttons[1];
            }
            segments.push_back({x, width, primary, secondary});
            x += width + spacerWidth;
        }
        return segments;
    }

    std::uint64_t FindBottomHintButton(const std::vector<BottomHintSegment>& segments, int touchX) {
        for (const auto& segment : segments) {
            if (touchX < segment.x || touchX > (segment.x + segment.width)) continue;
            if (segment.secondary) {
                int splitX = segment.x + (segment.width / 2);
                return (touchX >= splitX) ? segment.secondary : segment.primary;
            }
            return segment.primary;
        }
        return 0;
    }

    bool DetectBottomHintTap(pu::ui::Touch pos, BottomHintTouchState& state, int topY, int height, int& outX) {
        const int bottomY = topY + height;
        if (!pos.IsEmpty()) {
            const bool inBottom = pos.Y >= topY && pos.Y <= bottomY;
            if (!state.active && inBottom) {
                state.active = true;
                state.moved = false;
                state.startX = pos.X;
                state.startY = pos.Y;
                state.lastX = pos.X;
            } else if (state.active) {
                int dx = pos.X - state.startX;
                int dy = pos.Y - state.startY;
                if (dx < 0) dx = -dx;
                if (dy < 0) dy = -dy;
                if (dx > 12 || dy > 12) {
                    state.moved = true;
                }
                state.lastX = pos.X;
            }
        } else if (state.active) {
            bool tapped = !state.moved;
            outX = state.lastX;
            state.active = false;
            state.moved = false;
            return tapped;
        }
        return false;
    }
}
