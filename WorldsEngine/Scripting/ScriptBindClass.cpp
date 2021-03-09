#include "WrenVM.hpp"

namespace worlds {
    StaticLinkedList<ScriptBindClass> ScriptBindClass::bindClasses;
    ScriptBindClass::ScriptBindClass() {
        bindClasses.add(this);
    }
}
