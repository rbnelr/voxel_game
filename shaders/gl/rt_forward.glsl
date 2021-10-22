#version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t, uint8_t
//#extension GL_ARB_shader_group_vote : enable

#if VISUALIZE_COST && VISUALIZE_TIME
	#extension GL_ARB_shader_clock : enable
#endif

layout(local_size_x = WG_PIXELS_X, local_size_y = WG_PIXELS_Y) in;

#include "rt_util.glsl"

uniform int rand_seed_time = 0;
uniform ivec2 framebuf_size;

layout(rgba32f, binding = 0) writeonly restrict uniform image2D gbuf_depth;
layout(rgba32f, binding = 1) writeonly restrict uniform image2D gbuf_pos;
layout(rgba16f, binding = 2) writeonly restrict uniform image2D gbuf_col;
layout(rgba16f, binding = 3) writeonly restrict uniform image2D gbuf_norm;

void main () {
	INIT_VISUALIZE_COST();
	
	ivec2 threadid = ivec2(gl_LocalInvocationID.xy);
	ivec2 wgroupid = ivec2(work_group_tiling(20u));
	
	ivec2 pxpos = wgroupid * ivec2(WG_PIXELS_X,WG_PIXELS_Y) + threadid;
	
	//srand(pxpos.x, pxpos.y, rand_seed_time);
	
	#if DEBUGDRAW
	_dbgdraw = update_debugdraw && pxpos.x == framebuf_size.x/2 && pxpos.y == framebuf_size.y/2;
	#endif
	
	vec3 ray_pos, ray_dir;
	bool bray = get_ray(vec2(pxpos), ray_pos, ray_dir);
	
	float depth = INF;
	vec3 pos  = vec3(-100.0);
	vec4 col  = vec4(1,0,0,1);
	vec3 norm = vec3(0.0);
	
	//Hit hit;
	//if (bray && trace_ray(ray_pos, ray_dir, INF, near_plane_dist, hit, vec3(1,0,0),true)) {
	//	vec4 clip = view.world_to_clip * vec4(hit.pos, 1.0);
	//	float ndc_depth = clip.z / clip.w;
	//	
	//	//gl_FragDepth = ((gl_DepthRange.diff * ndc_depth) + gl_DepthRange.near + gl_DepthRange.far) / 2.0;
	//	
	//	pos = hit.pos;
	//	col = vec4(hit.col.rgb, hit.emiss_raw);
	//	norm = hit.normal;
	//}
	
	GET_VISUALIZE_COST(col.rgb);
	
	//gl_FragDepth = -0.5; // why is this not working?
	imageStore(gbuf_pos, pxpos, vec4(pos, 0.0));
	imageStore(gbuf_col, pxpos, col);
	imageStore(gbuf_pos, pxpos, vec4(norm, 0.0));
}
