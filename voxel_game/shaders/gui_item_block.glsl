#version 330 core

$include "common.glsl"

$if vertex
	layout (location = 0) in vec3	pos;
	layout (location = 1) in vec3	normal;
	layout (location = 2) in vec2	uv;
	layout (location = 3) in float	tex_indx;
	
	out vec3	vs_normal;
	out vec2	vs_uv;
	out float	vs_tex_indx;

	void main () {
		gl_Position =		vec4(pos.xy / viewport_size * 2 - 1, 0, 1);

		vs_normal =			normal;
		vs_uv =		        uv;
		vs_tex_indx =		tex_indx;
	}
$endif

$if fragment
	in vec3		vs_normal;
	in vec2	    vs_uv;
	in float	vs_tex_indx;

	uniform	sampler2DArray tile_textures;
	
	const vec3 light_dir = normalize(vec3(0.1,0.4,1));

	void main () {
		vec4 col = texture(tile_textures, vec3(vs_uv, vs_tex_indx));
		col.rgb *= max(dot(normalize(vs_normal), light_dir), 0.0) * 0.7 + 0.3;

		frag_col = col;
	}
$endif
