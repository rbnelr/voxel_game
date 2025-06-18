#pragma once
#include "common.hpp"
#include "chunks.hpp"
#include "game.hpp"

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

struct Player;
struct World;

struct Physics {
	float3 grav_accel = float3(0, 0, -20);

	void imgui () {
		if (!ImGui::CollapsingHeader("Physics")) return;

		ImGui::DragFloat3("grav_accel", &grav_accel.x, 0.2f);
	}

	float jump_height_from_jump_impulse (float jump_impulse_up) {
		return jump_impulse_up*jump_impulse_up / -grav_accel.z * 0.5f;
	}
	float jump_impulse_for_jump_height (float jump_height) {
		return sqrt( 2.0f * jump_height * -grav_accel.z );
	}
	
	static CollisionHit calc_earliest_collision (Chunks& chunks, PhysicsObject& obj) {
		ZoneScoped;

		CollisionHit closest_hit;
		closest_hit.dist = +INF;

		if (length_sqr(obj.vel) != 0) {
			// for all blocks we could be touching during movement by at most one block on each axis
			int3 start =	(int3)floor(obj.pos -float3(obj.r,obj.r,0)) -1;
			int3 end =		(int3)ceil(obj.pos +float3(obj.r,obj.r,obj.h)) +1;

			for (int z=start.z; z<end.z; ++z) {
				for (int y=start.y; y<end.y; ++y) {
					for (int x=start.x; x<end.x; ++x) {
						auto b = chunks.read_block(x,y,z);

						if (g->assets->block_types[b].collision == CM_SOLID) {

							float3 local_origin = (float3)int3(x,y,z);

							float3 pos_local = obj.pos - local_origin;
							float3 vel = obj.vel;

							CollisionHit hit;
							hit.dist = +INF;

							cylinder_cube_cast(pos_local, vel, obj.r, obj.h, &hit);

							if (hit.dist < closest_hit.dist) {
								closest_hit = hit;
								closest_hit.pos += local_origin; // convert to world coords
							}
						}
					}
				}
			}
		}

		return closest_hit;
	}

	static void handle_collison (PhysicsObject& obj, CollisionHit const& hit) {
		// handle block collision
		float friction;
		float bounciness;
		float min_bounce_speed;

		if (hit.normal.z == +1) {
			// hit top of block ie. ground
			friction = obj.coll.falling_ground_friction;
			bounciness = obj.coll.falling_bounciness;
			min_bounce_speed = obj.coll.falling_min_bounce_speed;
		} else {
			// hit side of block or bottom of block ie. wall or ceiling
			friction = obj.coll.wall_friction;
			bounciness = obj.coll.wall_bounciness;
			min_bounce_speed = obj.coll.wall_min_bounce_speed;
		}

		float3 normal = hit.normal;
		float norm_speed = dot(normal, obj.vel); // normal points out of the wall
		float3 norm_vel = normal * norm_speed;

		float3 frict_vel = obj.vel -norm_vel;
		frict_vel.z = 0; // do not apply friction on vertical movement
		float frict_speed = length(frict_vel);

		float3 remain_vel = obj.vel -norm_vel -frict_vel;

		if (frict_speed != 0) {
			float3 frict_dir = frict_vel / frict_speed;

			float friction_dv = friction * max(-norm_speed, 0.0f); // change in speed due to kinetic friction (unbounded ie. can be larger than our actual velocity)
			frict_vel -= frict_dir * min(friction_dv, frict_speed);
		}

		norm_vel = bounciness * -norm_vel;

		if (length(norm_vel) <= min_bounce_speed) norm_vel = 0;

		obj.vel = float3(norm_vel +frict_vel +remain_vel);

		obj.pos = hit.pos;

		// Floating point calculations in the raycasting code seem to produce small errors than can leave us a tiny bit inside the wall at which point the next raycast (not sure why it would do that) does not see the wall anymore
		obj.pos += float3(hit.normal * COLLISION_EPSILON);
	}

	void update_object (float dt, Chunks& chunks, PhysicsObject& obj) {
		ZoneScoped;

		//// gravity
		obj.vel += grav_accel * dt;

		////

		float t_remain = dt;

		while (t_remain > 0) {

			CollisionHit earliest_collision = calc_earliest_collision(chunks, obj);

			// if we are moving so fast that we would move by more than one block on any one axis we will do sub steps of exactly one block
			float max_dt = min(t_remain, 1.0f / max_component(abs(obj.vel)));

			// inf if there is no collision
			float earliest_collision_t = earliest_collision.dist / length(obj.vel);

			//logf("%5d: pos.z: %7.4f vel.z: %7.4f coll.dist: %7.4f coll.pos.z: %7.4f", frame_counter, obj.pos.z, obj.vel.z, earliest_collision.dist, earliest_collision.pos.z);

			if (earliest_collision_t >= max_dt) {
				obj.pos += obj.vel * max_dt;
				t_remain -= max_dt;
			} else {

				handle_collison(obj, earliest_collision);

				t_remain -= earliest_collision_t;
			}
		}

		//// kill velocity if too small
		if (length(obj.vel) < 0.01f)
			obj.vel = 0;

		//logf("%5d: pos.z: %7.4f vel.z: %7.4f", frame_counter, obj.pos.z, obj.vel.z);
	}
};
