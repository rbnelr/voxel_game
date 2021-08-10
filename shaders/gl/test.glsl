#version 460 // for GL_ARB_shader_draw_parameters

#include "common.glsl"
#include "gpu_voxels.glsl"

layout(location = 0) vs2fs VS {
	vec3	pos;
	vec3	norm;
	vec4	col;
	vec2	uv;
} vs;

#ifdef _VERTEX
	layout (location = 0) in vec3 pos;
	layout (location = 1) in vec3 norm;
	layout (location = 2) in vec2 uv;
	layout (location = 3) in vec4 col;
	
	uniform mat4 model2world;

	void main () {
		gl_Position =		view.world_to_clip * (model2world * vec4(pos, 1));
		vs.pos =			(model2world * vec4(pos, 1)).xyz;
		vs.norm =			mat3(model2world) * norm;
		vs.uv =				uv;
		vs.col =			col;
	}
#endif

#ifdef _FRAGMENT
	layout(location = 0) out vec4 frag_pos;
	layout(location = 1) out vec4 frag_col;
	layout(location = 2) out vec4 frag_norm;
	
	void main () {
		frag_pos = vec4(vs.pos + float(WORLD_SIZE)/2.0, 0.0);
		frag_col = vec4(0.9,0.9,0.9, 0.0);
		frag_norm = vec4(normalize(vs.norm), 0.0);
	}
#endif
