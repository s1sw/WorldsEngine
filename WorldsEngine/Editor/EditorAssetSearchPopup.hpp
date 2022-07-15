#pragma once
#include "SearchPopup.hpp"

namespace worlds
{
    typedef uint32_t AssetID;
    class Editor;
    class EditorAssetSearchPopup : SearchPopup<AssetID>
    {
      public:
        EditorAssetSearchPopup(Editor* ed);
        void show();
        void draw();

      protected:
        void candidateSelected(size_t index) override;
        void drawCandidate(size_t index) override;
        void updateCandidates() override;

      private:
        Editor* editor;
    };
}