#include "common.hpp"
#include "chunks.hpp"
#include "game.hpp"
#include "player.hpp"

void Player::update_movement (Input& I, Player::PlayerInput& inp) {
	ZoneScoped;
	
	float3 prev_vel = vel;

	//// walking
	float2x2 body_rotation = rotate2(rot_ae.x);

	{
		auto& m = movement_params;

		float target_speed = inp.sprint ? m.run_speed : m.walk_speed;
		float2 target_vel = body_rotation * (inp.move_dir * target_speed);


		float2 delta_vel = target_vel - (float2)vel;
		float delta_speed = length(delta_vel);

		float accel = m.air_control_accel_base;
		if (grounded)
			accel = delta_speed * m.walk_accel_proport + m.walk_accel_base;

		delta_vel = normalizesafe(delta_vel) * min(accel * I.dt, delta_speed);
		vel += float3(delta_vel, 0);
	}

	//// jumping
	// TODO: player_on_ground is not reliable because of a hack in the collision system, so went_down does not work yet
	if (inp.jump_held/*went_down*/ && grounded)
		vel += jump_impulse;

	////
	PhysicsObject obj;
	
	obj.pos = pos;
	obj.vel = vel;
	obj.aabb0 = float3(-width*0.5f,-width*0.5f, 0);
	obj.aabb1 = float3(+width*0.5f,+width*0.5f, height());
	
	obj.coll = collison_response;

	g->physics->update_object(I, *g->chunks, obj);
	
	pos = obj.pos;
	vel = obj.vel;
	grounded = obj.grounded; // in theory still valid for next frame, at least if no voxel changes?

#if 1 // movement speed plotting to better develop movement code
	{
		float3 calc_accel3d = (vel - prev_vel) / I.dt;
		float calc_accel = length(calc_accel3d);

		static constexpr int COUNT = 128;
		static float vels[COUNT] = {};
		static float poss[COUNT] = {};
		static float accels[COUNT] = {};
		static int cur = 0;

		float cur_speed = length((float2)vel);

		if (!I.pause_time) {
			vels[cur] = cur_speed;
			poss[cur] = pos.x + 83.0f;
			accels[cur] = calc_accel;
			cur = (cur+1) % COUNT;
		}

		ImGui::Begin("Player Movement Dev");
		
		ImGui::Text("Speed: %6.2f m/s (%6.2f km/h)", cur_speed, cur_speed * 3.6f);
		ImGui::Text("Accel: %6.2f m/s^2  %6.2f g", calc_accel, calc_accel / 9.80665f);

		ImGui::SetNextItemWidth(-1);
		ImGui::PlotLines("###_debug_vel", vels, COUNT, cur, "player vel", 0, 15, ImVec2(0, 200));

		ImGui::SetNextItemWidth(-1);
		ImGui::PlotLines("###_debug_pos", poss, COUNT, cur, "player pos", -7, 7, ImVec2(0, 200));

		ImGui::SetNextItemWidth(-1);
		ImGui::PlotLines("###_debug_accel", accels, COUNT, cur, "player accel", 0, 20, ImVec2(0, 200));

		ImGui::End();
	}
#endif
}
