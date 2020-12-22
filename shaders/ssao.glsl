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
	//layout(set = 1, binding = 1) uniform sampler2D src_image;
	//layout(std140, set = 1, binding = 2) uniform Uniforms {
	//
	//}
	
	vec2 blur_kernel[4] = {
		vec2(-1,0),
		vec2(+1,0),
		vec2(0,-1),
		vec2(0,+1),
	};
	
	void main () {
		vec3 col = texture(src_image, vs.uv).rgb;
		for (int i=0; i<4; ++i)
			col += texture(src_image, vs.uv + blur_kernel[i]*vec2(0.01)).rgb;
		frag_col = vec4(col / vec3(5), 1.0);
	}
#endif
