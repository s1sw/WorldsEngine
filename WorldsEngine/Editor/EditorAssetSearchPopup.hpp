#pragma once
#include <slib/String.hpp>
#include <slib/List.hpp>

namespace worlds {
    class Editor;
    class EditorAssetSearchPopup {
    public:
        EditorAssetSearchPopup(Editor* ed);
        void show();
        void draw();
    private:
        Editor* editor;
        slib::String currentSearchText;
        slib::List<uint32_t> currentCandidateList;
        int selectedIndex = 0;
    };
}