
#include "common.glsl"

#if DEBUGDRAW
	#include "dbg_indirect_draw.glsl"
#endif

#include "gpu_voxels.glsl"
#include "rand.glsl"

#define PI	3.1415926535897932384626433832795
#define INF (1. / 0.)

#if VISUALIZE_COST
	int iterations = 0;
	
	uniform sampler2D heat_gradient;
	
	#if VISUALIZE_WARP_COST
		#define WARP_COUNT_IN_WG ((WG_PIXELS_X*WG_PIXELS_Y) / 32) 
		shared uint warp_iter[WARP_COUNT_IN_WG];
		
		#define INIT_VISUALIZE_COST \
			if (subgroupElect()) { \
				warp_iter[gl_SubgroupID] = 0u; \
			} \
			iterations = 0; \
			barrier();
			
		#define GET_VISUALIZE_COST(VAL) \
			const uint warp_cost = warp_iter[gl_SubgroupID]; \
			const uint local_cost = iterations; \
			 \
			float wasted_work = float(warp_cost - local_cost) / float(warp_cost); \
			VAL = texture(heat_gradient, vec2(wasted_work, 0.5)).rgb;
			
		#define VISUALIZE_COST_COUNT \
			++iterations; \
			if (subgroupElect()) atomicAdd(warp_iter[gl_SubgroupID], 1u);
	#else
		#define INIT_VISUALIZE_COST \
			iterations = 0;
			
		#define GET_VISUALIZE_COST(VAL) \
			VAL = texture(heat_gradient, vec2(float(iterations) / float(max_iterations), 0.5)).rgb;
			
		#define VISUALIZE_COST_COUNT \
			++iterations;
	#endif
#else
	#define INIT_VISUALIZE_COST
	#define GET_VISUALIZE_COST(VAL)
	#define VISUALIZE_COST_COUNT
#endif

vec3 generate_tangent (vec3 normal) { // NOTE: tangents currently do not correspond with texture uvs, normal mapping will be wrong
	vec3 tangent = vec3(0,0,+1);
	if (abs(normal.z) > 0.99) return vec3(0,+1,0);
	return tangent;
}
mat3 calc_TBN (vec3 normal, vec3 tangent) {
	vec3 bitangent = cross(normal, tangent); // generate bitangent vector orthogonal to both normal and tangent
	tangent = cross(bitangent, normal); // regenerate tangent vector in case it was not orthogonal to normal
	
	return mat3(tangent, bitangent, normal);
}

//// Some raytracing primitives

// The MIT License
// Copyright © 2016 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

float ray_capsule (in vec3 ro, in vec3 rd, in vec3 pa, in vec3 pb, in float r) {
    vec3  ba = pb - pa;
    vec3  oa = ro - pa;

    float baba = dot(ba,ba);
    float bard = dot(ba,rd);
    float baoa = dot(ba,oa);
    float rdoa = dot(rd,oa);
    float oaoa = dot(oa,oa);

    float a = baba      - bard*bard;
    float b = baba*rdoa - baoa*bard;
    float c = baba*oaoa - baoa*baoa - r*r*baba;
    float h = b*b - a*c;
    if (h >= 0.0) {
        float t = (-b-sqrt(h))/a;
        float y = baoa + t*bard;
        // body
        if(y > 0.0 && y < baba) return t;
        // caps
        vec3 oc = (y<=0.0) ? oa : ro - pb;
        b = dot(rd,oc);
        c = dot(oc,oc) - r*r;
        h = b*b - c;
        if (h > 0.0) return -b - sqrt(h);
    }
    return -1.0;
}
vec3 capsule_normal (in vec3 pos, in vec3 a, in vec3 b, in float r) {
    vec3  ba = b - a;
    vec3  pa = pos - a;
    float h = clamp(dot(pa,ba)/dot(ba,ba),0.0,1.0);
    return (pa - h*ba)/r;
}

float ray_sphere (in vec3 ro, in vec3 rd, in vec3 sph, in float r) {
	vec3 oc = ro - sph;
	float b = dot( oc, rd );
	float c = dot( oc, oc ) - r*r;
	float h = b*b - c;
	if( h<0.0 ) return INF;
	return -b - sqrt( h );
}
vec3 sphere_normal (in vec3 pos, in vec3 sph) {
    return normalize(pos-sph.xyz);
}

void sphere (vec3 ray_pos, vec3 ray_dir, vec3 sph_pos, float sph_r, inout float cur_t, inout vec3 cur_pos) {
	float t = ray_sphere(ray_pos, ray_dir, sph_pos, sph_r);
	if (t >= cur_t) return;
	
	cur_t = t;
	cur_pos = sph_pos;
}

//// Noise function for testing, replace with set of better noise functions later
// > don't use mod289 for example, use better int based hashing instead, seems to be superior on modern gpus
// > float-based noise usually shows patterns on closer inspection

float mod289(float x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 mod289(vec4 x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 perm(vec4 x){return mod289(((x * 34.0) + 1.0) * x);}

float noise(vec3 p){
    vec3 a = floor(p);
    vec3 d = p - a;
    d = d * d * (3.0 - 2.0 * d);

    vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
    vec4 k1 = perm(b.xyxy);
    vec4 k2 = perm(k1.xyxy + b.zzww);

    vec4 c = k2 + a.zzzz;
    vec4 k3 = perm(c);
    vec4 k4 = perm(c + 1.0);

    vec4 o1 = fract(k3 * (1.0 / 41.0));
    vec4 o2 = fract(k4 * (1.0 / 41.0));

    vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
    vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

    float val = o4.y * d.y + o4.x * (1.0 - d.y);
	
	return val * 2.0 - 1.0;
}

// get pixel ray in world space based on pixel coord and matricies
bool get_ray (vec2 px_pos, out vec3 ray_pos, out vec3 ray_dir) {
	
#if 1 // Normal camera projection
	//vec2 px_center = px_pos + rand2();
	vec2 px_center = px_pos + vec2(0.5);
	vec2 ndc = px_center / view.viewport_size * 2.0 - 1.0;
	
	vec4 clip = vec4(ndc, -1, 1) * view.clip_near; // ndc = clip / clip.w;

	// TODO: can't get optimization to one single clip_to_world mat mul to work, why?
	// -> clip_to_cam needs translation  cam_to_world needs to _not_ have translation
	vec3 cam = (view.clip_to_cam * clip).xyz;

	ray_dir = (view.cam_to_world * vec4(cam, 0)).xyz;
	ray_dir = normalize(ray_dir);
	
	// ray starts on the near plane
	ray_pos = (view.cam_to_world * vec4(cam, 1)).xyz;
	
	// make relative to gpu world representation (could bake into matrix)
	ray_pos += float(WORLD_SIZE/2);
	
	return true;

#else // 360 Sphere Projections
	
	vec2 px_center = (px_pos + vec2(0.5)) / view.viewport_size; // [0,1]
	
	#if 0 // Equirectangular projection
		float lon = (px_center.x - 0.5) * PI*2;
		float lat = (px_center.y - 0.5) * PI;
	#else // Mollweide projection
		float x = px_center.x * 2.0 - 1.0;
		float y = px_center.y * 2.0 - 1.0;
		
		if ((x*x + y*y) > 1.0)
			return false;
		
		float theta = asin(y);
		
		float lon = (PI * x) / cos(theta);
		float lat = asin((2.0 * theta + sin(2.0 * theta)) / PI);
	#endif
	
	float c = cos(lat);
	vec3 dir_cam = vec3(c * sin(lon), sin(lat), -c * cos(lon));
	
	ray_dir = (view.cam_to_world * vec4(dir_cam, 0)).xyz;
	ray_pos = (view.cam_to_world * vec4(0,0,0,1)).xyz;
	
	// make relative to gpu world representation (could bake into matrix)
	ray_pos += float(WORLD_SIZE/2);

	return true;
#endif
}

//
struct Hit {
	vec3	pos;
	mat3	TBN;
	float	dist;
	uint	bid;
	vec4	col;
};

const float WORLD_SIZEf = float(WORLD_SIZE);
const float INV_WORLD_SIZEf = 1.0 / WORLD_SIZEf;

bool _dbgdraw = false;

uniform int max_iterations = 200;

bool trace_ray (vec3 ray_pos, vec3 ray_dir, float max_dist, out Hit hit) {
	float dist;
	
	{ // allow ray to start outside ray for nice debugging views
		// flip coordinate space such that ray is always positive (simplifies stepping logic)
		// keep track of flip via flipmask
		bvec3 ray_neg = lessThan(ray_dir, vec3(0.0));
		vec3 flippedf = mix(ray_pos, WORLD_SIZEf - ray_pos, ray_neg);
		
		//uvec3 flipmask = mix(uvec3(0u), uvec3(FLIPMASK), ray_neg);
		
		// precompute part of plane projection equation
		// prefer  'pos * inv_dir + bias'  over  'inv_dir * (pos - ray_pos)'
		// due to mad instruction
		vec3 inv_dir = 1.0 / abs(ray_dir);
		vec3 bias = inv_dir * -flippedf;
		
		// calculate entry and exit coords into whole world cube
		vec3 t0v =     vec3(0) * inv_dir + bias;
		vec3 t1v = WORLD_SIZEf * inv_dir + bias;
		float t0 = max(max(t0v.x, t0v.y), t0v.z);
		float t1 = min(min(t1v.x, t1v.y), t1v.z);
		
		t0 = max(t0, 0.0);
		t1 = max(t1, 0.0);
		
		// ray misses world texture
		if (t1 <= t0)
			return false;
		
		// adjust ray to start where it hits cube initally
		dist = t0;
		max_dist = t1;
		
		//flippedf += abs(dir) * t;
		//flippedf = max(flippedf, vec3(0.0));
	}
	
	#if DEBUGDRAW
	if (_dbgdraw) dbgdraw_vector(ray_pos - WORLD_SIZEf/2.0, ray_dir * dist, vec4(1,0,0,1));
	#endif
	
	for (int iter=0; iter<max_iterations; ++iter) {
		if (iter >= max_iterations || dist >= max_dist)
			return false; // miss
		VISUALIZE_COST_COUNT
		
		vec3 pos = dist * ray_dir + ray_pos;
		
		uint bid = textureLod(voxel_tex, pos * INV_WORLD_SIZEf, 0.0).r;
		if (bid > B_AIR) break;
		
		dist += 0.5;
	}
	
	hit.col = vec4(vec3(dist / 200.0), 1);
	return true; // hit
}

#if 0
float fresnel (vec3 view, vec3 norm, float F0) {
	float x = clamp(1.0 - dot(view, norm), 0.0, 1.0);
	float x2 = x*x;
	return F0 + ((1.0 - F0) * x2 * x2 * x);
}

vec3 hemisphere_sample () {
	// cosine weighted sampling (100% diffuse)
	// http://www.rorydriscoll.com/2009/01/07/better-sampling/
	
	// takes a uniform sample on a disc (x,y)
	// and projects into up into a hemisphere to get the cosine weighted points on the hemisphere
	
	// random sampling (Monte Carlo)
	vec2 uv = rand2(); // uniform sample in [0,1) square
	
	// map square to disc, preserving uniformity
	float r = sqrt(uv.y);
	float theta = 2*PI * uv.x;
	
	float x = r * cos(theta);
	float y = r * sin(theta);
	
	// map (project) disc up to hemisphere,
	// turning uniform distribution into cosine weighted distribution
	vec3 dir = vec3(x,y, sqrt(max(0.0, 1.0 - uv.y)));
	return dir;
}
vec3 hemisphere_sample_stratified (int i, int n) {
	// cosine weighted sampling (100% diffuse)
	// http://www.rorydriscoll.com/2009/01/07/better-sampling/
	
	// takes a uniform sample on a disc (x,y)
	// and projects into up into a hemisphere to get the cosine weighted points on the hemisphere
	
	// stratified sampling (Quasi Monte Carlo)
	int Nx = 4;
	ivec2 strata = ivec2(i % Nx, i / Nx);
	
	float scale = 1.0 / float(Nx);
	
	vec2 uv = rand2(); // uniform sample in [0,1) square
	//uv = (vec2(strata) + 0.5) * scale;
	uv = (vec2(strata) + uv) * scale;
	
	// map square to disc, preserving uniformity
	float r = sqrt(uv.y);
	float theta = 2*PI * uv.x;
	
	float x = r * cos(theta);
	float y = r * sin(theta);
	
	// map (project) disc up to hemisphere,
	// turning uniform distribution into cosine weighted distribution
	vec3 dir = vec3(x,y, sqrt(max(0.0, 1.0 - uv.y)));
	return dir;
}

vec3 random_in_sphere () {
	vec3 rnd = rand3();
    float theta = rnd.x * 2.0 * PI;
    float phi = acos(2.0 * rnd.y - 1.0);
    float r = pow(rnd.z, 0.33333333);
    float sp = sin(phi);
    float cp = cos(phi);
    return r * vec3(sp * sin(theta), sp * cos(theta), cp);
}

// TODO: could optimize this to be precalculated for all normals, since we currently only have 6
// this also will not really do what I want for arbitrary normals and also not work for normal mapping
// this just gives you a arbitrary tangent and bitangent that does not correspond to the uvs at all
mat3 get_tangent_to_world (vec3 normal) {
	vec3 tangent = abs(normal.x) >= 0.9 ? vec3(0,1,0) : vec3(1,0,0);
	vec3 bitangent = cross(normal, tangent);
	tangent = cross(bitangent, normal);
	
	return mat3(tangent, bitangent, normal);
}
#endif
