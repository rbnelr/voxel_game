#pragma once
#include "kissmath.hpp"
#include "dear_imgui.hpp"

#define DEFAULT_GRAVITY 20

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
};

// Global physics
extern Physics physics;

