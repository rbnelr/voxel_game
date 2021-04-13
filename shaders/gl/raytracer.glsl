#version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t, uint8_t

#if VISUALIZE_COST && VISUALIZE_WARP_COST
	#extension GL_KHR_shader_subgroup_arithmetic : enable
	#extension GL_ARB_shader_group_vote : enable
#endif

layout(local_size_x = LOCAL_SIZE_X, local_size_y = LOCAL_SIZE_Y) in;

#define RAND_SEED_TIME 1

#include "rt_util.glsl"

layout(rgba16f, binding = 4) writeonly restrict uniform image2D img;
uniform sampler2D prev_framebuffer;

uniform mat4 prev_world2clip;
uniform float taa_alpha = 0.05;
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
	
	// maybe try not to do rays that we do not see (happens due to local group size)
	if (pxpos.x >= uint(view.viewport_size.x) || pxpos.y >= uint(view.viewport_size.y))
		return;
	
#if DEBUG_RAYS
	_dbg_ray = update_debug_rays && pxpos.x == uint(view.viewport_size.x)/2 && pxpos.y == uint(view.viewport_size.y)/2;
	if (_dbg_ray) line_drawer_init();
#endif
	
	uint hit_id = 0;
	
#if ONLY_PRIMARY_RAYS
	vec3 ray_pos, ray_dir;
	bool bray = get_ray(vec2(pxpos), ray_pos, ray_dir);
	
	Hit hit;
	bool did_hit = bray && trace_ray(ray_pos, ray_dir, INF, hit, false);
	vec3 col = did_hit ? hit.col : vec3(0.0);
#else
	srand(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, rand_frame_index);
	
	// primary ray
	vec3 ray_pos, ray_dir;
	bool bray = get_ray(vec2(pxpos), ray_pos, ray_dir);
	float max_dist = INF;
	
	vec3 first_hit_pos;
	uint first_hit_bid;
	bool was_reflected = false;
	
	vec3 col = vec3(0.0);
	
	Hit hit;
	bool did_hit = bray && trace_ray_refl_refr(ray_pos, ray_dir, max_dist, hit, was_reflected);
	if (did_hit) {
		first_hit_pos = hit.pos;
		first_hit_bid = hit.bid;
		
		ray_pos = hit.pos + hit.normal * 0.001;
		
		if (visualize_light)
			hit.col = vec3(1.0);
		
		vec3 light = collect_sunlight(ray_pos, hit.normal);
		col += hit.emiss + hit.col * light;
		
		if (bounces_enable) {
			max_dist = bounces_max_dist;
			
			vec3 cur_normal = hit.normal;
			vec3 contrib = hit.col;
			
			for (int j=0; j<bounces_max_count; ++j) {
				ray_dir = get_tangent_to_world(cur_normal) * hemisphere_sample(); // already cos weighted
				
				bool was_reflected2;
				if (!trace_ray_refl_refr(ray_pos, ray_dir, max_dist, hit, was_reflected2))
					break;
				
				ray_pos = hit.pos + hit.normal * 0.001;
				max_dist -= hit.dist;
				
				cur_normal = hit.normal;
				
				light = collect_sunlight(ray_pos, cur_normal);
				
				col += (hit.emiss + hit.col * light) * contrib;
				contrib *= hit.col;
			}
		}
	}
	
	if (did_hit && !was_reflected) {
		first_hit_pos -= float(WORLD_SIZE/2);
		
		vec4 prev_clip = prev_world2clip * vec4(first_hit_pos, 1.0);
		prev_clip.xyz /= prev_clip.w;
		
		vec2 uv = prev_clip.xy * 0.5 + 0.5;
		if (all(greaterThan(uv, vec2(0.0))) && all(lessThan(uv, vec2(1.0)))) {
			vec4 prev_val = texture(prev_framebuffer, uv);
			
			vec3 prev_col = prev_val.rgb;
			uint prev_bid = packHalf2x16(vec2(prev_val.a, 0.0));
			
			if (prev_bid == first_hit_bid)
				col = mix(prev_col, col, vec3(taa_alpha));
		}
		
		hit_id = first_hit_bid;
	}
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
	
	imageStore(img, ivec2(pxpos), vec4(col, unpackHalf2x16(hit_id).x));
}
