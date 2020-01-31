#pragma once
#include "graphics/camera.hpp"
#include "physics.hpp"
#include "kissmath.hpp"
#include "intersection_test.hpp"
#include <string>

// Global for now, the world should store this if it is not randomized
extern float3	player_spawn_point;

class Player {

public:
	const std::string name;

	// Player ground position
	float3	pos;

	// Player velocity
	float3	vel;

	// Player look rotation
	float2	rot_ae =		float2(deg(0), deg(+80)); // azimuth elevation

	//// Cameras
	bool third_person = false;

	// Fps camera pivot for fps mode ie. where your eyes are
	//  First and third person cameras rotate around this
	float3 head_pivot = float3(0, 0, 1.6f);

	Camera fps_camera = Camera("fps_camera", 0);
	Camera tps_camera = Camera("tps_camera", float3(0.5f, -0.4f, 4));

	//// Physics
	float walk_speed = 5.0f;
	float run_speed = 13.0f;

	float walk_accel_base = 5;
	float walk_accel_proport = 10;

	float collision_r =	0.4f;
	float collision_h =	1.7f;

	float walking_friction_alpha =		0.15f;

	float falling_ground_friction =		0.0f;
	float falling_bounciness =			0.25f;
	float falling_min_bounce_speed =	6;

	float wall_friction =				0.2f;
	float wall_bounciness =				0.55f;
	float wall_min_bounce_speed =		8;

	float3 jump_impulse = float3(0,0, physics.jump_impulse_for_jump_height(1.2f, DEFAULT_GRAVITY)); // jump height based on the default gravity, tweaked gravity will change the jump height

	Player (std::string name): name{name} {} 

	void imgui () {
		if (!imgui_push(name, "Player")) return;

		ImGui::DragFloat3("pos", &pos.x, 0.05f);

		ImGui::DragFloat3("vel", &vel.x, 0.03f);

		float2 rot_ae_deg = to_degrees(rot_ae);
		if (ImGui::DragFloat2("rot_ae", &rot_ae_deg.x, 0.05f))
			rot_ae = to_radians(rot_ae_deg);

		ImGui::Checkbox("third_person", &third_person);

		fps_camera.imgui();
		tps_camera.imgui();

		ImGui::DragFloat("walk_speed", &walk_speed, 0.05f);
		ImGui::DragFloat("run_speed", &run_speed, 0.05f);
		ImGui::DragFloat("walk_accel_base", &walk_accel_base, 0.05f);
		ImGui::DragFloat("walk_accel_proport", &walk_accel_proport, 0.05f);

		imgui_pop();
	}

	void update_controls (bool player_on_ground);

	void update_physics (bool player_on_ground);

	Camera_View update_post_physics ();

	void respawn () {
		pos = player_spawn_point;
		vel = 0;
	}

};
