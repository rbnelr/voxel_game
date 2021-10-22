#version 460 core
#extension GL_NV_gpu_shader5 : enable // for uint16_t, uint8_t
//#extension GL_ARB_shader_group_vote : enable

#if VISUALIZE_COST && VISUALIZE_TIME
	#extension GL_ARB_shader_clock : enable
#endif

#include "fullscreen_triangle.glsl"
#ifdef _FRAGMENT
	#include "rt_util.glsl"

	uniform int rand_seed_time = 0;
	uniform ivec2 framebuf_size;

	layout(location=0) out vec4 frag_pos;
	layout(location=1) out vec4 frag_col;
	layout(location=2) out vec3 frag_norm;

	void main () {
		INIT_VISUALIZE_COST();
		
		ivec2 pxpos = ivec2(gl_FragCoord.xy);
		
		//srand(pxpos.x, pxpos.y, rand_seed_time);
		
		#if DEBUGDRAW
		_dbgdraw = update_debugdraw && pxpos.x == framebuf_size.x/2 && pxpos.y == framebuf_size.y/2;
		#endif
		
		vec3 ray_pos, ray_dir;
		bool bray = get_ray(vec2(pxpos), ray_pos, ray_dir);
		
		float depth = INF;
		vec3 pos  = vec3(-100.0);
		vec4 col  = vec4(0.0);
		vec3 norm = vec3(0.0);
		
		Hit hit;
		if (bray && trace_ray(ray_pos, ray_dir, INF, near_plane_dist, hit, vec3(1,0,0),true)) {
			vec4 clip = view.world_to_clip * vec4(hit.pos, 1.0);
			float ndc_depth = clip.z / clip.w;
			
			gl_FragDepth = ((gl_DepthRange.diff * ndc_depth) + gl_DepthRange.near + gl_DepthRange.far) / 2.0;
			
			pos = hit.pos;
			col = vec4(hit.col.rgb, hit.emiss_raw);
			norm = hit.normal;
		}
		
		GET_VISUALIZE_COST(col.rgb);
		
		gl_FragDepth = 0.5; // why is this not working?
		frag_pos = vec4(pos, 0.0);
		frag_col = col;
		frag_norm = norm;
	}
#endif
