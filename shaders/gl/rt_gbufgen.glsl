#version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t, uint8_t

layout(local_size_x = WG_PIXELS_X, local_size_y = WG_PIXELS_Y) in;

#include "rt_util.glsl"

layout(rgba32f, binding = 0) writeonly restrict uniform image2D gbuf_pos ;
layout(rgba16f, binding = 1) writeonly restrict uniform image2D gbuf_col ;
layout(rgba16f, binding = 2) writeonly restrict uniform image2D gbuf_norm;
layout(rgba16f, binding = 3) writeonly restrict uniform image2D gbuf_tang;

void main () {
	ivec2 pxpos = ivec2(gl_GlobalInvocationID.xy);
	
	vec3 ray_pos, ray_dir;
	bool bray = get_ray(vec2(pxpos), ray_pos, ray_dir);
	
	uint start_bid = read_bid(ivec3(floor(ray_pos)));
	
	vec3 pos  = vec3(-1000.0);
	vec4 col  = vec4(0.0);
	vec3 norm = vec3(0.0);
	vec3 tang = vec3(0.0);
	
	Hit hit;
	if (bray && trace_ray(ray_pos, ray_dir, INF, start_bid, hit, RAYT_PRIMARY)) {
		pos     = hit.pos;
		col.rgb = hit.col;
		col.a   = hit.emiss;
		norm    = hit.TBN[2];
		tang    = hit.TBN[0];
	}
	
	imageStore(gbuf_pos , pxpos, vec4(pos, 0.0));
	imageStore(gbuf_col , pxpos, col);
	imageStore(gbuf_norm, pxpos, vec4(norm, 0.0));
	imageStore(gbuf_tang, pxpos, vec4(tang, 0.0));
}
