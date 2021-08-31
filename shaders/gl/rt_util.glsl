
#include "common.glsl"

#if DEBUGDRAW
	#include "dbg_indirect_draw.glsl"
#endif

#include "gpu_voxels.glsl"
#include "rand.glsl"

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
uniform int max_iterations = 200;

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
		float val = float(dur) / float(1200000);
		//float val = float(dur) / float(_iterations) / 2000.0;
		
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
	vec3	normal;
	uint	bid;
	vec4	col;
};

const float WORLD_SIZEf = float(WORLD_SIZE);
const float INV_WORLD_SIZEf = 1.0 / WORLD_SIZEf;
const int CHUNK_MASK = ~63;
const float epsilon = 0.001; // This epsilon should not round to zero with numbers up to 4096 

bool _dbgdraw = false;

float rounded_cube_sdf (vec3 p, vec3 loc, float size, float r) {
	vec3 rel = p - loc;
	vec3 a = abs(rel);
	float sdf = max(max(a.x, a.y), a.z) - size + r;
	if (sdf <= 0.0) return sdf;
	
	vec3 closest = clamp(rel, -size, +size);
	return length(closest - rel) - r;
}
float sphere_sdf (vec3 p, vec3 loc, float r) {
	vec3 center = loc;
	return length(p - center) - r;
}

float cylinderZ_df (vec3 p, vec3 loc, float r, float h0, float h1) {
	float closestz = clamp(p.z, loc.z + h0, loc.z + h1);
	
	vec2 centerxy = loc.xy;
	return length(p - vec3(centerxy, closestz)) - r;
}

void voxel_df (inout float hit_df, inout ivec3 hit_vox, vec3 pos, uint bid, ivec3 vox_coord, float noiseval) {
	float df;
	
	vec3 cent = vec3(vox_coord) + 0.5;
	
	if (bid == B_LEAVES)
		df = sphere_sdf(pos, cent, 0.7);
	else if (bid == B_GLOWSHROOM)
		df = max( sphere_sdf(pos, cent, 0.5), min(-(pos.z - cent.z), cylinderZ_df(pos, cent, 0.15, -0.5, +0.5)) );
	else if (bid == B_TREE_LOG)
		df = cylinderZ_df(pos, cent, 0.4, -0.5, +0.5);
	else if (bid == B_CRYSTAL || (bid >= B_CRYSTAL2 && bid <= B_CRYSTAL6))
		df = cylinderZ_df(pos, cent, 0.55, -0.5, +0.5);
	else
		df = rounded_cube_sdf(pos, cent, 0.43, 0.1);
	
	//df += noiseval;
	
	if (df < hit_df) {
		hit_df = df;
		hit_vox = vox_coord;
	}
}

float eval_vox_df (vec3 pos, out ivec3 vox_hit) {
	ivec3 texcoord = ivec3(round(pos));
	
	uint tex000 = texelFetchOffset(voxel_tex, texcoord, 0, ivec3(-1,-1,-1)).r;
	uint tex100 = texelFetchOffset(voxel_tex, texcoord, 0, ivec3( 0,-1,-1)).r;
	uint tex010 = texelFetchOffset(voxel_tex, texcoord, 0, ivec3(-1, 0,-1)).r;
	uint tex110 = texelFetchOffset(voxel_tex, texcoord, 0, ivec3( 0, 0,-1)).r;
	uint tex001 = texelFetchOffset(voxel_tex, texcoord, 0, ivec3(-1,-1, 0)).r;
	uint tex101 = texelFetchOffset(voxel_tex, texcoord, 0, ivec3( 0,-1, 0)).r;
	uint tex011 = texelFetchOffset(voxel_tex, texcoord, 0, ivec3(-1, 0, 0)).r;
	uint tex111 = texelFetchOffset(voxel_tex, texcoord, 0, ivec3( 0, 0, 0)).r;
	
	float df = 0.40;
	
	float N = noise(pos * 3.0) * 0.1;
	
	if (tex000 > B_AIR) voxel_df(df, vox_hit, pos, tex000, texcoord + ivec3(-1,-1,-1), N);
	if (tex100 > B_AIR) voxel_df(df, vox_hit, pos, tex100, texcoord + ivec3( 0,-1,-1), N);
	if (tex010 > B_AIR) voxel_df(df, vox_hit, pos, tex010, texcoord + ivec3(-1, 0,-1), N);
	if (tex110 > B_AIR) voxel_df(df, vox_hit, pos, tex110, texcoord + ivec3( 0, 0,-1), N);
	if (tex001 > B_AIR) voxel_df(df, vox_hit, pos, tex001, texcoord + ivec3(-1,-1, 0), N);
	if (tex101 > B_AIR) voxel_df(df, vox_hit, pos, tex101, texcoord + ivec3( 0,-1, 0), N);
	if (tex011 > B_AIR) voxel_df(df, vox_hit, pos, tex011, texcoord + ivec3(-1, 0, 0), N);
	if (tex111 > B_AIR) voxel_df(df, vox_hit, pos, tex111, texcoord + ivec3( 0, 0, 0), N);
	
	return df;
}
vec3 eval_vox_df_normal (float df, vec3 pos) {
	ivec3 _ignore;
	df = eval_vox_df(pos, _ignore);
	
	float dfX = eval_vox_df(pos + vec3(epsilon,0,0), _ignore);
	float dfY = eval_vox_df(pos + vec3(0,epsilon,0), _ignore);
	float dfZ = eval_vox_df(pos + vec3(0,0,epsilon), _ignore);
	
	return normalize(vec3(dfX, dfY, dfZ) - df);
}

bool trace_ray (vec3 ray_pos, vec3 ray_dir, float max_dist, out Hit hit) {
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
	
	vec3 pos;
	ivec3 coord;
	
	int dbgcol = 0;
	int iter = 0;
	
	ivec3 vox_hit;
	float df;
	
	float prev_dist = 0.0;
	
	for (;;) {
		VISUALIZE_ITERATION
		
		pos = dist * ray_dir + ray_pos;
		coord = ivec3(pos);
		
		df = float(texelFetch(df_tex, coord, 0).r);
		df *= manhattan_fac;
		
		if (df <= 0) {
			
			df = eval_vox_df(pos, vox_hit);
			
			if (df <= 0.0)
				break; // hit
		}
		
		float min_step = 0.05; // limit min step for perf reasons, TODO: scale this to be larger the further along the ray is, parameterize relative to pixel size?
		df = max(df, min_step);
		
	#if DEBUGDRAW
		{
			vec3 pos = dist * ray_dir + ray_pos; // fix pos not being updated after DDA (just for dbg)
			vec4 col = dbgcol==0 ? vec4(1,0,0,1) : vec4(0.8,0.2,0,1);
			if (_dbgdraw) dbgdraw_wire_sphere(pos - WORLD_SIZEf/2.0, vec3(df*2.0), col);
			if (_dbgdraw) dbgdraw_point(      pos - WORLD_SIZEf/2.0,      df*0.5 , col);
		}
	#endif
		
		// compute chunk exit, since DF is not valid for things outside of the chunk it is generated for
		vec3 chunk_exit = vec3(coord & CHUNK_MASK) + chunk_exit_planes; // 3 conv + 3 and + 3 add
		
		vec3 chunk_t1v = inv_dir * chunk_exit + bias; // 3 madd
		float chunk_t1 = min(min(chunk_t1v.x, chunk_t1v.y), chunk_t1v.z); // 2 min
		
		prev_dist = dist;
		dist += df; // 1 add
		dist = min(dist, chunk_t1); // 1 min  limit step to exactly on the exit face of the chunk
		
		dbgcol ^= 1;
		iter++;
		if (iter >= max_iterations || dist >= max_dist)
			return false; // miss
	}
	
	#if DEBUGDRAW
	if (_dbgdraw) dbgdraw_vector(ray_pos - WORLD_SIZEf/2.0, ray_dir * dist, vec4(1,0,0,1));
	#endif
	
	{ // binary search for isosurface
		float at = prev_dist, bt = dist; // at: df>0   bt: df<0   search for dist at which df=0
		for (int i=0; i<8; ++i) {
			float midt = (at + bt) * 0.5;
			
			pos = midt * ray_dir + ray_pos;
			
			df = eval_vox_df(pos, vox_hit);
			
			if (df < 0.0) bt = midt;
			else          at = midt;
		}
		dist = (at + bt) * 0.5; // take avg of whatever interval is still left
	}
	
	{ // calc hit info
		
		hit.bid = texelFetch(voxel_tex, vox_hit, 0).r;
		hit.dist = dist;
		hit.pos = dist * ray_dir + ray_pos;
		
		hit.normal = eval_vox_df_normal(df, hit.pos);
		
		vec2 uv;
		//vec2 uv_dx; // uv gradients to get mip mapping
		//vec2 uv_dy;
		
		int face;
		{ // calc hit face, uv and normal
			vec3 hit_center = vec3(vox_hit) + 0.5;
			
			vec3 offs = (hit.pos - hit_center);
			vec3 abs_offs = abs(offs);
			
			//hit.normal = vec3(0.0);
			
			if (abs_offs.x >= abs_offs.y && abs_offs.x >= abs_offs.z) {
				//hit.normal.x = sign(offs.x);
				face = offs.x < 0.0 ? 0 : 1;
				uv = hit.pos.yz;
				if (offs.x < 0.0) uv.x = 1.0 - uv.x;
			} else if (abs_offs.y >= abs_offs.z) {
				//hit.normal.y = sign(offs.y);
				face = offs.y < 0.0 ? 2 : 3;
				uv = hit.pos.xz;
				if (offs.y >= 0.0) uv.x = 1.0 - uv.x;
			} else {
				//hit.normal.z = sign(offs.z);
				face = offs.z < 0.0 ? 4 : 5;
				uv = hit.pos.xy;
				if (offs.z < 0.0) uv.y = 1.0 - uv.y;
			}
		}
		
		uint medium_bid = B_AIR;
		uint tex_bid = hit.bid == B_AIR ? medium_bid : hit.bid;
		float texid = float(block_tiles[tex_bid].sides[face]);
		
		hit.col = textureLod(tile_textures, vec3(uv, texid), log2(dist)*0.20 - 1.0).rgba;
		
		//hit.col.rgb = hit.normal;
		
		//if (tex_bid == B_TALLGRASS && face >= 4)
		//	hit.col = vec4(0.0);
	}
	
	//hit.col = vec4(vec3(dist / 200.0), 1);
	return true; // hit
}
