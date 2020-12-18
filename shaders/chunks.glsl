#version 460 // for GL_ARB_shader_draw_parameters

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec3	uvi; // uv + tex_index
	//float	brightness;
} vs;

#ifdef _VERTEX
	#define FIXEDPOINT_FAC (1.0 / 256.0)
	
	layout(location = 0) in vec3	pos;
	layout(location = 1) in vec2	uv;
	layout(location = 2) in float	texid;
	
	//
	layout(push_constant) uniform PC {
		vec3 chunk_pos;
	};
	
	void main () {
		gl_Position =		world_to_clip * vec4(pos + chunk_pos, 1);
		vs.uvi =		    vec3(uv, texid);
	}
#endif

#ifdef _FRAGMENT
	#define ALPHA_TEST_THRES 127.0

	layout(set = 0, binding = 2) uniform sampler2DArray textures;

	void main () {
		vec4 col = texture(textures, vs.uvi);
		
	#if ALPHA_TEST && !defined(WIREFRAME)
		if (col.a <= ALPHA_TEST_THRES / 255.0)
			discard;
		col.a = 1.0;
	#endif
		
	#if WIREFRAME
		col = vec4(1.0);
	#endif
		
		frag_col = col;
	}
#endif
