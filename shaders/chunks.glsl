#version 450
#include "common.glsl"

#ifdef _VERTEX
	layout(location = 0) in vec3	pos_model;
	layout(location = 1) in vec2	uv;
	layout(location = 2) in int		tex_indx;

	//uniform vec3 chunk_pos;
	//uniform float sky_light_reduce;

	layout(location = 0) out VS {
		vec2	uv;
		float	tex_indx;
		//float	brightness;
	} vs;

	float brightness_function (float light) {
		return light * pow(1.0 / (2.0 - light), 4.0); 
	}

	void main () {
		gl_Position =		world_to_clip * vec4(pos_model/* + chunk_pos*/, 1);

		vs.uv =		        uv;
		vs.tex_indx =		float(tex_indx);
		//vs.brightness =		brightness_function( max(block_light, sky_light - sky_light_reduce) );
	}
#endif

#ifdef _FRAGMENT
	layout(location = 0) in VS {
		vec2	uv;
		float	tex_indx;
		//float	brightness;
	} vs;

	//uniform	sampler2DArray tile_textures;

	#define ALPHA_TEST

	#define ALPHA_TEST_THRES 127.0

	void main () {
		vec4 col = vec4(vs.uv, 0.0, 1.0);
		//vec4 col = texture(tile_textures, vec3(vs.uv, vs.tex_indx));
		
		//col.rgb *= vec3(vs.brightness);
		
	#ifdef ALPHA_TEST
		if (col.a <= ALPHA_TEST_THRES / 255.0)
			DISCARD();
		col.a = 1.0;
	#endif

		FRAG_COL(col);
	}
#endif
