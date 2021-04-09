#version 460 core
#if VISUALIZE_COST && VISUALIZE_WARP_COST
	#extension GL_KHR_shader_subgroup_arithmetic : enable
	#extension GL_ARB_shader_group_vote : enable
#endif

layout(local_size_x = LOCAL_SIZE_X, local_size_y = LOCAL_SIZE_Y) in;

#define RAND_SEED_TIME 1

#include "rt_util.glsl"

layout(rgba16f, binding = 4) uniform image2D img;
uniform sampler2D prev_framebuffer;

uniform mat4 prev_world2clip;

uniform float taa_alpha = 0.05;

uniform uint rand_frame_index = 0;
uniform ivec2 dispatch_size;

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
	if (pxpos.x >= view.viewport_size.x || pxpos.y >= view.viewport_size.y)
		return;
	
	_dbg_ray = update_debug_rays && pxpos.x == uint(view.viewport_size.x)/2 && pxpos.y == uint(view.viewport_size.y)/2;
	if (_dbg_ray) line_drawer_init();
	
	srand(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y, rand_frame_index);
	
	#if ONLY_PRIMARY_RAYS
	vec3 ray_pos, ray_dir;
	bool bray = get_ray(vec2(pxpos), ray_pos, ray_dir);
	
	Hit hit;
	bool did_hit = bray && trace_ray(ray_pos, ray_dir, INF, hit);
	vec3 col = did_hit ? hit.col : vec3(0.0);
	
	#else
	// primary ray
	vec3 ray_pos, ray_dir;
	bool bray = get_ray(vec2(pxpos), ray_pos, ray_dir);
	
	vec3 col = vec3(0.0);
	
	Hit hit;
	bool did_hit = bray && trace_ray_refl_refr(ray_pos, ray_dir, INF, hit);
	if (did_hit) {
		vec3 pos = hit.pos + hit.normal * 0.001;
		
		vec3 light = ambient_light;
		light += collect_sunlight(pos, hit.normal);
		
		if (bounces_enable) {
			
			float max_dist = bounces_max_dist;
			
			vec3 cur_normal = hit.normal;
			vec3 contrib = vec3(1.0);
			
			for (int j=0; j<bounces_max_count; ++j) {
				vec3 dir = get_tangent_to_world(cur_normal) * hemisphere_sample(); // already cos weighted
				
				Hit hit2;
				if (!trace_ray_refl_refr(pos, dir, max_dist, hit2))
					break;
				
				pos = hit2.pos + hit2.normal * 0.001;
				max_dist -= hit2.dist;
				
				cur_normal = hit2.normal;
				
				vec3 light2 = collect_sunlight(pos, cur_normal);
				
				light += (hit2.emiss + hit2.col * light2) * contrib;
				contrib *= hit2.col;
			}
		}
		
		if (visualize_light)
			hit.col = vec3(1.0);
		
		col += hit.emiss + hit.col * light;
	}
	#endif
	
	uint hit_id = 0;
	if (did_hit) {
		vec4 prev_clip = prev_world2clip * vec4(hit.pos, 1.0);
		prev_clip.xyz /= prev_clip.w;
		
		vec2 uv = prev_clip.xy * 0.5 + 0.5;
		if (all(greaterThan(uv, vec2(0.0))) && all(lessThan(uv, vec2(1.0)))) {
			vec4 prev_val = texture(prev_framebuffer, uv);
			
			vec3 prev_col = prev_val.rgb;
			uint prev_bid = packHalf2x16(vec2(prev_val.a, 0.0));
			
			if (prev_bid == hit.bid)
				col = mix(prev_col, col, vec3(taa_alpha));
		}
		
		hit_id = hit.bid;
	}

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
