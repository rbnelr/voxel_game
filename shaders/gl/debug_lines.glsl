#version 460 core

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec4 col;
	
#if DRAW_OCCLUDED
	flat vec2 pos_start;
	     vec2 pos_vert;
#endif
} vs;

#ifdef _VERTEX
	layout(location = 0) in vec3 pos_world;
	layout(location = 1) in vec4 color;

	void main () {
		vec4 pos_clip = view.world_to_clip * vec4(pos_world, 1);
		
		gl_Position = pos_clip;
		vs.col = color;
		
	#if DRAW_OCCLUDED
		vs.pos_vert = pos_clip.xy / pos_clip.w;
		vs.pos_start = vs.pos_vert;
	#endif
	}
#endif

#ifdef _FRAGMENT

#if DRAW_OCCLUDED
	uniform float occluded_alpha = 0.3;
	uniform float stipple_factor = 4.0;
	uniform uint stipple_pattern = 0xbbbb;//0xf99f;// 0xCCCC;
#endif
	
	layout(location = 0) out vec4 frag_col;
	void main () {
		vec4 col = vs.col;
		
	#if DRAW_OCCLUDED
		{
			vec2 offs = (vs.pos_vert - vs.pos_start) * (view.viewport_size / 2.0);
			float dist = length(offs);
			
			uint bit = uint(round(dist / stipple_factor)) & 15U;
			if ((stipple_pattern & (1U << bit)) == 0U)
				discard;
		}
		col.a = occluded_alpha;
	#endif
	
		frag_col = col;
	}
#endif
