#include "util.glsl"

struct View {
	// forward
	mat4 world_to_clip;
	mat4 world_to_cam;
	mat4 cam_to_clip;
	// inverse
	mat4 clip_to_world;
	mat4 clip_to_cam;
	mat4 cam_to_world;

	// near clip plane distance (positive)
	float clip_near;
	// far clip plane distance (positive)
	float clip_far;
	// viewport size in pixels
	vec2 viewport_size;
};

layout(std140, binding = 0) uniform Common {
	View view;
};
