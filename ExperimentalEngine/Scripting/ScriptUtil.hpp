#pragma once

struct WrenVM;

namespace worlds {
    void makeVec3(WrenVM* vm, float x, float y, float z, int slot = 0);
}
