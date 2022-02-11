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

mat4 getBoxInverseTransform(AOBox box) {
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

mat4 getBoxTransform(AOBox box) {
    return inverse(getBoxInverseTransform(box));
}
//#define SLOW_BOX

float getBoxOcclusionArtistic(AOBox box, vec3 pos, vec3 nor) {
    vec3 boxSize = getBoxScale(box);
    mat3 rotation = getBoxRotationMat(box);

    vec3 boxCenter = getBoxTranslation(box);
    vec3 dv = pos - boxCenter;

    vec3 xAxis = rotation * vec3(1.0, 0.0, 0.0);
    vec3 yAxis = rotation * vec3(0.0, 1.0, 0.0);
    vec3 zAxis = rotation * vec3(0.0, 0.0, 1.0);

    float xDist = dot(xAxis, dv);
    float yDist = dot(yAxis, dv);
    float zDist = dot(zAxis, dv);

    xDist = clamp(xDist, -boxSize.x, boxSize.x);
    yDist = clamp(yDist, -boxSize.y, boxSize.y);
    zDist = clamp(zDist, -boxSize.z, boxSize.z);

    vec3 point = boxCenter + (xAxis * xDist) + (yAxis * yDist) + (zAxis * zDist);
    float d = distance(point, pos);

    //return distance(pos, point);
    //vec3 dirToPoint = normalize(point - pos);

    //return 1.0f;
    //return d;
    return clamp(pow(1.0f / 1.0 - d, 5.0), 0.0, 1.0);
    //return distanceToBox;
}

// THIS CODE IS NOT MINE.
// It is taken from https://iquilezles.org/www/articles/boxocclusion/boxocclusion.htm
// It is publicly available for use and is not part of my computing project.
#ifdef SLOW_BOX
float getBoxOcclusionNonClipped(AOBox box, vec3 pos, vec3 nor) {
    vec3 boxSize = getBoxScale(box);
    mat4 transform = getBoxInverseTransform(box);

    vec3 p = (transform*vec4(pos,1.0)).xyz;
	vec3 n = (transform*vec4(nor,0.0)).xyz;

    // 8 verts
    vec3 v0 = normalize(vec3(-1.0,-1.0,-1.0)*boxSize - p);
    vec3 v1 = normalize(vec3( 1.0,-1.0,-1.0)*boxSize - p);
    vec3 v2 = normalize(vec3(-1.0, 1.0,-1.0)*boxSize - p);
    vec3 v3 = normalize(vec3( 1.0, 1.0,-1.0)*boxSize - p);
    vec3 v4 = normalize(vec3(-1.0,-1.0, 1.0)*boxSize - p);
    vec3 v5 = normalize(vec3( 1.0,-1.0, 1.0)*boxSize - p);
    vec3 v6 = normalize(vec3(-1.0, 1.0, 1.0)*boxSize - p);
    vec3 v7 = normalize(vec3( 1.0, 1.0, 1.0)*boxSize - p);

    // 12 edges
    float k02 = dot( n, normalize( cross(v2,v0)) ) * acos( dot(v0,v2) );
    float k23 = dot( n, normalize( cross(v3,v2)) ) * acos( dot(v2,v3) );
    float k31 = dot( n, normalize( cross(v1,v3)) ) * acos( dot(v3,v1) );
    float k10 = dot( n, normalize( cross(v0,v1)) ) * acos( dot(v1,v0) );
    float k45 = dot( n, normalize( cross(v5,v4)) ) * acos( dot(v4,v5) );
    float k57 = dot( n, normalize( cross(v7,v5)) ) * acos( dot(v5,v7) );
    float k76 = dot( n, normalize( cross(v6,v7)) ) * acos( dot(v7,v6) );
    float k37 = dot( n, normalize( cross(v7,v3)) ) * acos( dot(v3,v7) );
    float k64 = dot( n, normalize( cross(v4,v6)) ) * acos( dot(v6,v4) );
    float k51 = dot( n, normalize( cross(v1,v5)) ) * acos( dot(v5,v1) );
    float k04 = dot( n, normalize( cross(v4,v0)) ) * acos( dot(v0,v4) );
    float k62 = dot( n, normalize( cross(v2,v6)) ) * acos( dot(v6,v2) );

    // 6 faces
    float occ = 0.0;
    occ += ( k02 + k23 + k31 + k10) * step( 0.0,  v0.z );
    occ += ( k45 + k57 + k76 + k64) * step( 0.0, -v4.z );
    occ += ( k51 - k31 + k37 - k57) * step( 0.0, -v5.x );
    occ += ( k04 - k64 + k62 - k02) * step( 0.0,  v0.x );
    occ += (-k76 - k37 - k23 - k62) * step( 0.0, -v6.y );
    occ += (-k10 - k51 - k45 - k04) * step( 0.0,  v0.y );

    return occ / 6.283185;
}
#else
float getBoxOcclusionNonClipped(AOBox box, vec3 pos, vec3 nor) {
    vec3 boxSize = getBoxScale(box);
    mat4 transform = getBoxInverseTransform(box);
    
	vec3 p = (transform*vec4(pos,1.0)).xyz;
	vec3 n = (transform*vec4(nor,0.0)).xyz;
    
    // Orient the hexagon based on p
    vec3 f = boxSize * sign(p);
    
    // Make sure the hexagon is always convex
    vec3 s = sign(boxSize - abs(p));
    
    // 6 verts
    vec3 v0 = normalize( vec3( 1.0, 1.0,-1.0)*f - p);
    vec3 v1 = normalize( vec3( 1.0, s.x, s.x)*f - p);
    vec3 v2 = normalize( vec3( 1.0,-1.0, 1.0)*f - p);
    vec3 v3 = normalize( vec3( s.z, s.z, 1.0)*f - p);
    vec3 v4 = normalize( vec3(-1.0, 1.0, 1.0)*f - p);
    vec3 v5 = normalize( vec3( s.y, 1.0, s.y)*f - p);
    
    // 6 edges
    return abs( dot( n, normalize( cross(v0,v1)) ) * acos( dot(v0,v1) ) +
    	    	dot( n, normalize( cross(v1,v2)) ) * acos( dot(v1,v2) ) +
    	    	dot( n, normalize( cross(v2,v3)) ) * acos( dot(v2,v3) ) +
    	    	dot( n, normalize( cross(v3,v4)) ) * acos( dot(v3,v4) ) +
    	    	dot( n, normalize( cross(v4,v5)) ) * acos( dot(v4,v5) ) +
    	    	dot( n, normalize( cross(v5,v0)) ) * acos( dot(v5,v0) ))
            	/ 6.283185;
}
#endif
#endif
