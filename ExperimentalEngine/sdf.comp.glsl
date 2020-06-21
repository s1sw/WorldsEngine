#version 450
layout (binding = 0, rgba8) uniform writeonly image2D resultImage;
layout (binding = 1) uniform sampler2D depthImage;
layout (binding = 2) uniform sampler2D polyImage;
layout (local_size_x = 16, local_size_y = 16) in;

layout (binding = 3) uniform Settings {
	vec2 resolution;
};

layout (push_constant) uniform PC {
	vec4 camPos;
	vec4 camRot;
};

const int MAX_MARCHING_STEPS = 255;
const float MIN_DIST = 0.01;
const float MAX_DIST = 2500.0;
const float EPSILON = 0.0001;
const vec4 AMBIENT_COLOR = vec4(0.529, 0.808, 0.98, 1.0);

float depth2dist(float depth, float near, float far)
{
	return (depth * far) + near;
}

float plane( vec3 p)
{
  return p.y;
}

float smin( float a, float b)
{
    float h = clamp( 0.5+0.5*(b-a)/0.1, 0.0, 1.0 );
    return mix( b, a, h ) - 0.1*h*(1.0-h);
}

float roundBox( vec3 p, vec3 b, float r )
{
  return length(max(abs(p)-b,0.0))-r;
}

float sphere(vec3 spherePos, float sphereSize, vec3 evalPos)
{
    return length(evalPos - spherePos) - sphereSize;
}

vec3 qtransform( vec4 q, vec3 v )
{
	vec3 u = q.xyz;
	float s = q.w;
	return 2.0 * dot(u, v) * u
		 + (s * s - dot(u, u)) * v
		 + 2.0 * s * cross(u, v);
} 

float sceneSDFActual(vec3 pos)
{
	float field = sphere(vec3(0.0, 0.0, 0.0), 1.0, pos);
    //field = min(field, sphere(vec3(2.2, 0.0, 0.0), 1.0, pos));
    field = min(field, roundBox(pos, vec3(1.35,0.5,0.5), 0.2));
    return field;
}

float opRep( in vec3 p, in vec3 c )
{
    vec3 q = mod(p+0.5*c,c)-0.5*c;
    return sceneSDFActual(q);
}

float sceneSDF(vec3 pos)
{
    return opRep(qtransform(camRot, pos) + camPos.xyz, vec3(15.0));
}

float calcAO( in vec3 pos, in vec3 nor )
{
	float occ = 0.0;
    float sca = 1.0;
    for( int i=0; i<5; i++ )
    {
        float hr = 0.01 + 0.12*float(i)/4.0;
        vec3 aopos =  nor * hr + pos;
        float dd = sceneSDF( aopos );
        occ += -(dd-hr)*sca;
        sca *= 0.95;
    }
    return clamp( 1.0 - 3.0*occ, 0.0, 1.0 );    
}

float calcSoftShadow( in vec3 ro, in vec3 rd, in float mint, in float tmax )
{
	float res = 1.0;
    float t = mint;
    for( int i=0; i<16; i++ )
    {
		float h = sceneSDF( ro + rd*t );
        res = min( res, 8.0*h/t );
        t += clamp( h, 0.02, 0.10 );
        if( res<0.005 || t>tmax ) break;
    }
    return clamp( res, 0.0, 1.0 );
}

float diffuse(vec3 lightDir, vec3 normal)
{
    return clamp(dot(normal, lightDir), 0.0, 1.0);
}

vec3 estimateNormal(vec3 p) 
{
    return normalize(vec3(
        sceneSDF(vec3(p.x + EPSILON, p.y, p.z)) - sceneSDF(vec3(p.x - EPSILON, p.y, p.z)),
        sceneSDF(vec3(p.x, p.y + EPSILON, p.z)) - sceneSDF(vec3(p.x, p.y - EPSILON, p.z)),
        sceneSDF(vec3(p.x, p.y, p.z  + EPSILON)) - sceneSDF(vec3(p.x, p.y, p.z - EPSILON))
    ));
}

vec4 light(vec4 color, vec3 lightDir, vec3 point)
{
    float ao = calcAO(point, estimateNormal(point));
    vec4 ambient = AMBIENT_COLOR * ao * 0.8;
    float shadow = calcSoftShadow(point, lightDir, 0.01, 5.0);
    return ((color * diffuse(lightDir, estimateNormal(point))) * shadow) + ambient;
}


float shortestDistanceToSurface(vec3 marchingDirection, float start, float end)
{
    float depth = start;
    for (int i = 0; i < MAX_MARCHING_STEPS; i++) 
    {
        float dist = sceneSDF(depth * marchingDirection);
        if (dist < EPSILON) 
        {
			return depth;
        }
        depth += dist;
        if (depth >= end) 
        {
            return end;
        }
    }
    return end;
}

float calcHorizFov() {
	return 2 * atan( tan(1.25/2) * 1.77777777777777777777778 );
}

vec3 rayDirection(vec2 size, vec2 fragCoord) 
{
    vec2 xy = fragCoord - size / 2.0;
    float z = size.y / tan(calcHorizFov() / 2.0);
    return normalize(vec3(xy, -z));
}

float A = 0.15;
float B = 0.50;
float C = 0.10;
float D = 0.20;
float E = 0.02;
float F = 0.30;
float W = 11.2;

vec4 uncharted2Tonemap(vec4 x)
{
   return vec4((((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F).xyz, x.w);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord)
{
	//fragCoord.y = resolution.y - fragCoord.y;
    vec3 marchDir = -rayDirection(resolution, fragCoord);
    //vec3 marchDir = vec3(fragCoord/resolution, 0.0);
    vec3 lightDir = vec3(0.0, 1.0, 0.0);
    
    
    float dist = shortestDistanceToSurface(marchDir, MIN_DIST, MAX_DIST);
	
	if (dist > depth2dist(texture(depthImage, fragCoord/resolution).r, 0.01, 2500.0)) {
		fragColor = vec4(0.0);
		return;
	}
    
    vec3 point = dist * marchDir;
        
    vec4 sampleCol = dist > MAX_DIST - EPSILON ? AMBIENT_COLOR : light(vec4(1.0), lightDir, point);
    float exposure = 2.0;
    
    vec4 whiteScale = 1.0f/uncharted2Tonemap(vec4(W, W, W, 1.0));
    sampleCol = uncharted2Tonemap(sampleCol*exposure);
    sampleCol *= whiteScale;
    sampleCol = pow( sampleCol, vec4(1.0/2.2));
    // Output to screen
    fragColor = sampleCol;//col;//vec4(fragCoord/resolution, 0.0, 1.0);
}

void main()
{
	if (gl_GlobalInvocationID.x > resolution.x || gl_GlobalInvocationID.y > resolution.y) return;
	
	vec2 uv = gl_GlobalInvocationID.xy/resolution;
	vec4 color = vec4(0.0);
	mainImage(color, gl_GlobalInvocationID.xy);
	if (color.a > 0.0)
		imageStore(resultImage, ivec2(gl_GlobalInvocationID.xy), color);
	else
		imageStore(resultImage, ivec2(gl_GlobalInvocationID.xy), texture(polyImage, uv)); 
}