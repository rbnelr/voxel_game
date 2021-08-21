#version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t, uint8_t

#if VISUALIZE_COST && VISUALIZE_WARP_COST
	#extension GL_KHR_shader_subgroup_arithmetic : enable
	#extension GL_ARB_shader_group_vote : enable
#endif

layout(local_size_x = WG_PIXELS_X, local_size_y = WG_PIXELS_Y) in;

#define DEBUGDRAW 0
#include "rt_util.glsl"

layout(rgba32f, binding = 0) writeonly restrict uniform image2D img_col;
//layout(rgba32f, binding = 0) writeonly restrict uniform image2D gbuf_pos ;
//layout(rgba16f, binding = 1) writeonly restrict uniform image2D gbuf_col ;
//layout(rgba16f, binding = 2) writeonly restrict uniform image2D gbuf_norm;

uniform ivec2 framebuf_size;

// Instead of executing work groups in a simple row major order
// reorder them into columns of width N (by returning a different 2d index)
// in each column the work groups are still row major order
// replicates this: https://developer.nvidia.com/blog/optimizing-compute-shaders-for-l2-locality-using-thread-group-id-swizzling/
uvec2 work_group_tiling (uint N) {
	#if 1
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

void main () {
	//ivec2 pxpos = ivec2(gl_GlobalInvocationID.xy);
	
	ivec2 threadid = ivec2(gl_LocalInvocationID.xy);
	ivec2 wgroupid = ivec2(work_group_tiling(16u));
	
	ivec2 pxpos = wgroupid * ivec2(WG_PIXELS_X,WG_PIXELS_Y) + threadid;
	
	#if DEBUGDRAW
	_dbgdraw = update_debugdraw && pxpos.x == framebuf_size.x/2 && pxpos.y == framebuf_size.y/2;
	#endif
	INIT_VISUALIZE_COST
	
	vec3 ray_pos, ray_dir;
	bool bray = get_ray(vec2(pxpos), ray_pos, ray_dir);
	
	vec4 col = vec4(0.0);
	
	Hit hit;
	if (bray && trace_ray(ray_pos, ray_dir, INF, hit)) {
		col = hit.col;
	}
	
	//uint start_bid = read_bid_octree(ivec3(floor(ray_pos)));
	//
	//vec3 pos  = vec3(-1000.0);
	//vec4 col  = vec4(0.0);
	//vec3 norm = vec3(0.0);
	//vec3 tang = vec3(0.0);
	//
	//Hit hit;
	//if (bray && trace_ray(ray_pos, ray_dir, INF, start_bid, hit, RAYT_PRIMARY)) {
	//	pos     = hit.pos;
	//	col.rgb = hit.col;
	//	col.a   = hit.emiss;
	//	norm    = hit.TBN[2];
	//	//tang    = hit.TBN[0];
	//}
	
	GET_VISUALIZE_COST(col.rgb)
	imageStore(img_col, pxpos, col);
	//imageStore(gbuf_pos , pxpos, vec4(pos, 0.0));
	//imageStore(gbuf_col , pxpos, col);
	//imageStore(gbuf_norm, pxpos, vec4(norm, 0.0));
}