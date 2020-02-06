#pragma once
#include "kissmath.hpp"
#include "dear_imgui.hpp"
#include "util/collision.hpp"

#define DEFAULT_GRAVITY 20

static inline constexpr float COLLISION_EPSILON = 0.0001f; // floats have about 7 decimal digits of precision, so this only works up to about 10km in each direction, at some point the collision system just gets unreliable

struct CollisionResponse {
	float falling_ground_friction =		0.0f;
	float falling_bounciness =			0.25f;
	float falling_min_bounce_speed =	6;

	float wall_friction =				0.2f;
	float wall_bounciness =				0.55f;
	float wall_min_bounce_speed =		8;

	void imgui () {
		ImGui::DragFloat("falling_ground_friction", &falling_ground_friction, 0.05f);
		ImGui::DragFloat("falling_bounciness", &falling_bounciness, 0.05f);
		ImGui::DragFloat("falling_min_bounce_speed", &falling_min_bounce_speed, 0.05f);

		ImGui::DragFloat("wall_friction", &wall_friction, 0.05f);
		ImGui::DragFloat("wall_bounciness", &wall_bounciness, 0.05f);
		ImGui::DragFloat("wall_min_bounce_speed", &wall_min_bounce_speed, 0.05f);
	}
};

struct PhysicsObject {
	float3 pos;
	float3 vel;

	// always a cylinder for now
	float r;
	float h;

	CollisionResponse coll;
};

class Player;
class World;

class Physics {
public:

	float3 grav_accel = float3(0, 0, -DEFAULT_GRAVITY);

	void imgui () {
		if (!ImGui::CollapsingHeader("Physics")) return;

		ImGui::DragFloat3("grav_accel", &grav_accel.x, 0.2f);
	}

	static float jump_height_from_jump_impulse (float jump_impulse_up, float grav_mag) {
		return jump_impulse_up*jump_impulse_up / grav_mag * 0.5f;
	}
	static float jump_impulse_for_jump_height (float jump_height, float grav_mag) {
		return sqrt( 2.0f * jump_height * grav_mag );
	}

	void update_object (World& world, PhysicsObject& obj);

	void update_player (World& world, Player& player);
};

// Global physics
extern Physics physics;

