#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <pu/Plutonium>

namespace inst::ui {
    struct BottomHintSegment {
        int x;
        int width;
        std::uint64_t primary;
        std::uint64_t secondary;
    };

    struct BottomHintTouchState {
        bool active = false;
        bool moved = false;
        int startX = 0;
        int startY = 0;
        int lastX = 0;
    };

    std::vector<BottomHintSegment> BuildBottomHintSegments(const std::string& text, int startX, int fontSize);
    std::uint64_t FindBottomHintButton(const std::vector<BottomHintSegment>& segments, int touchX);
    bool DetectBottomHintTap(pu::ui::Touch pos, BottomHintTouchState& state, int topY, int height, int& outX);
}
