#version 450
#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec3	uvi; // uv + tex_index
	//float	brightness;
} vs;

#ifdef _VERTEX
	layout(location = 0) in vec3	pos_model;
	layout(location = 1) in vec2	uv;
	layout(location = 2) in int		tex_indx;

	layout(push_constant) uniform PC {
		vec3 chunk_pos;
	};

	//uniform vec3 chunk_pos;
	//uniform float sky_light_reduce;

	float brightness_function (float light) {
		return light * pow(1.0 / (2.0 - light), 4.0); 
	}

	void main () {
		gl_Position =		world_to_clip * vec4(pos_model + chunk_pos, 1);

		vs.uvi =		    vec3(uv, float(tex_indx));
		//vs.brightness =		brightness_function( max(block_light, sky_light - sky_light_reduce) );
	}
#endif

#ifdef _FRAGMENT
	layout(set = 0, binding = 1) uniform sampler2DArray textures;

	#define ALPHA_TEST

	#define ALPHA_TEST_THRES 127.0

	void main () {
		vec4 col = texture(textures, vs.uvi);
		
		//col.rgb *= vec3(vs.brightness);
		
	#ifdef ALPHA_TEST
		if (col.a <= ALPHA_TEST_THRES / 255.0)
			discard;
		col.a = 1.0;
	#endif

		frag_col = col;
	}
#endif