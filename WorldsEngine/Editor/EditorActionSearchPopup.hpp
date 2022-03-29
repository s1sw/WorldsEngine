#pragma once
#include "EditorActions.hpp"
#include "SearchPopup.hpp"

namespace worlds {
    class EditorActionSearchPopup : SearchPopup<uint32_t> {
    public:
        EditorActionSearchPopup(Editor* ed, entt::registry& reg);
        void show();
        void draw();
    protected:
        void candidateSelected(size_t index) override;
        void drawCandidate(size_t index) override;
        void updateCandidates() override;
    private:
        Editor* ed;
        entt::registry& reg;
    };
}