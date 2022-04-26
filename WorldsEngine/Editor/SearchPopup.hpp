#pragma once
#include <SDL_scancode.h>
#include <Editor/GuiUtil.hpp>
#include <ImGui/imgui.h>
#include <slib/String.hpp>
#include <slib/List.hpp>

namespace worlds {
    template <typename T>
    class SearchPopup {
    public:
        virtual ~SearchPopup() {}
    protected:
        void drawPopup(const char* title);
        virtual void candidateSelected(size_t index) = 0;
        virtual void drawCandidate(size_t index) = 0;
        virtual void updateCandidates() = 0;
        size_t selectedIndex = 0;
        slib::String currentSearchText;
        slib::List<T> currentCandidateList;
        float fadeAlpha = 0.0f;
    };
}