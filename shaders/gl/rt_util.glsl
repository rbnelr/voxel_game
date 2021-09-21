
#include "common.glsl"

#if DEBUGDRAW
	#include "dbg_indirect_draw.glsl"
#endif

#include "gpu_voxels.glsl"
#include "rand.glsl"

vec3 generate_tangent (vec3 normal) { // NOTE: tangents currently do not correspond with texture uvs, normal mapping will be wrong
	vec3 tangent = vec3(0,0,1);
	if (abs(normal.z) > 0.8) return vec3(1,0,0);
	return tangent;
}
mat3 calc_TBN (vec3 normal, vec3 tangent) {
	vec3 bitangent = cross(normal, tangent); // generate bitangent vector orthogonal to both normal and tangent
	tangent = cross(bitangent, normal); // regenerate tangent vector in case it was not orthogonal to normal
	
	return mat3(normalize(tangent), normalize(bitangent), normal);
}
mat3 generate_TBN (vec3 normal) {
	return calc_TBN(normal, generate_tangent(normal));
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


float fresnel (float dotVN, float F0) {
	float x = clamp(1.0 - dotVN, 0.0, 1.0);
	float x2 = x*x;
	return F0 + ((1.0 - F0) * x2 * x2 * x);
}
float fresnel_roughness (float dotVN, float F0, float roughness) {
	float x = clamp(1.0 - dotVN, 0.0, 1.0);
	float x2 = x*x;
	return F0 + ((max((1.0 - roughness), F0) - F0) * x2 * x2 * x);
}

vec3 hemisphere_sample () {
	// cosine weighted sampling (100% diffuse)
	// http://www.rorydriscoll.com/2009/01/07/better-sampling/
	
	// takes a uniform sample on a disc (x,y)
	// and projects into up into a hemisphere to get the cosine weighted points on the hemisphere
	
	// random sampling (Monte Carlo)
	vec2 rnd = rand2(); // uniform sample in [0,1) square
	
	// map square to disc, preserving uniformity
	float r = sqrt(rnd.y);
	float theta = 2*PI * rnd.x;
	
	float x = r * cos(theta);
	float y = r * sin(theta);
	
	// map (project) disc up to hemisphere,
	// turning uniform distribution into cosine weighted distribution
	//vec3 dir = vec3(x,y, sqrt(max(0.0, 1.0 - rnd.y)));
	vec3 dir = vec3(x,y, sqrt(1.0 - rnd.y));
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

vec3 reflect_roughness (vec3 refl, vec3 normal, float roughness) {
    mat3 TBN = generate_TBN(refl); // tangent space relative to reflected vector to be able to 'offset' it by roughness
	
    float a = roughness * roughness;
    vec2 rnd = rand2(); // uniform sample in [0,1) square
	
	//float z = sqrt(clamp( (1.0 - r.y) / 1.0 + (a-1.0) * r.y, 0.0,1.0));
	float z = sqrt( (1.0 - rnd.y) / (1.0 + (a-1.0) * rnd.y) );
	
	float r = sqrt(1.0 - z*z);
	float theta = 2*PI * rnd.x;
	
	float x = r * cos(theta);
	float y = r * sin(theta);
	
    vec3 dir = TBN * vec3(x,y,z);
    return dot(dir, normal) > 0.0 ? dir : refl;
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
uniform float near_px_size;

float near_plane_dist;
float ray_r_per_dist;

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
	
	near_plane_dist = length(ray_dir);
	ray_dir = normalize(ray_dir);
	
	ray_r_per_dist = near_px_size * (view.clip_near / near_plane_dist);
	
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

const float WORLD_SIZEf = float(WORLD_SIZE);
const float INV_WORLD_SIZEf = 1.0 / WORLD_SIZEf;
const int CHUNK_MASK = ~63;
const float epsilon = 0.001; // This epsilon should not round to zero with numbers up to 4096 

#if TAA_ENABLE
layout(rgba16f , binding = 5) writeonly restrict uniform  image2D taa_color;
layout(rgba16ui, binding = 6) writeonly restrict uniform uimage2D taa_posage;

uniform  sampler2D taa_history_color;
uniform usampler2D taa_history_posage;

uniform mat4 prev_world2clip;
uniform int taa_max_age = 256;

vec3 APPLY_TAA (vec3 val, vec3 pos, ivec3 coord, vec3 normal, ivec2 pxpos) {
	uint age = 1u;
	
	vec4 prev_clip = prev_world2clip * vec4(pos - WORLD_SIZEf/2, 1.0);
	prev_clip.xyz /= prev_clip.w;
	
	uint cur_norm = 5;
	if      (normal.x < -0.5) cur_norm = 0;
	else if (normal.x > +0.5) cur_norm = 1;
	else if (normal.y < -0.5) cur_norm = 2;
	else if (normal.y > +0.5) cur_norm = 3;
	else if (normal.z < -0.5) cur_norm = 4;
	else if (normal.z > +0.5) cur_norm = 5;
	
	uint cur_pos = 0;
	if      (cur_norm/2 == 0) cur_pos = (uint)coord.x;
	else if (cur_norm/2 == 1) cur_pos = (uint)coord.y;
	else if (cur_norm/2 == 2) cur_pos = (uint)coord.z;
	
	vec2 uv = prev_clip.xy * 0.5 + 0.5;
	if (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0) {
		uvec4 sampl = textureLod(taa_history_posage, uv, 0.0);
		
		//vec3 sampl_pos = vec3(sampl.rgb) / float(0xffff) * WORLD_SIZEf;
		uint sampl_norm = sampl.r; // 0=-X 1=+X 2=-Y ...
		uint sampl_pos = sampl.g; // relative to normal
		uint sampl_age = sampl.a;
		
		//if (distance(pos, sampl_pos) < 0.1) {
		if (cur_norm == sampl_norm && cur_pos == sampl_pos) {
			age = min(sampl_age, uint(taa_max_age));
			
			vec3 accumulated = textureLod(taa_history_color, uv, 0.0).rgb * float(age);
			
			age += 1u;
			val = (accumulated + val) / float(age);
		}
	}
	
	//uvec3 pos_enc = uvec3(round(pos * float(0xffff) / WORLD_SIZEf));
	//pos_enc = clamp(pos_enc, uvec3(0), uvec3(0xffff));
	
	imageStore(taa_color, pxpos, vec4(val, 0.0));
	imageStore(taa_posage, pxpos, uvec4(cur_norm, cur_pos, 0u, age));
	
	return val;
}
#else
	#define APPLY_TAA(val, pos, coord, normal, pxpos) (val)
#endif

//
uniform int max_iterations = 200;
uniform float visualize_mult = 1.0;

#if VISUALIZE_COST
	int _iterations = 0;
	uint64_t _ts_start;
	
	uniform sampler2D heat_gradient;
	
	void INIT_VISUALIZE_COST () {
		_iterations = 0;
		
		#if VISUALIZE_TIME
		_ts_start = clockARB();
		#endif
	}
		
	void GET_VISUALIZE_COST (inout vec3 col) {
		#if VISUALIZE_TIME
		uint64_t dur = clockARB() - _ts_start;
		float val = float(dur) / (100000.0 * visualize_mult);
		//float val = float(dur) / float(_iterations) / (2000.0*visualize_mult);
		
		#else
		float val = float(_iterations) / float(max_iterations);
		#endif
		
		col = texture(heat_gradient, vec2(val, 0.5)).rgb;
	}
	
	#define VISUALIZE_ITERATION ++_iterations;
	
#else
	#define INIT_VISUALIZE_COST()
	#define GET_VISUALIZE_COST(col)
	#define VISUALIZE_ITERATION
#endif

struct Hit {
	vec3	pos;
	float	dist;
	vec3	normal; // normal mapped normal
	mat3	gTBN; // non-normal mapped real geometry (uv-compatible) TBN
	
	ivec3	coord;
	uint	bid;
	
	vec4	col;
	vec3	emiss;
};

bool _dbgdraw = false;

uniform sampler2DArray test_cubeN;
uniform sampler2DArray test_cubeH;

uniform float parallax_zstep = 0.004;
uniform float parallax_max_step = 0.02;
uniform float parallax_scale = 0.15;
const float tex = 2.0;

bool parallax_mapping (vec3 ray_pos, vec3 ray_dir, ivec3 coord, float base_dist, inout float dist, out int face) {
	
	vec3 pos = dist * ray_dir + ray_pos;
	vec3 rel  = pos - vec3(coord);
	vec3 offs = abs(rel - 0.5);
	
	vec3 cur; // uv,height
	vec3 step;
	if (offs.x >= offs.y && offs.x >= offs.z) {
		face = rel.x < 0.5 ? 0 : 1;
		cur  = rel.x < 0.5 ? vec3(1.0-rel.y, rel.z, -rel.x) : vec3(rel.y, rel.z, rel.x-1.0);
		step = rel.x < 0.5 ? vec3(-ray_dir.y, ray_dir.z, -ray_dir.x) : vec3(ray_dir.y, ray_dir.z, ray_dir.x);
	} else if (offs.y >= offs.z) {
		face = rel.y < 0.5 ? 2 : 3;
		cur  = rel.y < 0.5 ? vec3(rel.x, rel.z, -rel.y) : vec3(1.0-rel.x, rel.z, rel.y-1.0);
		step = rel.y < 0.5 ? vec3(ray_dir.x, ray_dir.z, -ray_dir.y) : vec3(-ray_dir.x, ray_dir.z, ray_dir.y);
	} else {
		face = rel.z < 0.5 ? 4 : 5;
		cur  = rel.z < 0.5 ? vec3(rel.x, 1.0-rel.y, -rel.z) : vec3(rel.x, rel.y, rel.z-1.0);
		step = rel.z < 0.5 ? vec3(ray_dir.x, -ray_dir.y, -ray_dir.z) : vec3(ray_dir.x, ray_dir.y, ray_dir.z);
	}
	
	// width the ray would be at this hit point if it was a cone covering the pixel it tries to render
	float ray_r = (dist + base_dist) * ray_r_per_dist;
	if (ray_r > 0.03) return true;
	
	float stepsz = min(parallax_zstep / (abs(step.z)+epsilon), parallax_max_step);
	stepsz /= map(ray_r, 0.03, 0.01) + 0.1;
	
	vec3 start = cur;
	step *= stepsz;
	
	float texlodH = log2(length(step.xy) * float(textureSize(test_cubeH, 0).x));
	
	float prev_height = 0.0, height;
	
	for (int i=0; i<100; ++i) {
		VISUALIZE_ITERATION
		
		height = textureLod(test_cubeH, vec3(cur.xy, tex), texlodH).r * parallax_scale - parallax_scale; // [0,1] to [-0.15,0]
		
	#if DEBUGDRAW
		vec3 p = ray_pos + ray_dir * (length(cur - start) + dist);
		if (_dbgdraw) dbgdraw_point(p - WORLD_SIZEf/2.0, 0.02, vec4(0,1,0,1));
	#endif
		
		if (cur.z <= height) break;
		
		prev_height = height;
		cur += step;
		
		if (cur.x < 0.0 || cur.y < 0.0 || cur.x > 1.0 || cur.y > 1.0 || cur.z > 0.0)
			return false; // miss
	}
	if (height <= -parallax_scale) return false; // miss
	
	{ // linear interpolate surface between last two samples
		float t = (height - cur.z) / (height - prev_height - step.z);
		
		//float t0 = 0.0; // inside surface
		//float t1 = 1.0; // outside surface
		//for (int i=0; i<4; ++i) {
		//	float tm = (t0+t1)*0.5;
		//	vec3 c = cur - step * tm;
		//	
		//	height = textureLod(test_cubeH, vec3(c.xy, 1.0), lod).r * parallax_scale;
		//	if (cur.z <= height) t0 = tm; // tm inside surface
		//	else                 t1 = tm;
		//}
		//float t = (t0+t1)*0.5;
		
		cur = cur - step * t; // lerp(cur, cur - step, t)
	}
	
	dist += length(cur - start);
	return true; // hit
}

bool trace_ray (vec3 ray_pos, vec3 ray_dir, float max_dist, float base_dist, out Hit hit, vec3 raycol) {
	bvec3 dir_sign = greaterThanEqual(ray_dir, vec3(0.0));
	
	ivec3 step_dir = mix(ivec3(-1), ivec3(+1), dir_sign);
	ivec3 vox_exit = mix(ivec3(0), ivec3(1), dir_sign);
	
	// project to chunk bounds that are epsilon futher out to avoid ray getting stuck when exiting chunk
	// NOTE: this means we can sometimes step along the diagonal between chunks, missing cells
	vec3 chunk_exit_planes = mix(vec3(-epsilon), vec3(64.0 + epsilon), dir_sign);
	
	// precompute part of plane projection equation
	// prefer  'pos * inv_dir + bias'  over  'inv_dir * (pos - ray_pos)'
	// due to madd instruction
	vec3 inv_dir = 1.0 / ray_dir;
	vec3 bias = inv_dir * -ray_pos;
	
	float dist;
	{ // allow ray to start outside of world texture cube for nice debugging views
		vec3 world_min = vec3(        0.0) + epsilon;
		vec3 world_max = vec3(WORLD_SIZEf) - epsilon;
		
		// calculate entry and exit coords into whole world cube
		vec3 t0v = mix(world_max, world_min, dir_sign) * inv_dir + bias;
		vec3 t1v = mix(world_min, world_max, dir_sign) * inv_dir + bias;
		float t0 = max(max(t0v.x, t0v.y), t0v.z);
		float t1 = min(min(t1v.x, t1v.y), t1v.z);
		
		t0 = max(t0, 0.0);
		t1 = max(t1, 0.0);
		
		// ray misses world texture
		if (t1 <= t0)
			return false;
		
		// adjust ray to start where it hits cube initally
		dist = t0;
		max_dist = min(t1, max_dist);
	}
	
	// step epsilon less than 1m to possibly avoid somtimes missing one voxel cell when DF stepping
	float manhattan_fac = (1.0 - epsilon) / (abs(ray_dir.x) + abs(ray_dir.y) + abs(ray_dir.z));
	
	vec3 pos = dist * ray_dir + ray_pos;
	ivec3 coord = ivec3(pos);
	
	int dbgcol = 0;
	int iter = 0;
	int face;
	
	for (;;) {
		VISUALIZE_ITERATION
		
		int dfi = texelFetch(df_tex, coord, 0).r; // tex read
		
		// step up to exit of current cell, since DF is safe up until its bounds
		// seems to give a little bit of perf, as this reduces iteration count
		// of course iteration now has more instructions, so could hurt as well
		// -> disable for now, we save iterations, but it gets slower in almost every case
		//vec3 t1v = inv_dir * vec3(coord + vox_exit) + bias;
		//dist = min(min(t1v.x, t1v.y), t1v.z);
		
		if (dfi > 1) {
			float df = float(dfi) * manhattan_fac;
			
			// DF tells us that we can still step by <df> before we could possibly hit a voxel
			// step via DF raymarching
			
		//#if DEBUGDRAW
		//	pos = dist * ray_dir + ray_pos; // fix pos not being updated after DDA (just for dbg)
		//	vec4 col = dbgcol==0 ? vec4(1,0,0,1) : vec4(0.8,0.2,0,1);
		//	if (_dbgdraw) dbgdraw_wire_sphere(pos - WORLD_SIZEf/2.0, vec3(df*2.0), col);
		//	if (_dbgdraw) dbgdraw_point(      pos - WORLD_SIZEf/2.0,      df*0.5 , col);
		//#endif
			
			// compute chunk exit, since DF is not valid for things outside of the chunk it is generated for
			vec3 chunk_exit = vec3(coord & CHUNK_MASK) + chunk_exit_planes;
			
			vec3 chunk_t1v = inv_dir * chunk_exit + bias;
			float chunk_t1 = min(min(chunk_t1v.x, chunk_t1v.y), chunk_t1v.z);
			
			dist += df;
			dist = min(dist, chunk_t1); // limit step to exactly on the exit face of the chunk
			
			vec3 pos = dist * ray_dir + ray_pos;
			// update coord for next iteration
			coord = ivec3(pos);
		} else {
			// we need to check individual voxels by DDA now
			
		//#if DEBUGDRAW
		//	if (_dbgdraw) dbgdraw_wire_cube(vec3(coord) + 0.5 - WORLD_SIZEf/2.0, vec3(1.0), vec4(1,1,0,1));
		//#endif
		
			// -1 marks solid voxels (they have 1-voxel border of 0s around them)
			// this avoids one memory read
			// and should eliminate all empty block id reads and thus help improve caching for the DF values by a bit
			if (dfi < 0) {
				#if PRALLAX_MAPPING
				if (parallax_mapping(ray_pos, ray_dir, coord, base_dist, dist, face))
				#endif
					break; // hit
			}
			
			vec3 t1v = inv_dir * vec3(coord + vox_exit) + bias;
			dist = min(min(t1v.x, t1v.y), t1v.z);
			
			// step on axis where exit distance is lowest
			if      (t1v.x == dist) coord.x += step_dir.x;
			else if (t1v.y == dist) coord.y += step_dir.y;
			else                    coord.z += step_dir.z;
		}
		
		dbgcol ^= 1;
		iter++;
		if (iter >= max_iterations || dist >= max_dist)
			break; // miss
	}
	
	#if DEBUGDRAW
	if (_dbgdraw) dbgdraw_vector(ray_pos - WORLD_SIZEf/2.0, ray_dir * dist, vec4(raycol,1));
	#endif
	
	if (iter >= max_iterations || dist >= max_dist)
		return false; // miss
	
	{ // calc hit info
		
		hit.bid = texelFetch(voxel_tex, coord, 0).r;
		hit.dist = dist;
		hit.pos = dist * ray_dir + ray_pos;
		hit.coord = coord;
		
		vec2 uv;
		{ // calc hit face, uv and normal
			vec3 rel = hit.pos - vec3(coord);
			vec3 offs = rel - 0.5;
			vec3 abs_offs = abs(offs);
			
			vec3 geom_normal = vec3(0.0);
			vec3 tang = vec3(0.0);
			
		#if PARALLAX_MAPPING
			if (face <= 1) {
				geom_normal.x = sign(offs.x);
				tang.y = offs.x < 0.0 ? -1 : +1;
				
				uv = offs.x < 0.0 ? vec2(1.0-rel.y, rel.z) : vec2(rel.y, rel.z);
			} else if (face <= 3) {
				geom_normal.y = sign(offs.y);
				tang.x = offs.y < 0.0 ? +1 : -1;
				
				uv = offs.y < 0.0 ? vec2(rel.x, rel.z) : vec2(1.0-rel.x, rel.z);
			} else {
				geom_normal.z = sign(offs.z);
				tang.x = +1;
				
				uv = offs.z < 0.0 ? vec2(rel.x, 1.0-rel.y) : vec2(rel.x, rel.y);
			}
		#else
			if (abs_offs.x >= abs_offs.y && abs_offs.x >= abs_offs.z) {
				face = rel.x < 0.5 ? 0 : 1;
				
				geom_normal.x = sign(offs.x);
				tang.y = offs.x < 0.0 ? -1 : +1;
				
				uv = offs.x < 0.0 ? vec2(1.0-rel.y, rel.z) : vec2(rel.y, rel.z);
			} else if (abs_offs.y >= abs_offs.z) {
				face = rel.y < 0.5 ? 2 : 3;
				
				geom_normal.y = sign(offs.y);
				tang.x = offs.y < 0.0 ? +1 : -1;
				
				uv = offs.y < 0.0 ? vec2(rel.x, rel.z) : vec2(1.0-rel.x, rel.z);
			} else {
				face = rel.z < 0.5 ? 4 : 5;
				
				geom_normal.z = sign(offs.z);
				tang.x = +1;
				
				uv = offs.z < 0.0 ? vec2(rel.x, 1.0-rel.y) : vec2(rel.x, rel.y);
			}
		#endif
			
			hit.gTBN = calc_TBN(geom_normal, tang);
		}
		
		// width the ray would be at this hit point if it was a cone covering the pixel it tries to render
		float ray_r = (dist + base_dist) * ray_r_per_dist;
		
		float texlod  = log2(ray_r * float(textureSize(tile_textures, 0).x));
		float texlodN = log2(ray_r * float(textureSize(test_cubeN   , 0).x));
		
		uint medium_bid = B_AIR;
		
		uint tex_bid = hit.bid;
		if  (hit.bid == B_AIR) tex_bid = medium_bid; 
		
		float texid = float(block_tiles[tex_bid].sides[face]);
		
		hit.col = textureLod(tile_textures, vec3(uv, texid), texlod).rgba;
		
	#if NORMAL_MAPPING
		{ // normal mapping
			const float normal_map_stren = 1.0;
			
			vec3 sampl = textureLod(test_cubeN, vec3(uv, tex), texlodN).rgb;
			sampl = sampl * 2.0 - 1.0;
			sampl.xy *= normal_map_stren;
			
			//hit.normal = hit.gTBN * vec3(sampl, sqrt(1.0 - sampl.x*sampl.x - sampl.y*sampl.y));
			hit.normal = hit.gTBN * normalize(sampl);
		}
	#else
		hit.normal = hit.gTBN[2];
	#endif
		
		//hit.col.rgb = hit.normal * 0.5 + 0.5;
		
		hit.emiss = hit.col.rgb * get_emmisive(tex_bid);
	}
	
	//hit.col = vec4(vec3(dist / 200.0), 1);
	return true; // hit
}
