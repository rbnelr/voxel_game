#include "camera.hpp"
#include "../input.hpp"

#include "assert.h"

Camera_View Camera::calc_view () {
	Camera_View v;

	float3x3 world_to_cam_rot = rotate3_Z(rot_aer.z) * rotate3_X(-rot_aer.y) * rotate3_Z(-rot_aer.x);
	float3x3 cam_to_world_rot = rotate3_Z(rot_aer.x) * rotate3_X(rot_aer.y) * rotate3_Z(-rot_aer.z);

	v.world_to_cam = world_to_cam_rot * translate(-pos);
	v.cam_to_world = translate(pos) * cam_to_world_rot;

	float aspect = (float)input.window_size.x / (float)input.window_size.y;

	if (mode == PERSPECTIVE) {
		v.cam_to_clip = perspective_matrix(vfov, aspect, clip_near, clip_far);
	} else {
		assert(mode == ORTHOGRAPHIC);

		v.cam_to_clip = orthographic_matrix(ortho_vsize, aspect, clip_near, clip_far);
	}

	return v;
}

float4x4 perspective_matrix (float vfov, float aspect, float clip_near, float clip_far) {
	float2 frust_scale;
	frust_scale.y = tan(vfov / 2);
	frust_scale.x = frust_scale.y * aspect;

	float hfov = atan(frust_scale.x) * 2;

	float2 frust_scale_inv = 1.0f / frust_scale;

	float x = frust_scale_inv.x;
	float y = frust_scale_inv.y;
	float a = (clip_far +clip_near) / (clip_near -clip_far);
	float b = (2.0f * clip_far * clip_near) / (clip_near -clip_far);

	return float4x4(
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, a, b,
		0, 0, -1, 0
	);
}

float4x4 orthographic_matrix (float vsize, float aspect, float clip_near, float clip_far) {
	float hsize = vsize * aspect;

	float x = 2.0f / hsize;
	float y = 2.0f / vsize;

	float a = -2.0f / (clip_far - clip_near);
	float b = clip_near * a - 1;

	return float4x4(
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, a, b,
		0, 0, 0, 1
	);
}

void rotate_camera (Camera* cam, float3 aer_delta, float down_limit, float up_limit) {
	cam->rot_aer -= aer_delta;

	cam->rot_aer.x = wrap(cam->rot_aer.x, deg(-180), deg(180));
	cam->rot_aer.y = clamp(cam->rot_aer.y, down_limit, deg(180.0f) - up_limit);
	cam->rot_aer.z = wrap(cam->rot_aer.z, deg(-180), deg(180));
}

void translate_camera (Camera* cam, Camera_View const& view, float3 translation_local) {
	cam->pos += (float3x3)view.cam_to_world * translation_local;
}

void rotate_camera_with_mouselook (Camera* cam, float2 raw_mouselook, float sensitiviy_divider) {
	float3 aer = 0;

	aer.x += raw_mouselook.x * cam->vfov / sensitiviy_divider;
	aer.y += raw_mouselook.y * cam->vfov / sensitiviy_divider;

	rotate_camera(cam, aer);
}

Camera_View Flycam::update () {

	auto delta = input.get_mouselook_delta();

	rotate_camera_with_mouselook(this, delta, input.mouselook_sensitiviy_divider);

	Camera_View v = calc_view();

	{ // movement speed
		float3 move_dir = 0;
		if (input.buttons[GLFW_KEY_A]           .is_down) move_dir.x -= 1;
		if (input.buttons[GLFW_KEY_D]           .is_down) move_dir.x += 1;
		if (input.buttons[GLFW_KEY_W]           .is_down) move_dir.z -= 1;
		if (input.buttons[GLFW_KEY_S]           .is_down) move_dir.z += 1;
		if (input.buttons[GLFW_KEY_LEFT_CONTROL].is_down) move_dir.y -= 1;
		if (input.buttons[GLFW_KEY_SPACE]       .is_down) move_dir.y += 1;

		move_dir = normalizesafe(move_dir);
		float move_speed = length(move_dir); // could be analog with gamepad

		if (move_speed == 0.0f)
			cur_speed = base_speed; // no movement ticks down speed

		if (input.buttons[GLFW_KEY_LEFT_SHIFT].is_down) {
			move_speed *= fast_multiplier;

			cur_speed += base_speed * speedup_factor * input.dt;
		}

		cur_speed = clamp(cur_speed, base_speed, max_speed);

		translate_camera(this, v, cur_speed * move_dir * input.dt);
	}

	{
		if (!input.buttons[GLFW_KEY_F].is_down) {
			float delta_log = 0.1f * input.mouse_wheel_delta;
			base_speed = powf(2, log2f(base_speed) +delta_log );
		} else {
			float delta_log = -0.1f * input.mouse_wheel_delta;
			vfov = clamp(powf(2, log2f(vfov) +delta_log ), deg(1.0f/10), deg(170));
		}
	}

	return v;
}
