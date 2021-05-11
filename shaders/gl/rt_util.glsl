
#include "common.glsl"
#include "gpu_voxels.glsl"
#include "rand.glsl"

#define PI	3.1415926535897932384626433832795
#define INF (1. / 0.)

#if VISUALIZE_COST
int iterations = 0;

uniform int max_iterations = 200;

uniform sampler2D		heat_gradient;
	#if VISUALIZE_WARP_COST
		#define WARP_COUNT_IN_WG ((LOCAL_SIZE_X*LOCAL_SIZE_Y) / 32) 
		shared uint warp_iter[WARP_COUNT_IN_WG];
	#endif
#endif

#define RAYT_PRIMARY		0
#define RAYT_REFLECT		1
#define RAYT_DIFFUSE		2
#define RAYT_SPECULAR		3
#define RAYT_SUN			4

#define DEBUGDRAW 1
#if DEBUGDRAW
bool _debugdraw = false;
uniform bool update_debugdraw = false;

vec4 _dbg_ray_cols[] = {
	vec4(1,0,1,1),
	vec4(0,0,1,1),
	vec4(1,0,0,1),
	vec4(0,1,0,1),
	vec4(1,1,0,1),
};
#endif

#define REFLECTIONS 1

//
struct Hit {
	vec3	pos;
	mat3	TBN;
	float	dist;
	uint	bid;
	uint	medium;
	vec3	col;
	vec2	occl_spec;
	vec3	emiss;
};

const float WORLD_SIZEf = float(WORLD_SIZE);
const float INV_WORLD_SIZEf = 1.0 / WORLD_SIZEf;
const uint ROUNDMASK = -1;
const uint FLIPMASK = WORLD_SIZE-1;

bool trace_ray (vec3 pos, vec3 dir, float max_dist, uint medium_bid, out Hit hit, int type) {
	// flip coordinate space such that ray is always positive (simplifies stepping logic)
	// keep track of flip via flipmask
	bvec3 ray_neg = lessThan(dir, vec3(0.0));
	vec3 flippedf = mix(pos, WORLD_SIZEf - pos, ray_neg);
	
	uvec3 flipmask = mix(uvec3(0u), uvec3(FLIPMASK), ray_neg);
	
	// precompute part of plane projection equation
	// prefer  'pos * inv_dir + bias'  over  'inv_dir * (pos - ray_pos)'
	// due to mad instruction
	//vec3 inv_dir = mix(1.0 / abs(dir), vec3(INF), equal(dir, vec3(0.0)));
	vec3 inv_dir = 1.0 / abs(dir);
	vec3 bias = inv_dir * -flippedf;
	
	float dist = 0.0;
	#if 1 // allow ray to start outside ray for nice debugging views
	{
		// calculate entry and exit coords into whole world cube
		vec3 t0v = inv_dir * -flippedf;
		vec3 t1v = inv_dir * (vec3(WORLD_SIZEf) - flippedf);
		float t0 = max(max(t0v.x, t0v.y), t0v.z);
		float t1 = min(min(t1v.x, t1v.y), t1v.z);
		
		// only if ray not inside cube
		t0 = max(t0, 0.0);
		t1 = max(t1, 0.0);
		
		// ray misses world texture
		if (t1 <= t0)
			return false;
		
		// adjust ray to start where it hits cube initally
		dist = t0;
		flippedf += abs(dir) * dist;
		flippedf = max(flippedf, vec3(0.0));
	}
	#else
	// cull any rays starting outside of cube
	if ( any(lessThanEqual(flippedf, vec3(0.0))) ||
		 any(greaterThanEqual(flippedf, vec3(WORLD_SIZEf))))
		return false;
	#endif
	
	// start at some level of octree
	// -best to start at 0 if camera on surface
	// -best at higher levels if camera were in a large empty region
	uint mip = 0;
	//uint mip = uint(OCTREE_MIPS-1);
	
	// round down to start cell of octree
	uvec3 coord = uvec3(floor(flippedf));
	coord &= ROUNDMASK << mip;
	
	uint voxel;
	
	for (;;) {
		#if VISUALIZE_COST
		++iterations;
		#if VISUALIZE_WARP_COST
		if (subgroupElect()) atomicAdd(warp_iter[gl_SubgroupID], 1u);
		#endif
		#endif
	
		uvec3 flipped = (coord ^ flipmask) >> mip;
		
		// read octree cell
		voxel = texelFetch(octree, ivec3(flipped), int(mip)).r;
		
		if (voxel != medium_bid) {
			// non-air octree cell
			if (mip == 0u)
				break; // found solid leaf voxel
			
			// decend octree
			mip--;
			uvec3 next_coord = coord + (1u << mip);
			
			// upate coord by determining which child octant is entered first
			// by comparing ray hit against middle plane hits
			vec3 tmidv = inv_dir * vec3(next_coord) + bias;
			
			coord = mix(coord, next_coord, lessThan(tmidv, vec3(dist)));
			
		} else {
			// air octree cell, continue stepping
			uvec3 next_coord = coord + (1u << mip);
			
			// calculate exit distances of next octree cell
			vec3 t0v = inv_dir * vec3(next_coord) + bias;
			dist = min(min(t0v.x, t0v.y), t0v.z);
			
			// step on axis where exit distance is lowest
			uint stepcoord;
			if (t0v.x == dist) {
				coord.x = next_coord.x;
				stepcoord = coord.x;
			} else if (t0v.y == dist) {
				coord.y = next_coord.y;
				stepcoord = coord.y;
			} else {
				coord.z = next_coord.z;
				stepcoord = coord.z;
			}
			
			
			#if 0
			// step up to highest changed octree parent cell
			mip = findLSB(stepcoord);
			#else
			// step up one level
			// (does not work if lower mips cannot be safely read without reading higher levels)
			// also breaks  mip >= uint(OCTREE_MIPS-1)  as world exit condition
			
			//mip += min(findLSB(stepcoord >> mip) - mip, 1u);
			mip += bitfieldExtract(stepcoord, int(mip), 1) ^ 1; // extract lowest bit of coord 
			#endif
			
			// round down coord to lower corner of (potential) parent cell
			coord &= ROUNDMASK << mip;
			
			//// exit when either stepped out of world or max ray dist reached
			//if (mip >= uint(OCTREE_MIPS-1) || dist >= max_dist)
			if (stepcoord >= WORLD_SIZE || dist >= max_dist) {
				#if DEBUGDRAW
				if (_debugdraw) dbgdraw_vector(pos - WORLD_SIZEf/2.0, dir * dist, _dbg_ray_cols[type]);
				#endif
				return false;
			}
		}
	}
	
	#if DEBUGDRAW
	if (_debugdraw) dbgdraw_vector(pos - WORLD_SIZEf/2.0, dir * dist, _dbg_ray_cols[type]);
	#endif
	
	if (type < RAYT_SUN) {
		coord ^= flipmask; // flip back to real coords
		
		// arrived at solid leaf voxel, read block id from seperate data structure
		//hit.bid = read_bid(ivec3(coord));
		hit.bid = voxel;
		hit.medium = medium_bid;
		
		// calcualte surface hit info
		hit.dist = dist;
		hit.pos = pos + dir * dist;
		
		vec2 uv;
		int face;
		{ // calc hit face, uv and normal
			//vec3 hit_fract = fract(hit.pos);
			vec3 hit_fract = hit.pos;
			vec3 hit_center = vec3(coord) + 0.5;
			
			vec3 offs = (hit.pos - hit_center);
			vec3 abs_offs = abs(offs);
			
			vec3 normal = vec3(0.0);
			vec3 tangent = vec3(0.0);
			
			if (abs_offs.x >= abs_offs.y && abs_offs.x >= abs_offs.z) {
				normal.x = sign(offs.x);
				tangent.y = sign(offs.x);
				face = offs.x < 0.0 ? 0 : 1;
				uv = hit_fract.yz;
				if (offs.x < 0.0) uv.x = 1.0 - uv.x;
			} else if (abs_offs.y >= abs_offs.z) {
				normal.y = sign(offs.y);
				tangent.x = -sign(offs.y);
				face = offs.y < 0.0 ? 2 : 3;
				uv = hit_fract.xz;
				if (offs.y >= 0.0) uv.x = 1.0 - uv.x;
			} else {
				normal.z = sign(offs.z);
				tangent.x = 1.0;
				face = offs.z < 0.0 ? 4 : 5;
				uv = hit_fract.xy;
				if (offs.z < 0.0) uv.y = 1.0 - uv.y;
			}
			
			vec3 bitangent = cross(normal, tangent);
			hit.TBN = mat3(tangent, bitangent, normal);
		}
		
		uint tex_bid = hit.bid == B_AIR ? medium_bid : hit.bid;
		float texid = float(block_tiles[tex_bid].sides[face]);
		
		float lod2 = log2(dist)*0.90 - 2.0;
		
		//if (tex_bid == B_STONE) {
		//	hit.col = textureLod(textures2_A, vec3(uv / 2.0, 1), lod2).rgb;
		//	vec3 normalmap = textureLod(textures2_N, vec3(uv / 2.0, 4), lod2).rgb * 2.0 - 1.0;
		//	
		//	hit.occl_spec.x = 1.0;
		//	hit.occl_spec.y = textureLod(textures2_N, vec3(uv / 2.0, 7), lod2).r;
		//	
		//	vec3 bitangent = cross(hit.normal, tangent);
		//	mat3 TBN = mat3(tangent, bitangent, hit.normal);
		//	
		//	hit.normal = TBN * normalize(normalmap);
		//	
		//} else if (tex_bid == B_HARDSTONE) {
		//	hit.col = textureLod(textures2_A, vec3(uv / 2.0, 0), lod2).rgb;
		//	vec3 normalmap = textureLod(textures2_N, vec3(uv / 2.0, 0), lod2).rgb * 2.0 - 1.0;
		//	
		//	hit.occl_spec.x = 1.0;
		//	hit.occl_spec.y = textureLod(textures2_N, vec3(uv / 2.0, 3), lod2).r;
		//	
		//	vec3 bitangent = cross(hit.normal, tangent);
		//	mat3 TBN = mat3(tangent, bitangent, hit.normal);
		//	
		//	hit.normal = TBN * normalize(normalmap);
		//} else if (tex_bid == B_GRAVEL) {
		//	hit.col = textureLod(textures_A, vec3(uv / 2.0, 0), lod2).rgb;
		//	vec3 normalmap = textureLod(textures_N, vec3(uv / 2.0, 0), lod2).rgb * 2.0 - 1.0;
		//	
		//	hit.occl_spec.x = 1.0;
		//	hit.occl_spec.y = 0.5;
		//	
		//	vec3 bitangent = cross(hit.normal, tangent);
		//	mat3 TBN = mat3(tangent, bitangent, hit.normal);
		//	
		//	hit.normal = TBN * normalize(normalmap);
		//} else if (tex_bid == B_GRASS) {
		//	hit.col = textureLod(textures_A, vec3(uv / 4.0, 1), lod2).rgb;
		//	vec3 normalmap = textureLod(textures_N, vec3(uv / 4.0, 4), lod2).rgb * 2.0 - 1.0;
		//	
		//	hit.occl_spec.x = 1.0;
		//	hit.occl_spec.y = 0.5;
		//	
		//	vec3 bitangent = cross(hit.normal, tangent);
		//	mat3 TBN = mat3(tangent, bitangent, hit.normal);
		//	
		//	hit.normal = TBN * normalize(normalmap);
		//} else {
			hit.col = textureLod(tile_textures, vec3(uv, texid), log2(dist)*0.20 - 0.7).rgb;
			
			if (tex_bid == B_TALLGRASS && face >= 4)
				hit.col = vec3(0.0);
		
			hit.occl_spec.x = 1.0;
			hit.occl_spec.y = 1.0;
		//}
		hit.emiss = hit.col * get_emmisive(hit.bid);
	}
	return true;
}

uniform bool  visualize_light = false;

#if !ONLY_PRIMARY_RAYS
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

uniform bool  sunlight_enable = true;
uniform float sunlight_dist = 90.0;
uniform vec3  sunlight_col = vec3(0.98, 0.92, 0.65) * 1.3;

uniform vec3  ambient_light;

uniform bool  bounces_enable = true;
uniform float bounces_max_dist = 30.0;
uniform int   bounces_max_count = 8;

uniform vec3 sun_pos = vec3(-28, 67, 102);
uniform float sun_pos_size = 4.0;

uniform vec3 sun_dir = normalize(vec3(-1,2,3));
uniform float sun_dir_rand = 0.05;

uniform float water_F0 = 0.6;

const float water_IOR = 1.333;
const float air_IOR = 1.0;

uniform sampler2D water_N_A;
uniform sampler2D water_N_B;

uniform float water_normal_time = 0.0; // wrap on some integer to avoid losing precision over time
uniform float water_normal_scale = 0.1;
uniform float water_normal_strength = 0.05;

//// From: https://godotshaders.com/shader/realistic-water/

// Wave settings:
uniform float	wave_speed		 = 0.3; // Speed scale for the waves
const float	wave_steep		 	= 0.12; // Speed scale for the waves
uniform vec4	wave_a			 = vec4(1.0, 1.0, 0.35*wave_steep, 3.0); 	// xy = Direction, z = Steepness, w = Length
uniform	vec4	wave_b			 = vec4(1.0, 0.6, 0.30*wave_steep, 1.55);	// xy = Direction, z = Steepness, w = Length
uniform	vec4	wave_c			 = vec4(1.0, 1.3, 0.25*wave_steep, 0.9); 	// xy = Direction, z = Steepness, w = Length

// Surface settings:
uniform vec2 	sampler_scale 	 = vec2(0.25, 0.25); 			// Scale for the sampler
uniform vec2	sampler_direction= vec2(0.05, 0.04); 			// Direction and speed for the sampler offset

// Wave function:
vec3 wave(vec4 parameter, vec2 position, float time, inout vec3 tangent, inout vec3 binormal)
{
	float	wave_steepness	 = parameter.z;
	float	wave_length		 = parameter.w;

	float	k				 = 2.0 * 3.14159265359 / wave_length;
	float 	c 				 = sqrt(9.8 / k);
	vec2	d				 = normalize(parameter.xy);
	float 	f 				 = k * (dot(d, position) - c * time);
	float 	a				 = wave_steepness / k;
	
			tangent			+= normalize(vec3(1.0-d.x * d.x * (wave_steepness * sin(f)),    -d.x * d.y * (wave_steepness * sin(f)), d.x * (wave_steepness * cos(f))));
			binormal		+= normalize(vec3(   -d.x * d.y * (wave_steepness * sin(f)), 1.0-d.y * d.y * (wave_steepness * sin(f)), d.y * (wave_steepness * cos(f))));

	return vec3(d.x * (a * cos(f)), d.y * (a * cos(f)), a * sin(f) * 0.25);
}


// Vertex shader:
void water_shader (vec2 uv, float time, out vec3 normal, out vec3 vertex_pos) {
	float	t				 = time * wave_speed;
	
			vertex_pos		= vec3(0,0,0);
	vec3	vertex_position  = vec3(uv, 0.0);
	
	vec3	vertex_tangent 	 = vec3(0.0, 0.0, 0.0);
	vec3	vertex_binormal  = vec3(0.0, 0.0, 0.0);
	
			vertex_pos		+= wave(wave_a, vertex_position.xy, t, vertex_tangent, vertex_binormal);
			vertex_pos		+= wave(wave_b, vertex_position.xy, t, vertex_tangent, vertex_binormal);
			vertex_pos		+= wave(wave_c, vertex_position.xy, t, vertex_tangent, vertex_binormal);
	
	vec3	vertex_normal	 = normalize(cross(vertex_tangent, vertex_binormal));
	
	
	vec2	uv_offset 					 = sampler_direction * time;
	
	// Normalmap:
	vec3 	normalmap					 = texture(water_N_A, uv * 0.7 - uv_offset*2.0).rgb * 0.75;		// 75 % sampler A
			normalmap 					+= texture(water_N_B, uv * 0.5 + uv_offset).rgb * 0.25;			// 25 % sampler B
	
	// Refraction UV:
	vec3	ref_normalmap				 = normalmap * 2.0 - 1.0;
	//ref_normalmap.xy *= 0.03;
	ref_normalmap.xy *= 0.03;
	ref_normalmap = normalize(ref_normalmap);
	
			ref_normalmap				 = normalize(vertex_tangent*ref_normalmap.x + vertex_binormal*ref_normalmap.y + vertex_normal*ref_normalmap.z);
	normal = ref_normalmap;
}

#if 0
vec3 sample_water_normal (vec3 pos_world) {
	vec2 uv1 = (pos_world.xy + water_normal_time * 0.2) * water_normal_scale;
	vec2 uv2 = (pos_world.xy + -water_normal_time * 0.2) * water_normal_scale * 0.5;
	uv2.xy = uv2.yx;
	
	vec2 a = texture(water_normal, uv1).rg * 2.0 - 1.0;
	vec2 b = texture(water_normal, uv2).rg * 2.0 - 1.0;
	//
	return normalize(vec3((a+b) * water_normal_strength, 1.0));
}
#endif

bool trace_ray_refl_refr (vec3 ray_pos, vec3 ray_dir, float max_dist, uint medium_bid, out Hit hit, out bool was_reflected, int type) {
	bool did_hit = trace_ray(ray_pos, ray_dir, max_dist, medium_bid, hit, type);

#if REFLECTIONS
	if (did_hit && ((medium_bid == B_AIR && hit.bid == B_WATER) || (medium_bid == B_WATER && hit.bid == B_AIR))) {
		// reflect
		
		vec3 hit_normal = hit.TBN[2];
		
		vec3 vertex;
		vec3 normal_map = hit_normal;
		
		#if 1
		if (hit_normal.z > 0.0) {
			//normal_map = sample_water_normal(hit.pos);
			water_shader(vec2(1,-1) * hit.pos.yx * 0.3, water_normal_time, normal_map, vertex);
		}
		#endif
		
		float reflect_fac = fresnel(-ray_dir, hit_normal, water_F0);
		
		float eta = hit.bid == B_WATER ? air_IOR / water_IOR : water_IOR / air_IOR;
		
		vec3 reflect_dir = reflect(ray_dir, normal_map);
		vec3 refract_dir = refract(ray_dir, normal_map, eta);
		
		if (dot(refract_dir, refract_dir) == 0.0) {
			// total internal reflection, ie. outside of snells window
			reflect_fac = 1.0;
		}
		
		if (dot(reflect_dir, hit_normal) < 0.0) {
			reflect_fac = 0.0; // can't reflect below water (normal_map vector was caused raflection below actual geometry normal)
		}
		
		#if 1
		uint new_medium;
		if (rand() <= reflect_fac) {
			// reflect
			ray_pos = hit.pos + hit_normal * 0.001;
			ray_dir = reflect_dir;
			new_medium = medium_bid;
		} else {
			// refract
			ray_pos = hit.pos + hit_normal * -0.001;
			ray_dir = refract_dir;
			new_medium = medium_bid == B_AIR ? B_WATER : B_AIR;
		}
		max_dist -= hit.dist;
		
		was_reflected = true;
		return trace_ray(ray_pos, ray_dir, max_dist, new_medium, hit, RAYT_REFLECT);
		#endif
	}
#endif
	was_reflected = false;
	return did_hit;
}

vec3 collect_sunlight (vec3 pos, vec3 normal) {
	if (sunlight_enable) {
		#if SUNLIGHT_MODE == 0
		// directional sun
		vec3 dir = sun_dir + rand3()*sun_dir_rand;
		float cos = dot(dir, normal);
		
		Hit hit;
		if (cos > 0.0 && !trace_ray(pos, dir, sunlight_dist, B_AIR, hit, RAYT_SUN))
			return sunlight_col * cos;
		#else
		// point sun
		
		vec3 spos = sun_pos + float(WORLD_SIZE/2);
		
		vec3 offs = (spos + (rand3()-0.5) * sun_pos_size) - pos;
		float dist = length(offs);
		vec3 dir = normalize(offs);
		
		float cos = dot(dir, normal);
		float atten = 16000.0 / (dist*dist);
		
		float max_dist = dist - sun_pos_size*0.5;
		
		Hit hit;
		if (cos > 0.0 && !trace_ray(pos, dir, max_dist, B_AIR, hit, RAYT_SUN))
			return sunlight_col * (cos * atten);
		#endif
	}
	return vec3(0.0);
}
#endif
