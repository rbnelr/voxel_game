#version 450
#define NO_VIEW
#include "common.glsl"

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
	layout(set = 1, binding = 0) uniform sampler2D src_image;
	void main () {
		frag_col = texture(src_image, vs.uv);
	}
#endif
