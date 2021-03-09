#pragma once

struct WrenVM;

namespace worlds {
    void makeVec3(WrenVM* vm, float x, float y, float z, int slot = 0);
    void throwScriptErr(WrenVM* vm, const char* msg);
}
