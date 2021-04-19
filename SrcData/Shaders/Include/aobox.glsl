#ifndef AOBOX_H
#define AOBOX_H
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

float getBoxOcclusion(AOBox box, vec3 pos, vec3 nor) {
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
    return abs( dot( n, normalize( cross(v0,v1)) ) * acos( dot(v0,v1) ) +
    	    	dot( n, normalize( cross(v1,v2)) ) * acos( dot(v1,v2) ) +
    	    	dot( n, normalize( cross(v2,v3)) ) * acos( dot(v2,v3) ) +
    	    	dot( n, normalize( cross(v3,v4)) ) * acos( dot(v3,v4) ) +
    	    	dot( n, normalize( cross(v4,v5)) ) * acos( dot(v4,v5) ) +
    	    	dot( n, normalize( cross(v5,v0)) ) * acos( dot(v5,v0) ))
            	/ 6.283185;
}
#endif
