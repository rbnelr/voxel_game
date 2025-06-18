#include "common.hpp"
#include "chunks.hpp"
#include "game.hpp"
#include "player.hpp"

bool calc_ground_contact (Player& player, Chunks& chunks, bool* stuck) {
	{ // Check block intersection to see if we are somehow stuck inside a block
		int3 start =	(int3)floor(player.pos -float3(player.radius, player.radius, 0));
		int3 end =		(int3)ceil(player.pos +float3(player.radius, player.radius, player.height));

		bool any_intersecting = false;

		for (int z=start.z; z<end.z; ++z) {
			for (int y=start.y; y<end.y; ++y) {
				for (int x=start.x; x<end.x; ++x) {

					auto b = chunks.read_block(x,y,z);
					bool block_solid = g->assets->block_types[b].collision == CM_SOLID;

					bool intersecting = block_solid &&
						cylinder_cube_intersect(player.pos -(float3)int3(x,y,z), player.radius, player.height);

					//{
					//	lrgba col;
					//	if (!block_solid) col = srgba(40,40,40,100);
					//	else              col = intersecting ? srgba(255,40,40,200) : srgba(255,255,255,150);
					//	g_debugdraw.wire_cube((float3)int3(x,y,z) + 0.5f, 1, col);
					//}

					any_intersecting = any_intersecting || (intersecting && block_solid);
				}
			}
		}

		*stuck = any_intersecting; // player somehow ended up inside a block
	}

	bool grounded = false;
	{ // for all blocks we could be standing on

		int pos_z = floori(player.pos.z);

		//logf("pos.z fract: %10.8f  vel.z: %10.8f", (pos.z - pos_z), vel.z);
		if ((player.pos.z - pos_z) <= COLLISION_EPSILON * 1.5f && player.vel.z == 0) {

			int2 start =	(int2)floor((float2)player.pos - player.radius);
			int2 end =		(int2)ceil ((float2)player.pos + player.radius);

			int z = pos_z -1;

			for (int y=start.y; y<end.y; ++y) {
				for (int x=start.x; x<end.x; ++x) {

					auto b = chunks.read_block(x,y,z);

					bool block_solid = g->assets->block_types[b].collision == CM_SOLID;
					if (block_solid && circle_square_intersect((float2)player.pos -(float2)int2(x,y), player.radius))
						grounded = true; // cylinder base touches at least one soild block
				}
			}
		}
	}

	return grounded;
}

void Player::update_movement (Input& I, Player::PlayerInput& inp) {
	ZoneScoped;
	
	bool stuck;
	bool grounded = calc_ground_contact(*this, *g->chunks, &stuck);

	//// walking
	float2x2 body_rotation = rotate2(rot_ae.x);

	float3 prev_vel = vel;
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


	PhysicsObject obj;
	
	obj.pos = pos;
	obj.vel = vel;
	
	obj.r = radius;
	obj.h = height;
	
	obj.coll = collison_response;
	
	g->physics->update_object(I.dt, *g->chunks, obj);
	
	pos = obj.pos;
	vel = obj.vel;

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
