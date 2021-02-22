#version 460 core // for GL_ARB_shader_draw_parameters

#include "common.glsl"

layout(local_size_x = 1, local_size_y = 1) in;

//layout(std430, binding = 1) buffer Test {
//	vec4 cols[];
//};

layout(rgba16f, binding = 2) uniform image2D img;

vec4 g_col = vec4(0,0,0,0);
bool _debug_col = false;

void DEBUG (vec4 col) {
	if (!_debug_col) {
		g_col = col;
		_debug_col = true;
	}
}

// get pixel ray in world space based on pixel coord and matricies
void get_ray (vec2 px_pos, out vec3 ray_pos, out vec3 ray_dir) {

	//vec2 px_jitter = rand2(gl_FragCoord.xy) - 0.5;
	vec2 px_jitter = vec2(0.0);

	vec2 ndc = (px_pos + 0.5 + px_jitter) / view.viewport_size * 2.0 - 1.0;
	vec4 clip = vec4(ndc, -1, 1) * view.clip_near; // ndc = clip / clip.w;

	// TODO: can't get optimization to one single clip_to_world mat mul to work, why?
	vec3 cam = (view.clip_to_cam * clip).xyz;

	ray_pos = (view.cam_to_world * vec4(cam, 1)).xyz;
	ray_dir = (view.cam_to_world * vec4(cam, 0)).xyz;
	ray_dir = normalize(ray_dir);

	DEBUG(vec4(ray_dir, 1));
	//ray_pos -= vec3(svo_root_pos);
}

vec4 trace_pixel (vec2 px_pos) {
	vec3 ray_pos, ray_dir;
	get_ray(px_pos, ray_pos, ray_dir);

	return vec4(ray_dir, 1.0);
}

void main () {
	vec2 pos = gl_GlobalInvocationID.xy;

	vec4 col = trace_pixel(pos);
	//col.rg = pos / view.viewport_size;

	if (_debug_col)
		col = g_col;
	imageStore(img, ivec2(pos), col);
}
