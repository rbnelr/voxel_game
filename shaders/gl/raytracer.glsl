#version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t, uint8_t

#if VISUALIZE_COST && VISUALIZE_WARP_COST
	#extension GL_KHR_shader_subgroup_arithmetic : enable
	#extension GL_ARB_shader_group_vote : enable
#endif

layout(local_size_x = LOCAL_SIZE_X, local_size_y = LOCAL_SIZE_Y) in;

#define RAND_SEED_TIME 1

#include "rt_util.glsl"

layout(rgba16f, binding = 4) writeonly restrict uniform image2D output_color;

#if TAA_ENABLE
layout(rgba16f, binding = 5) writeonly restrict uniform image2D taa_color;
layout(r16ui  , binding = 6) writeonly restrict uniform uimage2D taa_posage;

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
	
#if DEBUG_RAYS
	_dbg_ray = update_debug_rays && pxpos.x == uint(view.viewport_size.x)/2 && pxpos.y == uint(view.viewport_size.y)/2;
	if (_dbg_ray) line_drawer_init();
#endif
	
	srand(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, rand_frame_index);
	
	// primary ray
	vec3 ray_pos, ray_dir;
	bool bray = get_ray(vec2(pxpos), ray_pos, ray_dir);
	float max_dist = INF;
	
	uint start_bid = read_bid(uvec3(floor(ray_pos)));
	
	vec3 col = vec3(0.0);
#if ONLY_PRIMARY_RAYS
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
		ray_pos = hit.pos + hit.normal * 0.001;
		
		surf_light = collect_sunlight(ray_pos, hit.normal);
		
		#if 1 // specular test
		if (bounces_enable) {
			max_dist = bounces_max_dist;
			
			vec3 cur_pos = ray_pos;
			vec3 cur_normal = hit.normal;
			vec3 contrib = vec3(hit.occl_spec.y);
			
			ray_dir = normalize(reflect(ray_dir, cur_normal) + random_in_sphere()*0.2);
			
			for (int j=0; j<bounces_max_count-1; ++j) {
				bool was_reflected2;
				Hit hit2;
				if (!trace_ray_refl_refr(cur_pos, ray_dir, max_dist, hit.medium, hit2, was_reflected2, RAYT_SPECULAR))
					break;
				
				cur_pos = hit2.pos + hit2.normal * 0.001;
				max_dist -= hit2.dist;
				
				cur_normal = hit2.normal;
				
				ray_dir = get_tangent_to_world(cur_normal) * hemisphere_sample(); // already cos weighted
				
				vec3 light2 = collect_sunlight(cur_pos, cur_normal);
				
				surf_light += (hit2.emiss + hit2.col * light2) * contrib;
				contrib *= hit2.col;
			}
		}
		#endif
		
		if (bounces_enable) {
			max_dist = bounces_max_dist;
			
			vec3 cur_pos = ray_pos;
			vec3 cur_normal = hit.normal;
			vec3 contrib = vec3(hit.occl_spec.x);
			
			for (int j=0; j<bounces_max_count; ++j) {
				ray_dir = get_tangent_to_world(cur_normal) * hemisphere_sample(); // already cos weighted
				
				bool was_reflected2;
				Hit hit2;
				if (!trace_ray_refl_refr(cur_pos, ray_dir, max_dist, hit.medium, hit2, was_reflected2, RAYT_DIFFUSE))
					break;
				
				cur_pos = hit2.pos + hit2.normal * 0.001;
				max_dist -= hit2.dist;
				
				cur_normal = hit2.normal;
				
				vec3 light2 = collect_sunlight(cur_pos, cur_normal);
				
				surf_light += (hit2.emiss + hit2.col * light2) * contrib;
				contrib *= hit2.col;
			}
		}
		
		#if TAA_ENABLE
		if (!was_reflected) {
			hit.pos -= float(WORLD_SIZE/2);
			
			uvec3 rounded = uvec3(ivec3(round(hit.pos))) & 0x3fffu;
			if      (abs(hit.normal.x) > 0.9) surf_position =           rounded.x;
			else if (abs(hit.normal.y) > 0.9) surf_position = 0x4000u | rounded.y;
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
