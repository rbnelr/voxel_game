#pragma once
#include "graphics/camera.hpp"
#include "chunks.hpp"
#include "physics.hpp"
#include "kissmath.hpp"
#include "kissmath.hpp"
#include <string>

// Global for now, the world should store this if it is not randomized
extern float3	player_spawn_point;

class World;

struct HighlightedBlock {
	Block*		block = nullptr;
	bpos		pos;
	BlockFace	face = (BlockFace)-1; // -1 == no face

	operator bool () {
		return block;
	}
};

class Tool {
public:

	float anim_freq = 3;
	float damage = 0.4f;

	float anim_t = 0;

	void imgui (const char* name=nullptr) {
		if (!imgui_push("Fists", name)) return;

		imgui_pop();
	}
	void update ();
};

class Player {
public:

	Player () {
		respawn();
	}

	// Player ground position
	float3	pos;

	// Player velocity
	float3	vel;

	// Player look rotation
	float2	rot_ae =		float2(deg(0), deg(-10)); // azimuth elevation

	////
	Tool tool;

	//// Cameras
	bool third_person = false;

	// Fps camera pivot for fps mode ie. where your eyes are
	//  First and third person cameras rotate around this
	float3 head_pivot = float3(0, 0, 1.6f);

	// Closest position the third person camera can go relative to head_pivot
	float3 tps_camera_base_pos = float3(0.5f, -0.15f, 0);
	// In which direction the camera moves back if no blocks are in the way
	float3 tps_camera_dir = float3(0,0,1);
	// How far the camera will move back
	float tps_camera_dist = 4;

	Camera fps_camera;
	Camera tps_camera;

	//// Physics
	float radius = 0.4f;
	float height = 1.7f;

	float walk_speed = 5.0f;
	float run_speed = 13.0f;

	float walk_accel_base = 5;
	float walk_accel_proport = 10;

	CollisionResponse collison_response;

	float3 jump_impulse = float3(0,0, physics.jump_impulse_for_jump_height(1.2f, DEFAULT_GRAVITY)); // jump height based on the default gravity, tweaked gravity will change the jump height

	void imgui (const char* name=nullptr) {
		if (!imgui_push("Player", name)) return;

		ImGui::DragFloat3("pos", &pos.x, 0.05f);

		ImGui::DragFloat3("vel", &vel.x, 0.03f);

		float2 rot_ae_deg = to_degrees(rot_ae);
		if (ImGui::DragFloat2("rot_ae", &rot_ae_deg.x, 0.05f))
			rot_ae = to_radians(rot_ae_deg);

		tool.imgui("tool");

		ImGui::Checkbox("third_person", &third_person);

		ImGui::DragFloat3("head_pivot", &head_pivot.x, 0.05f);
		ImGui::DragFloat3("tps_camera_base_pos", &tps_camera_base_pos.x, 0.05f);
		ImGui::DragFloat3("tps_camera_dir", &tps_camera_dir.x, 0.05f);
		ImGui::DragFloat("tps_camera_dist", &tps_camera_dist, 0.05f);

		fps_camera.imgui("fps_camera");
		tps_camera.imgui("tps_camera");

		ImGui::DragFloat("radius", &radius, 0.05f);
		ImGui::DragFloat("height", &height, 0.05f);

		ImGui::DragFloat("walk_speed", &walk_speed, 0.05f);
		ImGui::DragFloat("run_speed", &run_speed, 0.05f);
		ImGui::DragFloat("walk_accel_base", &walk_accel_base, 0.05f);
		ImGui::DragFloat("walk_accel_proport", &walk_accel_proport, 0.05f);

		collison_response.imgui();

		imgui_pop();
	}

	void update_movement_controls (bool player_on_ground);

	void update_physics (bool player_on_ground);

	float3x4 head_to_world;

	Camera_View update_post_physics (World& world);

	void respawn () {
		pos = player_spawn_point;
		vel = 0;
	}


	float3 find_third_person_cam_pos (World& world, float3x3 body_rotation, float3x3 head_elevation);
};
