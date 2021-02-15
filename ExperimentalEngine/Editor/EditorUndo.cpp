#include "Editor.hpp"
#include "../Serialization/SceneSerialization.hpp"

namespace worlds {
    void EditorUndo::pushState() {
        
    }

    void EditorUndo::undo() {
    }

    void EditorUndo::redo() {
    }

    void EditorUndo::setMaxStackSize(uint32_t max) {
        maxStackSize = max;
    }

    void EditorUndo::removeEnd() {
        if (undoStack.size() <= maxStackSize) return;

        for (size_t i = maxStackSize; i < undoStack.size(); i++) {
            undoStack.pop_back();
        }
    }
}
