#include "util.glsl"

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

	/*
	//	// mouse cursor pos in pixels
	//	vec2 cursor_pos;
	//
	//	int wireframe;
	//	// 0 -> off   !=0 -> on
	//	// bit 1 = shaded
	//	// bit 2 = colored
	*/
};
