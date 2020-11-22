#version 450
#extension GL_ARB_shader_draw_parameters : enable

#include "common.glsl"

layout(location = 0) vs2fs VS {
	vec3	uvi; // uv + tex_index
	//float	brightness;
} vs;

#ifdef _VERTEX
	layout(location = 0) in vec3	v_pos; // pos of voxel instance in chunk
	layout(location = 1) in uint	v_meshid;
	layout(location = 2) in float	v_texid;

	struct PerDraw {
		vec4 chunk_pos;
	};
	layout(std140, set = 1, binding = 0) uniform PerDrawData {
		#define MAX_UBO_SIZE (64*1024)
		#define MAX_PER_DRAW (MAX_UBO_SIZE / 16) // sizeof(PerDrawData)

		PerDraw per_draw[MAX_PER_DRAW];
	};

	const vec3 pos[6][4] = {
		{ vec3(0,1,0), vec3(0,0,0), vec3(0,0,1), vec3(0,1,1) },
		{ vec3(1,0,0), vec3(1,1,0), vec3(1,1,1), vec3(1,0,1) },
		{ vec3(0,0,0), vec3(1,0,0), vec3(1,0,1), vec3(0,0,1) },
		{ vec3(1,1,0), vec3(0,1,0), vec3(0,1,1), vec3(1,1,1) },
		{ vec3(0,1,0), vec3(1,1,0), vec3(1,0,0), vec3(0,0,0) },
		{ vec3(0,0,1), vec3(1,0,1), vec3(1,1,1), vec3(0,1,1) }
	};
	const vec2 uv[4] = { vec2(0,1), vec2(1,1), vec2(1,0), vec2(0,0) };

	const int indices[6] = {
		0,1,3, 3,1,2,
	};

	void main () {
		int idx = indices[gl_VertexIndex];
		vec3 pos_model	= pos[v_meshid][idx];
		vec2 uv			= uv[idx];

		vec3 chunk_pos = per_draw[gl_DrawIDARB].chunk_pos.xyz;

		gl_Position =		world_to_clip * vec4(pos_model + v_pos + chunk_pos, 1);

		vs.uvi =		    vec3(uv, v_texid);
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
