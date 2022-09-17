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


uniform bool show_light = false;
uniform bool show_normals = false;

uniform float bounce_max_dist = 90.0;
uniform int bounce_max_count = 3;

uniform float roughness = 0.8;


layout(r32f,    binding = 0) writeonly restrict uniform image2D gbuf_pos;
layout(r16ui,   binding = 1) writeonly restrict uniform uimage2D gbuf_faceid;
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
	get_ray(vec2(pxpos), ray_pos, ray_dir);
	
	float depth = -1.0;
	uint faceid = 0xffffu;
	vec4 col  = vec4(0,0,0,0);
	vec3 norm = vec3(0,0,0);
	
	
	Hit hit;
	if (trace_ray(ray_pos, ray_dir, INF, hit, vec3(1,0,0),true)) {
		
		depth = pos_to_depth(hit.pos);
		
		faceid = hit.faceid;
		
		col.rgb = hit.col.rgb;
		col.a = hit.emiss_raw;
		
		norm = hit.normal;
	}
	
	GET_VISUALIZE_COST(col.rgb);
	
	imageStore(gbuf_pos, pxpos, vec4(depth, 0,0,0));
	imageStore(gbuf_faceid, pxpos, uvec4(faceid, 0,0,0));
	imageStore(gbuf_col, pxpos, col);
	imageStore(gbuf_norm, pxpos, vec4(norm, 0));
}
