#include "common.hpp"
#include "chunks.hpp"
#include "game.hpp"
#include "player.hpp"

void Player::update_movement (Input& I, Player::PlayerInput& inp) {
	ZoneScoped;
	
	//// walking
	float2x2 body_rotation = rotate2(rot_ae.x);
	float3 prev_vel = vel;
	float cur_speed = length(vel);

	auto& m = movement_params;
	
	auto player_walk_dynamics = [&] () {
		bool forward_move = inp.move_dir.y > 0.0f;
		bool do_sprint = inp.sprint && (forward_move || m.allow_backwards_sprint);
		float target_speed = do_sprint ? m.run_speed : m.walk_speed;

		float2 target_vel = body_rotation * (normalizesafe(inp.move_dir) * target_speed);

		float2 delta_vel = target_vel - (float2)vel;
		float delta_speed = length(delta_vel);

		float accel = 0;
		if (grounded) {
			// scale acceleration to higher when standing still, and lower when close to target speed
			float control_fac = 1.0f - smoothstep(clamp(cur_speed / (m.walk_speed*2), 0.0f, 1.0f));
			float linear_boost = m.walk_accel_boost * pow(control_fac, 3.0f);
			accel = min(pow(delta_speed, 1.5f) * m.walk_accel_scaled, m.walk_accel_scaled_max) + linear_boost;
		}

		accel = max(accel, m.air_control_accel_base);

		delta_vel = normalizesafe(delta_vel) * min(accel * I.dt, delta_speed);
		vel += float3(delta_vel, 0);
	};
	player_walk_dynamics();

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
		ImGui::PlotLines("###_debug_accel", accels, COUNT, cur, "player accel", 0, 70, ImVec2(0, 200));

		ImGui::End();
	}
#endif
}
