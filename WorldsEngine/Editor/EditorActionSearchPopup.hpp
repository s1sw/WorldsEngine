#pragma once
#include "EditorActions.hpp"

namespace worlds {
    class EditorActionSearchPopup {
    public:
        EditorActionSearchPopup(Editor* ed, entt::registry& reg);
        void show();
        void hide();
        void draw();
    private:
        Editor* ed;
        entt::registry& reg;
        slib::String currentSearchText;
        slib::List<uint32_t> currentCandidateList;
        int selectedIndex = 0;
    };
}