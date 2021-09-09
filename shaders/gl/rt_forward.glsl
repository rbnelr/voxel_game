#version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t, uint8_t

#extension GL_ARB_shader_group_vote : enable

#if VISUALIZE_COST && VISUALIZE_TIME
	#extension GL_ARB_shader_clock : enable
#endif

layout(local_size_x = WG_PIXELS_X, local_size_y = WG_PIXELS_Y) in;

#define DEBUGDRAW 1
#include "rt_util.glsl"

layout(rgba32f, binding = 0) writeonly restrict uniform image2D img_col;
//layout(rgba32f, binding = 0) writeonly restrict uniform image2D gbuf_pos ;
//layout(rgba16f, binding = 1) writeonly restrict uniform image2D gbuf_col ;
//layout(rgba16f, binding = 2) writeonly restrict uniform image2D gbuf_norm;

uniform int rand_seed_time = 0;
uniform ivec2 framebuf_size;

// Instead of executing work groups in a simple row major order
// reorder them into columns of width N (by returning a different 2d index)
// in each column the work groups are still row major order
// replicates this: https://developer.nvidia.com/blog/optimizing-compute-shaders-for-l2-locality-using-thread-group-id-swizzling/
uvec2 work_group_tiling (uint N) {
	#if 0
	return gl_WorkGroupID.xy;
	#else
	uint idx = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
	
	uint column_size       = gl_NumWorkGroups.y * N;
	uint full_column_count = gl_NumWorkGroups.x / N;
	uint last_column_width = gl_NumWorkGroups.x % N;
	
	uint column_idx = idx / column_size;
	uint idx_in_column = idx % column_size;
	
	uint column_width = N;
	if (column_idx == full_column_count)
		column_width = last_column_width;
	
	uvec2 wg_swizzled;
	wg_swizzled.y = idx_in_column / column_width;
	wg_swizzled.x = idx_in_column % column_width + column_idx * N;
	return wg_swizzled;
	#endif
}

uniform bool show_light = false;

uniform float bounce_dist = 90.0;
uniform int bounce_limit = 3;

void main () {
	INIT_VISUALIZE_COST();
	
	ivec2 threadid = ivec2(gl_LocalInvocationID.xy);
	ivec2 wgroupid = ivec2(work_group_tiling(20u));
	
	ivec2 pxpos = wgroupid * ivec2(WG_PIXELS_X,WG_PIXELS_Y) + threadid;
	
	srand(pxpos.x, pxpos.y, rand_seed_time);
	
	#if DEBUGDRAW
	_dbgdraw = update_debugdraw && pxpos.x == framebuf_size.x/2 && pxpos.y == framebuf_size.y/2;
	#endif
	
	vec3 ray_pos, ray_dir;
	bool bray = get_ray(vec2(pxpos), ray_pos, ray_dir);
	
	vec4 col = vec4(0.0);
	
	Hit hit;
	if (bray && trace_ray(ray_pos, ray_dir, INF, hit, vec3(1,0,0))) {
		//#if DEBUGDRAW // visualize tangent space
	//if (_dbgdraw) dbgdraw_vector(hit.pos - WORLD_SIZEf/2.0, TBN[0]*0.3, vec4(1,0,0,1));
	//if (_dbgdraw) dbgdraw_vector(hit.pos - WORLD_SIZEf/2.0, TBN[1]*0.3, vec4(0,1,0,1));
	//if (_dbgdraw) dbgdraw_vector(hit.pos - WORLD_SIZEf/2.0, TBN[2]*0.3, vec4(0,0,1,1));
	//#endif
		
		if (show_light) hit.col.rgb = vec3(1.0);
		
	#if BOUNCE_ENABLE
		vec3 light = vec3(0.0);
		
		vec3 A = vec3(1.0);
		vec3 albedo = hit.col.rgb;
		
		float dist_remain = bounce_dist;
		
		vec3 normal = hit.normal;
		vec3 pos = hit.pos + normal * epsilon;
		
		for (int i=0; i<bounce_limit; ++i) {
			
			vec3 dir;
			
			const float roughness = 0.001;
			
			float F = fresnel_roughness(max(0., -dot(normal, ray_dir)), .04, roughness);
			if (F > rand()) {
				dir = reflect_roughness(reflect(ray_dir, normal), normal, roughness);
			} else {
				A *= albedo;
				dir = generate_TBN(normal) * hemisphere_sample();
			}
			
			Hit hit2;
			if (trace_ray(pos, dir, bounce_dist, hit2, vec3(0,0,1))) {
				light += A * hit2.emiss;
				albedo = hit2.col.rgb;
				
				dist_remain -= hit2.dist;
				if (dist_remain <= 0.0 || max(max(A.x,A.y),A.z) < 0.02)
					break;
				
				normal = hit2.normal;
				pos = hit2.pos + normal * epsilon;
				
			} else {
				break;
			}
		}
		
		light = APPLY_TAA(light, hit.pos, hit.normal, pxpos);
		
		col.rgb = light + hit.emiss;
		col.a = hit.col.a;
	#else
		col = hit.col;
	#endif
	}
	
	GET_VISUALIZE_COST(col.rgb);
	imageStore(img_col, pxpos, col);
}