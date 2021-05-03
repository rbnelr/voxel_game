#version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t, uint8_t

#if VISUALIZE_COST && VISUALIZE_WARP_COST
	#extension GL_KHR_shader_subgroup_arithmetic : enable
	#extension GL_ARB_shader_group_vote : enable
#endif

layout(local_size_x = LOCAL_SIZE_X, local_size_y = LOCAL_SIZE_Y) in;

#define RAND_SEED_TIME 1

#include "rt_util.glsl"

layout(rgba16f, binding = 5) writeonly restrict uniform image2D output_color;

#if TAA_ENABLE
layout(rgba16f, binding = 6) writeonly restrict uniform image2D taa_color;
layout(r16ui  , binding = 7) writeonly restrict uniform uimage2D taa_posage;

uniform  sampler2D taa_history_color;
uniform usampler2D taa_history_posage;

uniform mat4 prev_world2clip;
uniform float taa_alpha = 0.05;
#endif

uniform uint rand_frame_index = 0;

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

struct Cone {
	vec3   dir;
	float  slope;
	float  weight;
	float _pad0, _pad1, _pad2;
};

layout(std140, binding = 4) uniform ConeConfig {
	ivec4 count; // vector for padding
	Cone cones[32];
} cones;

const float vct_start_dist = 1.0 / 16;
uniform float vct_size = 1.0;

// sharpen texture samples by 
vec4 read_vct_texture (vec3 texcoord, float r) {
	float size = r * 2.0;
	float lod = log2(size);
	
	#if 0
	// support negative lod (size < 1.0) by snapping texture coords to nearest texels
	// when approaching size=0
	// size = 0.5 would snap [0.25,0.75] to 0.5
	//              and lerp [0.75,1.25] in [0.5,1.5]
	size = min(size, 1.0);
	texcoord = 0.5 * size + texcoord;
	texcoord = min(fract(texcoord) * (1.0 / size) - 0.5, 0.5) + floor(texcoord);
	#endif
	
	return textureLod(vct_tex, texcoord * INV_WORLD_SIZEf, lod);
}
vec4 trace_cone (vec3 cone_pos, vec3 cone_dir, float cone_slope, float max_dist) {
	vec3 color = vec3(0.0);
	float transp = 1.0; // inverse alpha to support alpha stepsize fix
	
	float dist = vct_start_dist;
	
	for (int i=0; i<1000; ++i) {
		#if VISUALIZE_COST
		++iterations;
		#if VISUALIZE_WARP_COST
		if (subgroupElect()) atomicAdd(warp_iter[gl_SubgroupID], 1u);
		#endif
		#endif
		
		vec3 pos = cone_pos + cone_dir * dist;
		float r = cone_slope * dist;
		
		vec4 sampl = read_vct_texture(pos, r);
		//sampl *= r;
		
		color += transp * sampl.rgb;
		transp -= transp * (1.0 - (1.0 - sampl.a));
		//transp -= transp * (1.0 - pow(1.0 - sampl.a, r*2.0));
		
		#if DEBUGDRAW
		if (_debugdraw) {
			dbgdraw_wire_cube(pos - WORLD_SIZEf/2.0, vec3(r*2.0), vec4(1,0,0,1));
			//dbgdraw_wire_cube(pos - WORLD_SIZEf/2.0, vec3(r*2.0), vec4(vec3(transp), 1.0));
		}
		#endif
		
		if (transp < 0.001 || dist >= max_dist)
			break;
		
		dist = (dist + r) / (1.0f - cone_slope);
	}
	
	return vec4(color, 1.0 - transp);
}

vec3 voxel_cone_trace (uvec2 pxpos, vec3 view_ray_dir, bool did_hit, in Hit hit) {
	#if 0
	// primary cone for debugging
	vec3 cone_pos, cone_dir;
	get_ray(vec2(pxpos), cone_pos, cone_dir);
	
	//cone_pos = hit.pos;
	//cone_dir = hit.TBN[2];
	//cone_dir = reflect(view_ray_dir, hit.TBN[2]);
	
	float max_dist = 400.0;
	float cone_slope = 1.0 / vct_size;
	return trace_cone(cone_pos, cone_dir, cone_slope, max_dist).rgb;
	#else
	vec3 col = vec3(0.0);
	if (did_hit) {
		//vec3 dir = hit.normal;
		//vec3 dir = reflect(view_ray_dir, hit.normal);
		
		float max_dist = 400.0;
		vec3 light = vec3(0.0);
		
		for (int i=0; i<cones.count.x; ++i) {
			Cone c = cones.cones[i];
			vec3 dir = hit.TBN * c.dir;
			light += trace_cone(hit.pos, dir, c.slope, max_dist).rgb * c.weight;
		}
		
		if (visualize_light)
			hit.col = vec3(1.0);
		col = light * hit.col + hit.emiss;
	}
	return col;
	#endif
}

void main () {
#if VISUALIZE_COST && VISUALIZE_WARP_COST
	if (subgroupElect()) {
		warp_iter[gl_SubgroupID] = 0u;
	}
	barrier();
#endif
	
	uvec2 pxpos = gl_GlobalInvocationID.xy;
	
	//// maybe try not to do rays that we do not see (happens due to local group size)
	//if (pxpos.x >= uint(view.viewport_size.x) || pxpos.y >= uint(view.viewport_size.y))
	//	return;
	
#if DEBUGDRAW
	_debugdraw = update_debugdraw && pxpos.x == uint(view.viewport_size.x)/2 && pxpos.y == uint(view.viewport_size.y)/2;
#endif
	
	srand(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, rand_frame_index);
	
	// primary ray
	vec3 ray_pos, ray_dir;
	bool bray = get_ray(vec2(pxpos), ray_pos, ray_dir);
	float max_dist = INF;
	
	uint start_bid = read_bid(ivec3(floor(ray_pos)));
	
	vec3 col = vec3(0.0);
#if VCT
	Hit hit;
	bool did_hit = bray && trace_ray(ray_pos, ray_dir, max_dist, start_bid, hit, RAYT_PRIMARY);
	
#if VISUALIZE_COST && VISUALIZE_WARP_COST
	if (subgroupElect()) {
		warp_iter[gl_SubgroupID] = 0u;
	}
	barrier();
#endif
#if VISUALIZE_COST
	iterations = 0;
#endif
	
	col = voxel_cone_trace(pxpos, ray_dir, did_hit, hit);
#elif ONLY_PRIMARY_RAYS
	Hit hit;
	if (bray && trace_ray(ray_pos, ray_dir, max_dist, start_bid, hit, RAYT_PRIMARY))
		col = hit.col;
#else
	vec3 surf_light = vec3(0.0); // light on surface for taa write
	
	#if TAA_ENABLE
	uint surf_position = 0xffffu;
	uint age = 0;
	#endif
	
	Hit hit;
	bool was_reflected = false;
	if (bray && trace_ray_refl_refr(ray_pos, ray_dir, max_dist, start_bid, hit, was_reflected, RAYT_PRIMARY)) {
		ray_pos = hit.pos + hit.TBN[2] * 0.001;
		
		surf_light = collect_sunlight(ray_pos, hit.TBN[2]);
		
		#if 0 // specular test
		if (bounces_enable) {
			max_dist = bounces_max_dist;
			
			vec3 cur_pos = ray_pos;
			mat3 TBN = hit.TBN;
			vec3 contrib = vec3(hit.occl_spec.y);
			
			ray_dir = normalize(reflect(ray_dir, TBN[2]) + random_in_sphere()*0.2);
			
			for (int j=0; j<bounces_max_count-1; ++j) {
				bool was_reflected2;
				Hit hit2;
				if (!trace_ray_refl_refr(cur_pos, ray_dir, max_dist, hit.medium, hit2, was_reflected2, RAYT_SPECULAR))
					break;
				
				cur_pos = hit2.pos + TBN[2] * 0.001;
				max_dist -= hit2.dist;
				
				TBN = hit2.TBN;
				
				ray_dir = TBN * hemisphere_sample(); // already cos weighted
				
				vec3 light2 = collect_sunlight(cur_pos, TBN[2]);
				
				surf_light += (hit2.emiss + hit2.col * light2) * contrib;
				contrib *= hit2.col;
			}
		}
		#endif
		
		if (bounces_enable) {
			max_dist = bounces_max_dist;
			
			vec3 cur_pos = ray_pos;
			mat3 TBN = hit.TBN;
			vec3 contrib = vec3(hit.occl_spec.x);
			
			for (int j=0; j<bounces_max_count; ++j) {
				ray_dir = TBN * hemisphere_sample(); // already cos weighted
				
				bool was_reflected2;
				Hit hit2;
				if (!trace_ray_refl_refr(cur_pos, ray_dir, max_dist, hit.medium, hit2, was_reflected2, RAYT_DIFFUSE))
					break;
				
				TBN = hit2.TBN;
				
				cur_pos = hit2.pos + TBN[2] * 0.001;
				max_dist -= hit2.dist;
				
				vec3 light2 = collect_sunlight(cur_pos, TBN[2]);
				
				surf_light += (hit2.emiss + hit2.col * light2) * contrib;
				contrib *= hit2.col;
			}
		}
		
		#if TAA_ENABLE
		if (!was_reflected) {
			hit.pos -= float(WORLD_SIZE/2);
			
			uvec3 rounded = uvec3(ivec3(round(hit.pos))) & 0x3fffu;
			if      (abs(hit.TBN[2].x) > 0.9) surf_position =           rounded.x;
			else if (abs(hit.TBN[2].y) > 0.9) surf_position = 0x4000u | rounded.y;
			else                              surf_position = 0xc000u | rounded.z;
			
			vec4 prev_clip = prev_world2clip * vec4(hit.pos, 1.0);
			prev_clip.xyz /= prev_clip.w;
			
			vec2 uv = prev_clip.xy * 0.5 + 0.5;
			if (all(greaterThan(uv, vec2(0.0))) && all(lessThan(uv, vec2(1.0)))) {
				uvec2 sampl = texture(taa_history_posage, uv).rg;
				
				uint sampl_pos = sampl.r;
				uint sampl_age = sampl.g;
				
				if (surf_position == sampl_pos) {
					uint max_age = (uint)round((1.0 / taa_alpha) - 1.0);
					
					age = min(sampl_age, max_age);
					float alpha = 1.0 / (float(age) + 1.0);
					
					vec3 prev_light = texture(taa_history_color, uv).rgb;
					surf_light = mix(prev_light, surf_light, vec3(alpha));
				}
			}
		}
		#endif
		
		if (visualize_light)
			hit.col = vec3(1.0);
		col = hit.emiss + hit.col * surf_light;
	}
	
	#if TAA_ENABLE
	//col = vec3(float(age) / 100.0);
	
	imageStore(taa_color,  ivec2(pxpos), vec4(surf_light, 0.0));
	imageStore(taa_posage, ivec2(pxpos), uvec4(surf_position, age + 1,0,0));
	#endif
	
#endif

#if VISUALIZE_COST
	#if VISUALIZE_WARP_COST
		const uint warp_cost = warp_iter[gl_SubgroupID];
		const uint local_cost = iterations;
		
		float wasted_work = float(warp_cost - local_cost) / float(warp_cost);
		col = texture(heat_gradient, vec2(wasted_work, 0.5)).rgb;
	#else
		col = texture(heat_gradient, vec2(float(iterations) / float(max_iterations), 0.5)).rgb;
	#endif
#endif
	
	imageStore(output_color, ivec2(pxpos), vec4(col, 1.0));
}
