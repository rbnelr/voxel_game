#pragma once
#include "common.hpp"
#include "chunks.hpp"
#include "game.hpp"

// TODO: rethink this?
struct CollisionResponse {
	//float falling_ground_friction =		0.0f;
	//float falling_bounciness =			0.25f;
	//float falling_min_bounce_speed =	6;
	//
	//float wall_friction =				0.2f;
	//float wall_bounciness =				0.55f;
	//float wall_min_bounce_speed =		8;

	void imgui () {
		//ImGui::DragFloat("falling_ground_friction", &falling_ground_friction, 0.05f);
		//ImGui::DragFloat("falling_bounciness", &falling_bounciness, 0.05f);
		//ImGui::DragFloat("falling_min_bounce_speed", &falling_min_bounce_speed, 0.05f);
		//
		//ImGui::DragFloat("wall_friction", &wall_friction, 0.05f);
		//ImGui::DragFloat("wall_bounciness", &wall_bounciness, 0.05f);
		//ImGui::DragFloat("wall_min_bounce_speed", &wall_min_bounce_speed, 0.05f);
	}
};

struct PhysicsObject {
	float3 pos;
	float3 vel;
	float3 aabb0, aabb1;

	CollisionResponse coll;

	bool grounded;

	void dbgdraw_aabb (float3 pos, lrgba col) {
		float3 sz = aabb1 - aabb0;
		float3 local_center = (aabb0 + aabb1) * 0.5f;
		g_debugdraw.wire_cube(pos + local_center, sz, col);
	}
};

struct World;

struct Physics {
	float3 grav_accel = float3(0, 0, -10);

	float min_speed = 0.001f;
	float max_speed = 1000;

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
	
#if 0
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
#endif

	struct VoxelCollisionHit {
		// Can currently be negative if still not exited voxel (set your own threshold to better handle slight float error)
		float t0 = INF; // Voxel entry time (pos + vel)

		// Collision normal
		float3 normal = 0;

		// Contact point (needed?)
		// Box position at t0 (pos + vel * t0)!

		bool buried = false;

		bool valid () { return t0 < INF; }
		operator bool () { return t0 < INF; }
	};

	// collision box AABB relative to voxel origin (aabb0, aabb1)
	// collision box vel already in voxel space
	// voxel AABB (vox0, vox1) relative to voxel origin
	// TODO: Can probably be significantly optimized if combined with voxel grid iteration
	bool voxel_box_cast (float3 box0, float3 box1, float3 box_vel,
	                     float3 vox0, float3 vox1, VoxelCollisionHit* hit) {
		*hit = {};

		// If object on left of voxel: how far to move in positive to start(l) and stop(h) colliding
		// If object on right, these are negative but vel sign cancels out
		float3 l = vox0 - box1;
		float3 h = vox1 - box0;

		bool parallelX = abs(box_vel.x) < min_speed;
		bool parallelY = abs(box_vel.y) < min_speed;
		bool parallelZ = abs(box_vel.z) < min_speed;
		
		bool overlapX = l.x < 0.0f && h.x > 0.0f;
		bool overlapY = l.y < 0.0f && h.y > 0.0f;
		bool overlapZ = l.z < 0.0f && h.z > 0.0f;

		float txl=-INF, txh=+INF;
		float tyl=-INF, tyh=+INF;
		float tzl=-INF, tzh=+INF;

		// Rays not parallel, so we can intersect ray with 'combined aabb' on this axis
		// Essentially compute ray timespan where this axis allows for collsion
		if (!parallelX) {
			float inv = 1.0f / box_vel.x; // TODO: Loop invariant for voxel iteration!
			txl = l.x * inv;
			txh = h.x * inv;
		}
		// If ray parallel collision condition never changes
		// so this axis either prevents collision entirely if outside, or will always allow collision
		else if (!overlapX) return false;
		
		if (!parallelY) {
			float inv = 1.0f / box_vel.y;
			tyl = l.y * inv;
			tyh = h.y * inv;
		}
		else if (!overlapY) return false;
		
		if (!parallelZ) {
			float inv = 1.0f / box_vel.z;
			tzl = l.z * inv;
			tzh = h.z * inv;
		}
		else if (!overlapZ) return false;

		// special case needed for zero velocity, whould could possibly be implemented differently
		if (parallelX && parallelY && parallelZ) {
			assert(overlapX && overlapY && overlapZ);
			// Stuck inside voxel (even if only slightly due to float error)
			hit->t0 = 0;
			hit->buried = true;
			return true;
		}

		// handle negative and positive movement on each axis by sorting entry/exit t
		float tx0 = min(txl, txh);
		float tx1 = max(txl, txh);
		float ty0 = min(tyl, tyh);
		float ty1 = max(tyl, tyh);
		float tz0 = min(tzl, tzh);
		float tz1 = max(tzl, tzh);

		// combine collision timespans to determine final timespan of collision
		float t0 = max(max(tx0, ty0), tz0);
		float t1 = min(min(tx1, ty1), tz1);

		// axis collision times do not overlap
		if (t1 <= t0) return false;
		// dit collide, but in the past
		if (t1 <= 0.0f) return false;

		// TODO: Stuck case with velocity, test should probably with absolute penetration depths
		if (t0 < -0.01f) {
			hit->t0 = 0;
			hit->buried = true;
			return true;
		}

		// Either will collide in future
		// Or if velocity zero: already colliding and will 'always' collide
		hit->t0 = t0;

		// Determine hit voxel face normal from hit time via comparison
		// TODO: If say, tx0/ty0 are close, could create diagonal normal via interpolation
		hit->normal = 0;
		if      (t0 == tx0) hit->normal.x = t0 == txl ? -1.0f : +1.0f;
		else if (t0 == ty0) hit->normal.y = t0 == tyl ? -1.0f : +1.0f;
		else if (t0 == tz0) hit->normal.z = t0 == tzl ? -1.0f : +1.0f;

		return true;
	}
	void handle_collison (PhysicsObject& obj, VoxelCollisionHit const& hit, bool* grounded) {
		// TODO: implement bouncing and friction again, probablty want to take average of bounciness and friction params of voxel and object
		// The problem is that earliest VoxelCollisionHit can be ambiguous, when standing on 4 flat voxels for example
		// -> if hit t0 different, simply take earliest
		// -> if hit t0 within epsilon, need to consider that both are valid
		// if normals are also equal we could possibly merge collision response factors by taking min/max friction
		// or possibly even lerp them based on estimated collision 'area', but this sounds complicated
		
		float norm_speed = dot(hit.normal, obj.vel); // normal points out of the wall
		float3 norm_vel = hit.normal * norm_speed;

		float3 sliding_vel = obj.vel - norm_vel;
		float sliding_speedsq = length_sqr(sliding_vel);
		
		// If moving into surface, bounce/deflect
		if (norm_speed < 0.0f) {
			float bounciness = 0.0f;
			float min_bounce_speed = 1.0f;
			norm_vel = bounciness * -norm_vel;
			if (length(norm_vel) <= min_bounce_speed) norm_vel = 0;
		}

		// If moving parallel with surface, apply sliding
		if (sliding_speedsq != 0.0f) {
			//float3 sliding_dir = sliding_vel / sqrt(sliding_speedsq);
			//
			// TODO: should friction actually depend on force/accel/speed into surface?
			// Bounces off of walls are 1 frame, so I'm not sure what's actually 'correct'
			// First and foremost friction would be important for things not bouncing off, but actually sliding
			// But it's exactly not something you really need with a character controller, as walking is explicitly not sliding
			//float friction_dv = sliding_dir * max(-norm_speed, 0.0f); // change in speed due to kinetic friction (unbounded ie. can be larger than our actual velocity)
			//frict_vel -= frict_dir * min(friction_dv, frict_speed);
		}

		// stop object at surface contact or (which is allowed to be slightly in the past)
		obj.pos = obj.pos + obj.vel * hit.t0;

		// if not bounced: essentially deflect object from surface
		obj.vel = sliding_vel + norm_vel;

		
		if (hit.normal.z > 0.0f) {
			// grounded if colliding with any top of block
			*grounded = true;
		}
	}

	VoxelCollisionHit world_voxel_box_collision (Chunks& chunks, PhysicsObject& obj) {

		//// Avoid zero speed to allow for easier voxel_box_cast
		//float speed = length(obj.vel);
		//float3 dir = speed >= min_speed ?
		//	obj.vel / speed :
		//	float3(0,0,-1);
		
		auto& block_types = g->assets->block_types;

		float3 aabb0 = obj.pos + obj.aabb0;
		float3 aabb1 = obj.pos + obj.aabb1;

		// TODO: take into account movement to compute all relevant voxels
		int3 start = (int3)floor(aabb0) -1;
		int3 end =   (int3)ceil( aabb1) +1;

		VoxelCollisionHit res_hit = {};

		for (int z=start.z; z<end.z; ++z)
		for (int y=start.y; y<end.y; ++y)
		for (int x=start.x; x<end.x; ++x) {
			auto bid = chunks.read_block(x,y,z);

			if (bid == B_NULL) break; // for debugging: colliding with unloaded chunk voxels is confusing for debugging, but in practice this might actually be desired

			if (block_types[bid].collision == CM_SOLID) {
				
				float3 vox_origin = (float3)int3(x,y,z);
				float3 local0 = aabb0 - vox_origin;
				float3 local1 = aabb1 - vox_origin;
				float3 vox0 = float3(0);
				float3 vox1 = float3(1);

				VoxelCollisionHit hit;
				if (!voxel_box_cast(local0, local1, obj.vel, vox0, vox1, &hit)) {
					continue;
				}

				g_debugdraw.wire_cube((float3)int3(x,y,z) + 0.5f, 0.98f, hit.buried ? srgba(255,255,0,200) : srgba(255,0,0,200));

				if (hit.t0 < res_hit.t0) {
					res_hit = hit;
				}
			}
			
			g_debugdraw.wire_cube((float3)int3(x,y,z) + 0.5f, 0.98f, srgba(40,40,40,100));
		}

		return res_hit;
	}


	void update_object (Input& I, Chunks& chunks, PhysicsObject& obj) {
		ZoneScoped;

		if (I.dt == 0) return;

		bool grounded = false;

		//// gravity
		obj.vel += grav_accel * I.dt;
		
		// clamp velocity
		// Ensure min_speed is low enough to let through gravity!!
		float speed_sq = length_sqr(obj.vel);
		if (speed_sq < min_speed*min_speed) obj.vel = 0;
		else if (speed_sq > max_speed*max_speed) obj.vel = max_speed * normalize(obj.vel);

		////
		float remain_dt = I.dt;

		if (I.buttons[KEY_N].went_down) {
			printf("");
		}

		for (int i=0; i<3; i++) {
			auto hit = world_voxel_box_collision(chunks, obj);
			
			obj.dbgdraw_aabb(obj.pos, lrgba(1,0,1,1));
			g_debugdraw.vector(obj.pos, obj.vel*0.1f, lrgba(0,0,1,1));
			if (hit) {
				float end_t = min(hit.t0, 1.0f);
				float3 collided_pos = obj.pos + obj.vel * end_t;
			
				obj.dbgdraw_aabb(collided_pos, lrgba(1,1,0,1));
			}

			if (hit && grounded && abs(obj.vel.z) < min_speed) {
				grounded = true;
			}

			if (hit && hit.buried) {
				//clog("collision - buried");

				obj.vel = 0;
			}
			else if (hit && hit.t0 <= remain_dt) {
				//clog("collision - handle_collison");
				handle_collison(obj, hit, &grounded);

				if (hit.t0 >= 0.0f) remain_dt -= hit.t0;

				if (remain_dt <= 0.0f) break;
				else continue; // repeat collision with deflected or bounced velocity and position using remainging timestep
			}
			else {
				//clog("collision - move without collision");
				obj.pos = obj.pos + obj.vel * remain_dt;
			}

			remain_dt = 0;
			break;
		}

		obj.grounded = grounded;

		// if remaining time, ignore it to prefer non-clipping to executing full movement speed
		// but could consider capping velocity to reflect missing movement step


		static int frame_counter = 0;
		clog("%5d: pos.z: %7.4f vel.z: %7.4f %s", frame_counter++, obj.pos.z, obj.vel.z,
			obj.grounded ? "(grounded)":"");
	}
};
