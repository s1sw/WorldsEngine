#ifndef CUBEMAP_H
#define CUBEMAP_H

const int CUBEMAP_FLAG_PARALLAX = 1;

struct Cubemap {
    vec3 extent;
    uint texture;
    vec3 position;
    uint flags;
};
#endif