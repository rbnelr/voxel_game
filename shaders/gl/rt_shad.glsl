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

uniform bool show_normals = false;

layout(rgba16f, binding = 0) writeonly restrict uniform image2D rt_col;

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
	get_ray(vec2(pxpos), ray_pos, ray_dir);
	
	vec4 col = vec4(0,0,0,1);
	
	Hit hit;
	if (trace_ray(ray_pos, ray_dir, INF, hit, vec3(1,0,0),true)) {
		
		col = vec4(hit.col.rgb, hit.emiss_raw);
		if (show_normals) col.rgb = hit.normal * 0.5 + 0.5;
	}
	
	GET_VISUALIZE_COST(col.rgb);
	
	imageStore(rt_col, pxpos, col);
}
