#version 450
layout(std430, binding = 0) buffer PickingBuffer {
    uint depth;
    uint objectID;
    uint doPicking;
} pickBuf;

layout (std140, push_constant) uniform PC { 
    uint clearObjId;
    uint doPicking;
};

void main() {
    if (clearObjId == 1) {
        pickBuf.depth = 4294967295;
        pickBuf.objectID = 4294967295;
    }
    pickBuf.doPicking = doPicking;
}
