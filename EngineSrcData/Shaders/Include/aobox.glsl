#ifndef AOBOX_H
#define AOBOX_H
#include <math.glsl>

struct AOBox {
    vec4 pack0;
    vec4 pack1;
    vec4 pack2;
    vec4 pack3;
};

mat3 getBoxRotationMat(AOBox box) {
    return mat3(
            box.pack0.x, box.pack0.y, box.pack0.z,
            box.pack0.w, box.pack1.x, box.pack1.y,
            box.pack1.z, box.pack1.w, box.pack2.x
            );
}

vec3 getBoxTranslation(AOBox box) {
    return vec3(box.pack2.y, box.pack2.z, box.pack2.w);
}

vec3 getBoxScale(AOBox box) {
    return vec3(box.pack3.xyz);
}

mat4 getBoxTransform(AOBox box) {
    mat4 rot = mat4(getBoxRotationMat(box));
    rot[3][3] = 1.0;
    vec3 translation = getBoxTranslation(box);

    mat4 translationMatrix = mat4(
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            translation.x, translation.y, translation.z, 1.0
            );

    return translationMatrix * rot;
}

float getBoxOcclusionNonClipped(AOBox box, vec3 pos, vec3 nor) {
    mat4 txx = getBoxTransform(box);
    vec3 rad = getBoxScale(box);
    vec3 p = (txx*vec4(pos,1.0)).xyz;
    vec3 n = (txx*vec4(nor,0.0)).xyz;

    // Orient the hexagon based on p
    vec3 f = rad * sign(p);

    // Make sure the hexagon is always convex
    vec3 s = sign(rad - abs(p));

    // 6 verts
    vec3 v0 = normalize( vec3( 1.0, 1.0,-1.0)*f - p);
    vec3 v1 = normalize( vec3( 1.0, s.x, s.x)*f - p);
    vec3 v2 = normalize( vec3( 1.0,-1.0, 1.0)*f - p);
    vec3 v3 = normalize( vec3( s.z, s.z, 1.0)*f - p);
    vec3 v4 = normalize( vec3(-1.0, 1.0, 1.0)*f - p);
    vec3 v5 = normalize( vec3( s.y, 1.0, s.y)*f - p);

    // 6 edges
    return abs( dot( n, normalize( cross(v0,v1)) ) * safeAcos( dot(v0,v1) ) +
            dot( n, normalize( cross(v1,v2)) ) * safeAcos( dot(v1,v2) ) +
            dot( n, normalize( cross(v2,v3)) ) * safeAcos( dot(v2,v3) ) +
            dot( n, normalize( cross(v3,v4)) ) * safeAcos( dot(v3,v4) ) +
            dot( n, normalize( cross(v4,v5)) ) * safeAcos( dot(v4,v5) ) +
            dot( n, normalize( cross(v5,v0)) ) * safeAcos( dot(v5,v0) ))
        / 6.283185;
}

vec3 clip( in vec3 a, in vec3 b, in vec4 p )
{
    return a - (b-a)*(p.w + dot(p.xyz,a))/dot(p.xyz,(b-a));
}


// fully visible front facing Triangle occlusion
float ftriOcclusion( in vec3 pos, in vec3 nor, in vec3 v0, in vec3 v1, in vec3 v2 )
{
    vec3 a = normalize( v0 - pos );
    vec3 b = normalize( v1 - pos );
    vec3 c = normalize( v2 - pos );

    return (dot( nor, normalize( cross(a,b)) ) * safeAcos( dot(a,b) ) +
            dot( nor, normalize( cross(b,c)) ) * safeAcos( dot(b,c) ) +
            dot( nor, normalize( cross(c,a)) ) * safeAcos( dot(c,a) ) ) / 6.2831;
}


// fully visible front acing Quad occlusion
float fquadOcclusion( in vec3 pos, in vec3 nor, in vec3 v0, in vec3 v1, in vec3 v2, in vec3 v3 )
{
    vec3 a = normalize( v0 - pos );
    vec3 b = normalize( v1 - pos );
    vec3 c = normalize( v2 - pos );
    vec3 d = normalize( v3 - pos );

    return (dot( nor, normalize( cross(a,b)) ) * safeAcos( dot(a,b) ) +
            dot( nor, normalize( cross(b,c)) ) * safeAcos( dot(b,c) ) +
            dot( nor, normalize( cross(c,d)) ) * safeAcos( dot(c,d) ) +
            dot( nor, normalize( cross(d,a)) ) * safeAcos( dot(d,a) ) ) / 6.2831;
}

// partially or fully visible, front or back facing Triangle occlusion
float triOcclusion( in vec3 pos, in vec3 nor, in vec3 v0, in vec3 v1, in vec3 v2, in vec4 plane )
{
    if( dot( v0-pos, cross(v1-v0,v2-v0) ) < 0.0 ) return 0.0;  // back facing

    float s0 = dot( vec4(v0,1.0), plane );
    float s1 = dot( vec4(v1,1.0), plane );
    float s2 = dot( vec4(v2,1.0), plane );

    float sn = sign(s0) + sign(s1) + sign(s2);

    vec3 c0 = clip( v0, v1, plane );
    vec3 c1 = clip( v1, v2, plane );
    vec3 c2 = clip( v2, v0, plane );

    // 3 (all) vertices above horizon
    if( sn>2.0 )
    {
        return ftriOcclusion(  pos, nor, v0, v1, v2 );
    }
    // 2 vertices above horizon
    else if( sn>0.0 )
    {
        vec3 pa, pb, pc, pd;
        if( s0<0.0 )  { pa = c0; pb = v1; pc = v2; pd = c2; }
        else  if( s1<0.0 )  { pa = c1; pb = v2; pc = v0; pd = c0; }
        else/*if( s2<0.0 )*/{ pa = c2; pb = v0; pc = v1; pd = c1; }
        return fquadOcclusion( pos, nor, pa, pb, pc, pd );
    }
    // 1 vertex aboce horizon
    else if( sn>-2.0 )
    {
        vec3 pa, pb, pc;
        if( s0>0.0 )   { pa = c2; pb = v0; pc = c0; }
        else  if( s1>0.0 )   { pa = c0; pb = v1; pc = c1; }
        else/*if( s2>0.0 )*/ { pa = c1; pb = v2; pc = c2; }
        return ftriOcclusion(  pos, nor, pa, pb, pc );
    }
    // zero (no) vertices above horizon

    return 0.0;
}

// Box occlusion (if fully visible)
float getBoxOcclusion(AOBox box, vec3 pos, vec3 nor) {
    mat4 txx = getBoxTransform(box);
    vec3 rad = getBoxScale(box);

    vec3 p = (txx*vec4(pos,1.0)).xyz;
    vec3 n = (txx*vec4(nor,0.0)).xyz;
    vec4 w = vec4( n, -dot(n,p) ); // clipping plane

    // 8 verts
    vec3 v0 = vec3(-1.0,-1.0,-1.0)*rad;
    vec3 v1 = vec3( 1.0,-1.0,-1.0)*rad;
    vec3 v2 = vec3(-1.0, 1.0,-1.0)*rad;
    vec3 v3 = vec3( 1.0, 1.0,-1.0)*rad;
    vec3 v4 = vec3(-1.0,-1.0, 1.0)*rad;
    vec3 v5 = vec3( 1.0,-1.0, 1.0)*rad;
    vec3 v6 = vec3(-1.0, 1.0, 1.0)*rad;
    vec3 v7 = vec3( 1.0, 1.0, 1.0)*rad;


    // 6 faces
    float occ = 0.0;
    occ += triOcclusion( p, n, v0, v2, v3, w );
    occ += triOcclusion( p, n, v0, v3, v1, w );

    occ += triOcclusion( p, n, v4, v5, v7, w );
    occ += triOcclusion( p, n, v4, v7, v6, w );

    occ += triOcclusion( p, n, v5, v1, v3, w );
    occ += triOcclusion( p, n, v5, v3, v7, w );

    occ += triOcclusion( p, n, v0, v4, v6, w );
    occ += triOcclusion( p, n, v0, v6, v2, w );

    occ += triOcclusion( p, n, v6, v7, v3, w );
    occ += triOcclusion( p, n, v6, v3, v2, w );

    occ += triOcclusion( p, n, v0, v1, v5, w );
    occ += triOcclusion( p, n, v0, v5, v4, w );

    return occ;
}
#endif
