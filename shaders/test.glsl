#version 450

#ifdef _VERTEX
	//layout(location = 0) in vec2 a_pos;
	//layout(location = 1) in vec3 a_col;
	#include "sub/inc.glsl"

	layout(std140, set = 0, binding = 0)
	uniform View {
		// world space to cam space, ie. view transform
		mat4 world_to_cam;
		// cam space to world space, ie. inverse view transform
		mat4 cam_to_world;
		// cam space to clip space, ie. projection transform
		mat4 cam_to_clip;
		// clip space to cam space, ie. inverse projection transform
		mat4 clip_to_cam;
		// world space to clip space, ie. view projection transform, to avoid two matrix multiplies when they are not needed
		mat4 world_to_clip;
		// near clip plane distance (positive)
		float clip_near;
		// far clip plane distance (positive)
		float clip_far;
		// viewport size in pixels
		vec2 viewport_size;
	};

	layout(location = 0) out vec3 vs_color;

	void main () {
		gl_Position = world_to_clip * vec4(a_pos, 0.0, 1.0);
		vs_color = a_col;
	}
#endif

#ifdef _FRAGMENT
	layout(location = 0) in vec3 vs_color;
	layout(location = 0) out vec4 frag_color;

	void main () {
		frag_color = vec4(vs_color, 1.0);
	}
#endif
