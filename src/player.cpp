#include "common.hpp"
#include "chunks.hpp"
#include "game.hpp"
#include "player.hpp"

void Player::update_movement (Input& I, Player::PlayerInput& inp) {
	ZoneScoped;
	
	//// walking
	float2x2 body_rotation2d = rotate2(rot_ae.x);
	float3 prev_vel = vel;
	float cur_speed = length(vel);

	auto& m = movement_params;

	auto player_crouching = [&] () {
		// TODO: support crouch toggle?
		
		//// Crouch logic
		bool want_crouch = inp.crouch_button.is_down;
		bool was_crouch = is_crouched();
		bool do_crouch = was_crouch;

		// if buried crouching state will remain
		if ((want_crouch != was_crouch) && !buried) {
			if (want_crouch) {
				// able to crouch if grounded
				if (grounded) {
					do_crouch = true;
				}
			}
			else {
				// able to uncrouch if collision box can actually be grown again
				auto standing_aabb = collision_local_aabb(0) + pos;
				if (!g->physics->world_voxel_box_overlap(*g->chunks, standing_aabb)) {
					do_crouch = false;
				}
			}
		}


		// Linearly animate head position during crouching transition if standing on ground
		// Crouching progress will eventually switch is_crouched() and thus collision box
		float crouch_target = do_crouch ? 1.0f : 0.0f;
		float prev_crouch = crouching_progress;
		crouching_progress = move_linear(crouching_progress, crouch_target,
			m.crouch_transition_speed * I.dt);
		
		{ // Compute head bob from head lower/raise toggle impulse
			// track delta crouch_change (float) instead of float3 head pivot to save on memory I guess?
			float3 crouch_dir = body_rotation() * normalizesafe(head_pivot_crouching - head_pivot_standing);
			float cur_crouch_change = normalizesafe(crouching_progress - prev_crouch);
			// only use direction, ignore real velocity to keep impulse constant
			float3 impulse = crouch_dir * (cur_crouch_change - crouching_change);
		
			apply_head_bob_impulse(impulse);
		
			crouching_change = cur_crouch_change;
		}
	};

	auto player_walk_dynamics = [&] () {
		bool forward_move = inp.move_dir.y > 0.0f;
		bool sprint_allowed = !is_crouched() && (forward_move || m.allow_backwards_sprint);
		bool do_sprint = sprint_allowed && inp.sprint;

		float walk_or_crouch_speed = lerp(m.walk_speed, m.crouch_speed, crouching_progress);
		float target_speed = do_sprint ? m.run_speed : walk_or_crouch_speed;

		float2 target_vel = body_rotation2d * (normalizesafe(inp.move_dir) * target_speed);

		float2 delta_vel = target_vel - (float2)vel;
		float delta_speed = length(delta_vel);
		
		float move_accel = 0;
		if (grounded) {
			// scale acceleration to higher when standing still, and lower when close to target speed
			float control_fac = 1.0f - smoothstep(clamp(cur_speed / (m.walk_speed*2), 0.0f, 1.0f));
			float linear_boost = m.walk_accel_boost * pow(control_fac, 3.0f);
			move_accel = min(pow(delta_speed, 1.5f) * m.walk_accel_scaled, m.walk_accel_scaled_max) + linear_boost;
		}

		move_accel = max(move_accel, m.air_control_accel_base);

		delta_vel = normalizesafe(delta_vel) * min(move_accel * I.dt, delta_speed);
		vel += float3(delta_vel, 0);
	};

	auto do_jump = [&] () {
		float3 jump_impulse = float3(0,0, g->physics->jump_impulse_for_jump_height(1.15f)); // jump height based on the default gravity, tweaked gravity will change the jump height

		vel += jump_impulse;

		// Head bob on jump
		apply_head_bob_impulse(jump_impulse);

		grounded.trigger_step_sound(1.4f, 1); // Jump sound
	};
	auto on_landing = [&] (PhysicsObject& obj, float3 falling_vel, float3 val_after_landing) {
		float3 delta_vel = val_after_landing - falling_vel;
		float impact_impulse = length(delta_vel);
		float audio_stren = map_clamp(impact_impulse, 2.0f, 20.0f, 0.7f, 6.0f);

		//log("Landed impact_impulse: %f", impact_impulse);

		// Landing sound, two makes it stronger and actually sounds like two feet landing
		obj.grounded.trigger_step_sound(audio_stren, 0.95f);
		obj.grounded.trigger_step_sound(audio_stren, 0.95f);
	};
	
	player_crouching();
	player_walk_dynamics();

	_dbg_apply_forw_impulse(I);

	//// jumping
	if (inp.jump_held/*went_down*/ && grounded && !buried && !is_crouched()) {
		do_jump();
	}

	////
	PhysicsObject obj;
	
	obj.pos = pos;
	obj.vel = vel;
	obj.local_aabb = collision_local_aabb(crouching_progress);
	obj.drag_coeff = g->physics->drag_coeff_for_terminal_vel(g->physics->player_terminal_speed);

	obj.coll = collison_response;

	g->physics->update_object(I, *g->chunks, obj, collision_debug);
	
	if (collision_debug || g->activate_flycam || third_person) {
		obj.local_aabb.dbgdraw(obj.pos, lrgba(1,0,1,1));
	}
	if (collision_debug) {
		g_debugdraw.vector(obj.pos, obj.vel*0.1f, lrgba(0,0,1,1));
	}

	// Head bob on collision, ie landing, hitting a wall etc.
	float3 collision_impulse = obj.vel - vel;
	apply_head_bob_impulse(collision_impulse);
	
	bool landed = !grounded && obj.grounded;
	if (landed) {
		on_landing(obj, vel, obj.vel);
	}

	pos = obj.pos;
	vel = obj.vel;
	grounded = obj.grounded; // in theory still valid for next frame, at least if no voxel changes?
	buried = obj.buried;

	cur_speed = length(vel);
	float cur_speed2d = length((float2)vel);

	float walking_ang = atan2f(-vel.x, vel.y);
	float anim_fac = clamp(map(cur_speed2d, m.walk_speed, m.run_speed), 0.0f, 1.0f);
	float facing_ang = lerp_angle(rot_ae.x, walking_ang, anim_fac * 0.5f);

	update_walking_step_bob(I, cur_speed2d, obj);

	update_body_dynamics(I, facing_ang);

	//apply_head_bob_impulse(vel - prev_vel); // full accel as impulse for luls
	update_view_dynamics(I);
	
	move_wind_sounds.update(cur_speed);

#if 1 // movement speed plotting to better develop movement code
	{
		static constexpr int COUNT = 128;
		static float vels[COUNT] = {};
		static float poss[COUNT] = {};
		static float accels[COUNT] = {};
		static int cur = 0;
		
		float3 calc_accel3d = (vel - prev_vel) / (I.dt + 0.0001f);
		float calc_accel = length(calc_accel3d);

		//float cur_speed = length((float2)vel);
		float cur_speed = length(vel);

		if (!I.pause_time) {
			vels[cur] = cur_speed;
			poss[cur] = pos.x + 83.0f;
			accels[cur] = calc_accel;
			cur = (cur+1) % COUNT;
		}

		if (ImGui::Begin("Player Movement Dev")) {
			ImGui::Text("Speed: %6.2f m/s (%6.2f km/h)", cur_speed, cur_speed * 3.6f);
			ImGui::Text("Accel: %6.2f m/s^2  %6.2f g", calc_accel, calc_accel / 9.80665f);

			ImGui::SetNextItemWidth(-1);
			ImGui::PlotLines("###_debug_vel", vels, COUNT, cur, "player vel", 0, 15, ImVec2(0, 200));

			ImGui::SetNextItemWidth(-1);
			ImGui::PlotLines("###_debug_pos", poss, COUNT, cur, "player pos", -7, 7, ImVec2(0, 200));

			ImGui::SetNextItemWidth(-1);
			ImGui::PlotLines("###_debug_accel", accels, COUNT, cur, "player accel", 0, 70, ImVec2(0, 200));
		}
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

void Player::update_walking_step_bob (Input& I, float cur_speed2d, PhysicsObject& phys) {
	if (!grounded) {
		// need to take full step again after landing, and no steps in air!
		walking_step_bob_counter = 0;
		return;
	}
	
	bool is_sprinting = cur_speed2d > movement_params.walk_speed;
	float step_len;
	if (is_crouched())     step_len = visual_dynamics.step_length_crouch;
	else if (is_sprinting) step_len = visual_dynamics.step_length_sprint;
	else                   step_len = visual_dynamics.step_length;

	walking_step_bob_counter += cur_speed2d * I.dt;
	if (walking_step_bob_counter > step_len) {
		walking_step_bob_counter = wrap(walking_step_bob_counter, step_len);

		float effect_scale;
		if (is_crouched()) effect_scale = 0.3f;
		else effect_scale = clamp(cur_speed2d / movement_params.walk_speed, 0.5f, 1.8f);

		float bob_impulse = visual_dynamics.step_head_bob_strength * effect_scale;
		
		// Kinda hacky to do it this way (could ignore head bob offset for third person)
		// But keeping head bob for impacts in third person would be interesting
		if (!third_person)
			apply_head_bob_impulse(float3(0,0,-bob_impulse));

		phys.grounded.trigger_step_sound(effect_scale);
	}
}

void Player::apply_head_bob_impulse (float3 delta_vel) {
	head_bob_vel -= visual_dynamics.bob_strength * delta_vel;
}
void Player::update_view_dynamics (Input& I) {
	auto& vd = visual_dynamics;
	
	// Spring constant
	float3 offs_t = head_bob_offset / vd.offset_max;
	float3 spring_accel = offs_t * vd.spring_k * -3;
	// Spring dampening
	spring_accel -= head_bob_vel / vd.offset_max * vd.spring_damp;
	// Apply spring accel
	head_bob_vel += spring_accel * I.dt;
	head_bob_vel = clamp(head_bob_vel, -vd.vel_max, +vd.vel_max);

	head_bob_offset += head_bob_vel * I.dt;
	head_bob_offset = clamp(head_bob_offset, -vd.offset_max, +vd.offset_max);

	ImGui::Text("head_bob_vel: %6.3f %6.3f %6.3f", head_bob_vel.x, head_bob_vel.y, head_bob_vel.z);
	ImGui::Text("head_bob_offset: %6.3f %6.3f %6.3f", head_bob_offset.x, head_bob_offset.y, head_bob_offset.z);

	if (g->activate_flycam || third_person)
		g_debugdraw.wire_cube(pos + body_rotation() * head_pivot() + head_bob_offset, float3(0.3f, 0.3f, 0.4f),  lrgba(1,0,1,1));
}
