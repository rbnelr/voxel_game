
layout(std140) uniform View {
	// world space to cam space, ie. view transform
	mat4 world_to_cam;
	// cam space to world space, ie. inverse view transform
	mat4 cam_to_world;
	// cam space to clip space, ie. projection transform
	mat4 cam_to_clip;
	// viewport size in pixels
	vec2 viewport_size;
};

layout(std140) uniform Debug {
	// mouse cursor pos in pixels
	vec2 cursor_pos;
};

float map (float x, float a, float b) {
	return (x - a) / (b - a);
}
