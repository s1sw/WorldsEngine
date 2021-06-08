#version 450
layout(std430, binding = 0) buffer PickingBuffer {
    uint doPicking;
    uint objectID;
} pickBuf;

layout (std140, push_constant) uniform PC { 
    uint clearObjId;
    uint doPicking;
};

void main() {
    if (clearObjId == 1) {
        pickBuf.objectID = ~0u;
    }
    pickBuf.doPicking = doPicking;
}
