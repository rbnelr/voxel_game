#include "player.hpp"
#include "input.hpp"
#include "physics.hpp"

float3	player_spawn_point = float3(4,32,28.001f);

void Player::update_controls (bool player_on_ground) {
	//// toggle camera view
	if (input.buttons[GLFW_KEY_F].went_down)
		third_person = !third_person;

	Camera& cam = third_person ? tps_camera : fps_camera;

	//// look
	rotate_with_mouselook(&rot_ae.x, &rot_ae.y, cam.vfov);

	//// walking
	float2x2 body_rotation = rotate2(rot_ae.x);

	{
		float2 input_dir = 0;
		if (input.buttons[GLFW_KEY_A].is_down) input_dir.x -= 1;
		if (input.buttons[GLFW_KEY_D].is_down) input_dir.x += 1;
		if (input.buttons[GLFW_KEY_S].is_down) input_dir.y -= 1;
		if (input.buttons[GLFW_KEY_W].is_down) input_dir.y += 1;
		input_dir = normalizesafe(input_dir);

		bool input_fast = input.buttons[GLFW_KEY_LEFT_SHIFT].is_down;

		float target_speed = input_fast ? run_speed : walk_speed;
		float2 target_vel = body_rotation * (input_dir * target_speed);

		
		float2 delta_vel = target_vel - (float2)vel;
		float delta_speed = length(delta_vel);

		float accel = delta_speed * walk_accel_proport + walk_accel_base;

		delta_vel = normalizesafe(delta_vel) * min(accel * input.dt, delta_speed);
		vel += float3(delta_vel, 0);
	}

#if 0 // movement speed plotting to better develop movement code
	{
		static constexpr int COUNT = 128;
		static float vels[COUNT] = {};
		static float poss[COUNT] = {};
		static int cur = 0;

		if (!input.pause_time) {
			vels[cur] = length((float2)vel);
			poss[cur++] = pos.x;
			cur %= COUNT;
		}

		ImGui::SetNextItemWidth(-1);
		ImGui::PlotLines("###_debug_vel", vels, COUNT, cur, "player.vel", 0, 15, ImVec2(0, 100));

		ImGui::SetNextItemWidth(-1);
		ImGui::PlotLines("###_debug_pos", poss, COUNT, cur, "player.pos", -7, 7, ImVec2(0, 100));
	}
#endif
	
	//// jumping
	if (input.buttons[GLFW_KEY_SPACE].is_down && player_on_ground)
		vel += jump_impulse;
}

void Player::update_physics (bool player_on_ground) {

	//// gravity
	// if !on ground only?
	vel += physics.grav_accel * input.dt;

	// kill velocity if too small
	if (length(vel) < 0.01f)
		vel = 0;
}

Camera_View Player::update_post_physics () {
	float3x3 body_rotation = rotate3_Z(rot_ae.x);
	float3x3 body_rotation_inv = rotate3_Z(-rot_ae.x);

	float3x3 head_elevation = rotate3_X(rot_ae.y);
	float3x3 head_elevation_inv = rotate3_X(-rot_ae.y);

	//position_third_person_cam();

	Camera& cam = third_person ? tps_camera : fps_camera;
	
	Camera_View v;
	v.world_to_cam = translate(-cam.pos) * head_elevation_inv * translate(-head_pivot) * body_rotation_inv * translate(-pos);
	v.cam_to_world = translate(pos) * body_rotation * translate(head_pivot) * head_elevation * translate(cam.pos);
	v.cam_to_clip = cam.calc_cam_to_clip();

	// TODO: third person raycast to prevent camera clipping in blocks
	// TODO: block selection raycast and block break and place controls
	return v;
}
