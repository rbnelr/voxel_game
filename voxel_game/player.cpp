#include "player.hpp"
#include "world.hpp"
#include "input.hpp"
#include "physics.hpp"
#include "util/collision.hpp"

float3	player_spawn_point = float3(0,0,34);

void Tool::update () {
	bool inp = input.buttons[GLFW_MOUSE_BUTTON_LEFT].is_down;

	if (anim_t > 0 || inp)
		anim_t += anim_freq * input.dt;

	if (anim_t >= 1)
		anim_t = 0;
}

void Player::update_movement_controls (bool player_on_ground) {
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

float3 Player::find_third_person_cam_pos (World& world, float3x3 body_rotation, float3x3 head_elevation) {
	Ray ray;
	ray.pos = pos + body_rotation * (head_pivot + tps_camera_base_pos);
	ray.dir = body_rotation * head_elevation * tps_camera_dir;

	float dist = tps_camera_dist;
	
	{
		BlockHit hit;
		if (world.raycast_solid_blocks(ray, dist, &hit))
			dist = max(hit.dist - 0.05f, 0.0f);
	}

	return tps_camera_base_pos + tps_camera_dir * dist;
}

Camera_View Player::update_post_physics (World& world) {
	float3x3 body_rotation = rotate3_Z(rot_ae.x);
	float3x3 body_rotation_inv = rotate3_Z(-rot_ae.x);

	float3x3 head_elevation = rotate3_X(rot_ae.y);
	float3x3 head_elevation_inv = rotate3_X(-rot_ae.y);

	float3 cam_pos = 0;
	if (third_person)
		cam_pos = find_third_person_cam_pos(world, body_rotation, head_elevation);

	Camera& cam = third_person ? tps_camera : fps_camera;

	float3x4 world_to_head = head_elevation_inv * translate(-head_pivot) * body_rotation_inv * translate(-pos);
	         head_to_world = translate(pos) * body_rotation * translate(head_pivot) * head_elevation;
	
	Camera_View v;
	v.world_to_cam = translate(-cam_pos) * rotate3_X(-deg(90)) * world_to_head;
	v.cam_to_world = head_to_world * rotate3_X(deg(90)) * translate(cam_pos);
	v.cam_to_clip = cam.calc_cam_to_clip();

	tool.update();

	// TODO: block selection raycast and block break and place controls
	return v;
}
