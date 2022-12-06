#pragma once
#include "Core/AssetDB.hpp"
#include <string>

namespace worlds
{
    enum class HTextAlign
    {
        Left,
        Middle,
        Right
    };

    struct WorldTextComponent
    {
        bool dirty = true;

        void setText(std::string newText)
        {
            text = newText;
            dirty = true;
        }

        void setTextScale(float newTextScale)
        {
            textScale = newTextScale;
            dirty = true;
        }

        void setFont(AssetID newFont)
        {
            font = newFont;
            dirty = true;
        }

        AssetID font = INVALID_ASSET;
        std::string text;
        float textScale = 0.025f;
        uint32_t idxOffset = 0;
        HTextAlign hAlign = HTextAlign::Middle;
    };
}
