#pragma once
#include <string>

namespace worlds {
    enum class HTextAlign {
        Left,
        Middle,
        Right
    };

    struct WorldTextComponent {
        bool dirty = true;

        void setText(std::string newText) {
            text = newText;
            dirty = true;
        }

        void setTextScale(float textScale) {
            textScale = 0.025f;
            dirty = true;
        }

        std::string text;
        float textScale = 0.025f;
        uint32_t idxOffset = 0;
        HTextAlign hAlign = HTextAlign::Middle;
    };
}
