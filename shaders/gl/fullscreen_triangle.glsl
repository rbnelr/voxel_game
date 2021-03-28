
#ifdef _VERTEX
	layout(location = 0) out vec2 vs_uv;

	void main () { \
		gl_Position = vec4(vec2(gl_VertexID & 1, gl_VertexID >> 1) * 4.0 - 1.0, 0.0, 1.0);
		vs_uv = vec2(vec2(gl_VertexID & 1, gl_VertexID >> 1) * 2.0);
	}
#endif

#ifdef _FRAGMENT
	layout(location = 0) in vec2 vs_uv;
#endif
