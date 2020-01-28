#include "player.hpp"
#include "input.hpp"
#include "physics.hpp"

float3	player_spawn_point = float3(4,32,43);

void Player::update_controls (bool player_on_ground) {
	//// toggle camera view
	if (input.buttons[GLFW_KEY_F].went_down)
		third_person = !third_person;

	Camera& cam = third_person ? tps_camera : fps_camera;

	//// look
	rotate_with_mouselook(&rot_ae.x, &rot_ae.y, cam.vfov);

	//// walking
	float2x2 body_rotation = rotate2(rot_ae.x);

	//if (player_stuck_in_solid_block) {
	//	vel_world = 0;
	//	printf(">>>>>>>>>>>>>> stuck!\n");
	//} else {
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

		static int movement_code = 4;
		ImGui::DragInt("movement_code", &movement_code);
	
		switch (movement_code) {
			case -1: {
				vel = float3(target_vel, vel.z);
			} break;
			case 0: { // old movement code

				float3 tmp = float3( lerp((float2)vel, target_vel, walking_friction_alpha), vel.z );

				if (player_on_ground) {
					vel = tmp;
				} else {
					if (length((float2)vel) < length(target_speed)*0.5f)
						vel = tmp; // only allow speeding up to slow speed with air control
				}
			} break;
			case 1: { // const accel based code
				
				static float accel = 20;
				static float accel_kickoff = 100;
				static float accel_kickoff_thres = -0.2f;

				ImGui::DragFloat("accel", &accel, 0.2f);
				ImGui::DragFloat("accel_kickoff", &accel_kickoff, 0.2f);
				ImGui::DragFloat("accel_kickoff_thres", &accel_kickoff_thres, 0.2f);
		
				float2 delta_vel = target_vel - (float2)vel;
				float delta_speed = length(delta_vel);

				float acc = accel;

				// Get a kick of initial accel if starting or stopping to walk or if direction changes by ~90 deg
				if (dot((float2)vel, target_vel) <= accel_kickoff_thres) {
					acc += accel_kickoff;
				}

				delta_vel = normalizesafe(delta_vel) * min(acc * input.dt, delta_speed); // don't overshoot

				vel += float3(delta_vel, 0);
			} break;
			case 2: {

				static float accel = 30;
				static float drag = 5;
				static bool drag_square = false;

				ImGui::DragFloat("accel", &accel, 0.2f);
				ImGui::DragFloat("drag", &drag, 0.2f);
				ImGui::Checkbox("drag_square", &drag_square);

				float2 delta_vel = target_vel - (float2)vel;
				float delta_speed = length(delta_vel);

				if (length(target_vel) > 0) { // let drag slow the player down
					delta_vel = normalizesafe(delta_vel) * min(accel * input.dt, delta_speed);
					vel += float3(delta_vel, 0);
				}

				if (player_on_ground) {
					float2 v = (float2)vel;
					float speed = length(v);
					float2 drag_force = -normalizesafe(v) * (drag_square ? speed * speed : speed) * drag;
					vel += float3(drag_force * input.dt, 0);
				}
			} break;
			case 4: {

				static float accel_base = 5;
				static float accel_proport = 10;

				ImGui::DragFloat("accel_base", &accel_base, 0.2f);
				ImGui::DragFloat("accel_proport", &accel_proport, 0.2f);

				float2 delta_vel = target_vel - (float2)vel;
				float delta_speed = length(delta_vel);

				float accel = delta_speed * accel_proport + accel_base;

				delta_vel = normalizesafe(delta_vel) * min(accel * input.dt, delta_speed);
				vel += float3(delta_vel, 0);

			} break;
		}
	}

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
	
	//// jumping
	if (input.buttons[GLFW_KEY_SPACE].is_down && player_on_ground)
		vel += jump_impulse;
}

void Player::update_physics (bool player_on_ground) {

	//// gravity
	// if on ground only?
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

	Camera& cam = third_person ? tps_camera : fps_camera;
	
	Camera_View v;
	v.world_to_cam = translate(-cam.pos) * head_elevation_inv * translate(-head_pivot) * body_rotation_inv * translate(-pos);
	v.cam_to_world = translate(pos) * body_rotation * translate(head_pivot) * head_elevation * translate(cam.pos);
	v.cam_to_clip = cam.calc_cam_to_clip();

	// TODO: third person raycast to prevent camera clipping in blocks
	// TODO: block selection raycast and block break and place controls
	return v;
}
