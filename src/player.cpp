#include "common.hpp"
#include "chunks.hpp"
#include "game.hpp"
#include "player.hpp"

// TODO: verify this is working
// Look at https://stackoverflow.com/questions/2708476/rotation-interpolation
inline float alerp (float ang0, float ang1, float t) {
	float offset = wrap(ang1 - ang0, TAU);
	if (abs(offset) > PI)
		offset -= TAU;
	return wrap(ang0 + offset * t, TAU);
}

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
	
	cur_speed = length(vel);

	float walking_ang = atan2f(-vel.x, vel.y);
	float anim_fac = clamp(map(cur_speed, m.walk_speed, m.run_speed), 0.0f, 1.0f);
	float facing_ang = alerp(rot_ae.x, walking_ang, anim_fac * 0.5f);

	clog("%f %f", rot_ae.x, walking_ang);

	update_body_dynamics(I, facing_ang);

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

inline float rotate_towards_range (Input& I, float cur, float target,
		float speed_scaled, float speed_flat, float2 orig_range, float2 output_range) {
	if (cur == INF) return target; // Skip animation using INF as signal, useful for load etc.

	float t = clamp(map(target, orig_range.x, orig_range.y), 0.0f, 1.0f);
	target = lerp(output_range.x, output_range.y, t);

	float offset = target - cur;
	float delta = abs(offset) * speed_scaled + speed_flat;
	cur += copysignf(1.0f, offset) * min(delta * I.dt, abs(offset));
	return cur;
}
inline float rotate_towards_shorter_angle (Input& I, float cur, float target,
		float speed_scaled, float speed_flat) {
	if (cur == INF) return target; // Skip animation using INF as signal, useful for load etc.

	float offset = wrap(target - cur, deg(360));
	if (abs(offset) > deg(180))
		offset -= deg(360);

	float delta = abs(offset) * speed_scaled + speed_flat;
	cur += copysignf(1.0f, offset) * min(delta * I.dt, abs(offset));
	return wrap(cur, deg(360));
}

// Body rotation azimuth (x) is still wonky af, probably because of 360deg wraparound not working?
void Player::update_body_dynamics (Input& I, float facing_ang) {
	body_rot.x = rotate_towards_shorter_angle(I, body_rot.x, facing_ang, deg(1200), deg(70));
	body_rot.y = rotate_towards_range(I, body_rot.y, rot_ae.y, deg(800), deg(50),
		deg(float2(-90, +90)), deg(float2(-60, +70)));
}

void Player::update_view_dynamics (Input& I) {

}
