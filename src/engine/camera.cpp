#include "common.hpp"
#include "camera.hpp"
#include "input.hpp"

void Camera_View::calc_frustrum () {
	// frustrum_corners set to cam space by perspective_matrix() or orthographic_matrix()

	for (int i=0; i<8; ++i)
		frustrum.corners[i] = cam_to_world * frustrum.corners[i];

	//near, left, right, bottom, up, far

	auto& corn = frustrum.corners;

	frustrum.planes[0] = {
		(corn[0] + corn[2]) / 2,
		normalize(cross(corn[2] - corn[1], corn[0] - corn[1]))
	};

	frustrum.planes[1] = {
		(corn[0] + corn[3]) / 2,
		normalize(cross(corn[3] - corn[0], corn[4] - corn[0]))
	};

	frustrum.planes[2] = {
		(corn[1] + corn[2]) / 2,
		normalize(cross(corn[1] - corn[2], corn[6] - corn[2]))
	};

	frustrum.planes[3] = {
		(corn[0] + corn[1]) / 2,
		normalize(cross(corn[0] - corn[1], corn[5] - corn[1]))
	};

	frustrum.planes[4] = {
		(corn[3] + corn[2]) / 2,
		normalize(cross(corn[2] - corn[3], corn[7] - corn[3]))
	};

	frustrum.planes[5] = {
		(corn[4] + corn[6]) / 2,
		normalize(cross(corn[7] - corn[4], corn[5] - corn[4]))
	};
}

float4x4 Camera::calc_cam_to_clip (int2 viewport_size, float4x4* clip_to_cam, View_Frustrum* frust, float2* frust_size) {
	float aspect = (float)viewport_size.x / (float)viewport_size.y;

	if (mode == PERSPECTIVE) {
		return perspective_matrix(vfov, aspect, clip_near, clip_far, clip_to_cam, frust, frust_size);
	} else {
		assert(mode == ORTHOGRAPHIC);

		return orthographic_matrix(ortho_vsize, aspect, clip_near, clip_far, clip_to_cam, frust, frust_size);
	}
}

// https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/
float4x4 perspective_matrix (float vfov, float aspect, float clip_near, float clip_far, float4x4* clip_to_cam, View_Frustrum* frust, float2* frust_size) {
	float2 frust_scale;
	frust_scale.y = tan(vfov / 2);
	frust_scale.x = frust_scale.y * aspect;

	float hfov = atan(frust_scale.x) * 2;

	float2 frust_scale_inv = 1.0f / frust_scale;

	float x = frust_scale_inv.x;
	float y = frust_scale_inv.y;

	float a, b;
	// use_reverse_depth with use infinite far plane
	// visible range on z axis in opengl and vulkan goes from -near to -inf
	// depth formula is: (with input w=1) z' = near, w' = -z  ->  depth = near/-z
	// depth values go from 1 at the near plane to 0 at infinity
	// inverse formula ist -z = near / depth
	clip_far = 1000000.0f; // can't actually set far to be infinite if I want frustrum culling to work without modification
	a = 0.0f;
	b = clip_near;

	//a = (clip_far + clip_near) / (clip_near - clip_far);
	//b = (2.0f * clip_far * clip_near) / (clip_near - clip_far);
	
	if (frust) {
		frust->corners[0] = float3(-frust_scale.x * clip_near, -frust_scale.y * clip_near, -clip_near);
		frust->corners[1] = float3(+frust_scale.x * clip_near, -frust_scale.y * clip_near, -clip_near);
		frust->corners[2] = float3(+frust_scale.x * clip_near, +frust_scale.y * clip_near, -clip_near);
		frust->corners[3] = float3(-frust_scale.x * clip_near, +frust_scale.y * clip_near, -clip_near);
		frust->corners[4] = float3(-frust_scale.x * clip_far , -frust_scale.y * clip_far , -clip_far );
		frust->corners[5] = float3(+frust_scale.x * clip_far , -frust_scale.y * clip_far , -clip_far );
		frust->corners[6] = float3(+frust_scale.x * clip_far , +frust_scale.y * clip_far , -clip_far );
		frust->corners[7] = float3(-frust_scale.x * clip_far , +frust_scale.y * clip_far , -clip_far );
	}
	if (frust_size) {
		*frust_size = frust_scale * 2.0f * clip_near;
	}
	if (clip_to_cam) {
		*clip_to_cam = float4x4(
			1.0f/x,      0,      0,       0,
			     0, 1.0f/y,      0,       0,
			     0,      0,      0,      -1,
			     0,      0, 1.0f/b,     a/b
		);
	}
	return float4x4(
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, a, b,
		0, 0, -1, 0
	);
}

float4x4 orthographic_matrix (float vsize, float aspect, float clip_near, float clip_far, float4x4* clip_to_cam, View_Frustrum* frust, float2* frust_size) {
	float hsize = vsize * aspect;

	float x = 2.0f / hsize;
	float y = 2.0f / vsize;

	float a, b;
	// TODO: why use clip_far here? because I can't divide z while not dividing x and y?
	a = 1.0f / (clip_far - clip_near);
	b = clip_near * a + 1.0f;

	// a = -2.0f / (clip_far - clip_near);
	// b = clip_near * a - 1;

	if (frust) {
		frust->corners[0] = float3(1.0f / -x, 1.0f / -y, -clip_near);
		frust->corners[1] = float3(1.0f / +x, 1.0f / -y, -clip_near);
		frust->corners[2] = float3(1.0f / +x, 1.0f / +y, -clip_near);
		frust->corners[3] = float3(1.0f / -x, 1.0f / +y, -clip_near);
		frust->corners[4] = float3(1.0f / -x, 1.0f / -y, -clip_far );
		frust->corners[5] = float3(1.0f / +x, 1.0f / -y, -clip_far );
		frust->corners[6] = float3(1.0f / +x, 1.0f / +y, -clip_far );
		frust->corners[7] = float3(1.0f / -x, 1.0f / +y, -clip_far );
	}
	if (frust_size) {
		*frust_size = float2(hsize, vsize);
	}
	if (clip_to_cam) {
		*clip_to_cam = float4x4(
			1.0f/x,      0,      0,       0,
			0, 1.0f/y,      0,       0,
			0,      0, 1.0f/a,    -b/a,
			0,      0,      0,       1
		);
	}
	return float4x4(
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, a, b,
		0, 0, 0, 1
	);
}

float wrap_azimuth (float azimuth) {
	return wrap(azimuth, deg(-180), deg(180));
}
float clamp_elevation (float elevation, float down_limit, float up_limit) {
	return clamp(elevation, deg(-90) + down_limit, deg(+90) - up_limit);
}
float wrap_roll (float roll) {
	return wrap(roll, deg(-180), deg(180));
}

void rotate_with_mouselook (Input& I, float* azimuth, float* elevation, float vfov) {
	auto raw_mouselook = I.get_mouselook_delta();

	float delta_x = raw_mouselook.x * vfov / I.mouselook_sensitiviy_divider;
	float delta_y = raw_mouselook.y * vfov / I.mouselook_sensitiviy_divider;

	*azimuth -= delta_x;
	*elevation += delta_y;

	*azimuth = wrap_azimuth(*azimuth);
	*elevation = clamp_elevation(*elevation, I.view_elevation_down_limit, I.view_elevation_up_limit);
}

float3x3 calc_ae_rotation (float2 ae, float3x3* out_inverse) {
	if (out_inverse)
		*out_inverse = rotate3_Z(+ae.x) * rotate3_X(+ae.y + deg(90));
	return             rotate3_X(-ae.y - deg(90)) * rotate3_Z(-ae.x);
}
float3x3 calc_aer_rotation (float3 aer, float3x3* out_inverse) {
	if (out_inverse)
		*out_inverse = rotate3_Z(+aer.x) * rotate3_X(+aer.y + deg(90)) * rotate3_Z(-aer.z);
	return             rotate3_Z(+aer.z) * rotate3_X(-aer.y - deg(90)) * rotate3_Z(-aer.x);
}

float3x3 Flycam::calc_world_to_cam_rot (float3x3* cam_to_world_rot) {
	return calc_aer_rotation(cam.rot_aer, cam_to_world_rot);
}

Camera_View Flycam::update (Input& I, int2 const& viewport_size) {

	//// look
	rotate_with_mouselook(I, &cam.rot_aer.x, &cam.rot_aer.y, cam.vfov);

	float3x3 cam_to_world_rot;
	float3x3 world_to_cam_rot = calc_aer_rotation(cam.rot_aer, &cam_to_world_rot);

	{ //// movement
		float3 move_dir = 0;
		if (I.buttons[KEY_A]           .is_down) move_dir.x -= 1;
		if (I.buttons[KEY_D]           .is_down) move_dir.x += 1;
		if (I.buttons[KEY_W]           .is_down) move_dir.z -= 1;
		if (I.buttons[KEY_S]           .is_down) move_dir.z += 1;
		if (I.buttons[KEY_LEFT_CONTROL].is_down) move_dir.y -= 1;
		if (I.buttons[KEY_SPACE]       .is_down) move_dir.y += 1;

		move_dir = normalizesafe(move_dir);
		float move_speed = length(move_dir); // could be analog with gamepad

		if (move_speed == 0.0f)
			cur_speed = base_speed; // no movement ticks down speed

		if (I.buttons[KEY_LEFT_SHIFT].is_down) {
			move_speed *= fast_multiplier;

			cur_speed += base_speed * speedup_factor * I.unscaled_dt;
		}

		cur_speed = clamp(cur_speed, base_speed, max_speed);

		float3 translation_cam_space = cur_speed * move_dir * I.unscaled_dt;

		cam.pos += cam_to_world_rot * translation_cam_space;
	}

	{ //// fov change
		if (!I.buttons[KEY_F].is_down) {
			float delta_log = 0.1f * I.mouse_wheel_delta;
			base_speed = powf(2, log2f(base_speed) +delta_log );
		} else {
			float delta_log = -0.1f * I.mouse_wheel_delta;
			cam.vfov = clamp(powf(2, log2f(cam.vfov) +delta_log ), deg(1.0f/10), deg(170));
		}
	}

	//// matrix calc
	Camera_View v;
	v.world_to_cam = world_to_cam_rot * translate(-cam.pos);
	v.cam_to_world = translate(cam.pos) * cam_to_world_rot;
	v.cam_to_clip = cam.calc_cam_to_clip(viewport_size, &v.clip_to_cam, &v.frustrum, &v.frustrum_size);
	v.clip_near = cam.clip_near;
	v.clip_far = cam.clip_far;
	v.calc_frustrum();
	return v;
}
