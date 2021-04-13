#ifndef SHADERCOMMS_GLSL
#define SHADERCOMMS_GLSL

#ifdef FRAGMENT
#define VARYING(type, name) in type in##name
#endif

#ifdef VERTEX
#define VARYING(type, name) out type out##name
#endif

#endif
