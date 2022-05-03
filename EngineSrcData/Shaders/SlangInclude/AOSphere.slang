import Math;

static const int AOSPHERE_SIZE = 16;
struct AOSphere
{
    float3 Position;
    float Radius;
}

// Adapted from http://iquilezles.org/www/articles/sphereao/sphereao.htm
float getSphereOcclusion(float3 pos, float3 normal, AOSphere sphere) {
    float3 di = sphere.Position - pos;
    float  l  = max(length(di), 0.00001);
    float  nl = dot(normal,di/l);
    float  h  = l/sphere.Radius;
    float  h2 = h*h;
    float  k2 = 1.0 - h2*nl*nl;

    // above/below horizon
    // EXACT: Quilez - http://iquilezles.org/www/articles/sphereao/sphereao.htm
    float res = max(0.0,nl)/h2;

    // intersecting horizon 
    if( k2 > 0.0001 ) {
        #if 1
            // EXACT : Lagarde/de Rousiers - https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
            res = nl*fastAcos(-nl*sqrt( (h2-1.0)/(1.0-nl*nl) )) - sqrt(k2*(h2-1.0));
            res = res/h2 + atan( sqrt(k2/(h2-1.0)));
            res /= 3.141593;
        #else
            // APPROXIMATED : Quilez - http://iquilezles.org/www/articles/sphereao/sphereao.htm
            res = (nl*h+1.0)/h2;
            res = 0.31*res*res;
        #endif
    }

    return min(res, 1.0);
}