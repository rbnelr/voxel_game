#version 450
#include "util.glsl"

layout(location = 0) vs2fs VS {
	vec2 uv;
} vs;

#ifdef _VERTEX
	void main () {
		vs.uv =		        vec2(gl_VertexIndex & 2, (gl_VertexIndex << 1) & 2);
		gl_Position =		vec4(vs.uv * 2.0 - 1.0, 0.0, 1.0);
}
#endif

#ifdef _FRAGMENT
	layout(set = 1, binding = 0) uniform sampler2D main_albedo;
	layout(set = 1, binding = 1) uniform sampler2D ssao;

	layout(location = 0) out vec4 frag_col;
	void main () {
	#if 1
		vec4 col = texture(main_albedo, vs.uv);
		col.rgb *= col.a * texture(ssao, vs.uv).rrr;
		frag_col = col;
	#else
		frag_col = vec4(texture(ssao, vs.uv).rrr, 1.0);
	#endif
	}
#endif
