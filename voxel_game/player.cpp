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
		float2 move_dir = 0;
		if (input.buttons[GLFW_KEY_A].is_down) move_dir.x -= 1;
		if (input.buttons[GLFW_KEY_D].is_down) move_dir.x += 1;
		if (input.buttons[GLFW_KEY_S].is_down) move_dir.y -= 1;
		if (input.buttons[GLFW_KEY_W].is_down) move_dir.y += 1;
		move_dir = normalizesafe(move_dir);

		bool walk_fast = input.buttons[GLFW_KEY_LEFT_SHIFT].is_down;

		float2 player_walk_speed = walk_fast ? run_speed : walk_speed;
		
		float2 feet_vel_world = body_rotation * (move_dir * player_walk_speed);

		float3 tmp = float3( lerp((float2)vel, feet_vel_world, walking_friction_alpha), vel.z );

		if (player_on_ground) {
			vel = tmp;
		} else {
			if (length((float2)vel) < length(player_walk_speed)*0.5f)
				vel = tmp; // only allow speeding up to slow speed with air control
		}
		
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
