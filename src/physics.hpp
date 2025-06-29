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

struct GroundedInfo {

	bool grounded;
	int3 vox = INT_MIN;
	block_id bid = B_NULL;
	
	void trigger_step_sound (float volume, float set_pitch = -1) {
		if (!grounded) return;

		float pitch = set_pitch >= 0.0f ? set_pitch :
			map_clamp(volume, 0, 2, 0.7f, 1.2f);

		auto* sounds = g->assets->block_types[bid].step_sound;
		if (sounds)
			sounds->play_random_once(volume, pitch);
	}

	operator bool () { return grounded; }
};
struct PhysicsObject {
	float3 pos;
	float3 vel;
	AABB local_aabb;
	float drag_coeff;

	CollisionResponse coll;

	GroundedInfo grounded;
	bool buried;

	float submerged_ratio;
	float total_fluid_volume;
	float avg_fluid_mass;
	float avg_fluid_drag;
};

struct World;

struct Physics {
	float3 grav_accel = float3(0, 0, -16);

	float min_speed = 0.001f;
	float max_speed = 1000;

	// https://www.sciencefacts.net/terminal-velocity-of-a-human.html
	// 40 m/s (skydiving belly first) - 100 m/s (skydiving head down)
	float player_terminal_speed = 50;

	void imgui () {
		ImGui::DragFloat3("grav_accel", &grav_accel.x, 0.1f);
		ImGui::DragFloat("player_terminal_speed", &player_terminal_speed, 0.1f);
	}

	float jump_height_from_jump_impulse (float jump_impulse_up) {
		return jump_impulse_up*jump_impulse_up / -grav_accel.z * 0.5f;
	}
	float jump_impulse_for_jump_height (float jump_height) {
		return sqrt( 2.0f * jump_height * -grav_accel.z );
	}

	static constexpr float air_density = 1;

	// drag_coeff includes object airflow area and 'regular' drag coefficient
	// in the original formula drag_coeff really just specifies the shape and surface air drag
	// and higher mass causes higher accel, but 
	float fluid_drag_decel (float speed_sqr, float drag_coeff, float fluid_dens=air_density) {
		// https://en.wikipedia.org/wiki/Drag_equation
		// gas_density = 1; // could change this number inside different gas voxels?
		// ignore useless 0.5
		//float drag_force = 0.5f * speed*speed * gas_density * drag_coeff * drag_area;
		//float drag_deceleration = drag_force / mass; // ignore mass as we don't want to track mass
		float drag_deceleration = speed_sqr * fluid_dens * drag_coeff;
		return drag_deceleration;
	}
	float drag_coeff_for_terminal_vel (float terminal_speed) {
		// ignore mass and useless 0.5 factor
		return length(grav_accel) / (terminal_speed*terminal_speed);
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

	struct VoxelCollisionCastHit {
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
	                     float3 vox0, float3 vox1, VoxelCollisionCastHit* hit) {
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
	void handle_collison (PhysicsObject& obj, VoxelCollisionCastHit const& hit) {
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

		
		if (hit.normal.z > 0.0f && abs(obj.vel.z) < min_speed) {
			obj.grounded.grounded = true;
		}
	}

	VoxelCollisionCastHit world_voxel_box_cast (Chunks& chunks, PhysicsObject& obj, bool collision_debug) {

		auto& block_types = g->assets->block_types;

		float3 aabb0 = obj.pos + obj.local_aabb.lo;
		float3 aabb1 = obj.pos + obj.local_aabb.hi;

		// TODO: take into account movement to compute all relevant voxels
		int3 start = (int3)floor(aabb0) -1;
		int3 end =   (int3)ceil( aabb1) +1;

		VoxelCollisionCastHit res_hit = {};

		for (int z=start.z; z<end.z; ++z)
		for (int y=start.y; y<end.y; ++y)
		for (int x=start.x; x<end.x; ++x) {
			auto bid = chunks.read_block(x,y,z);
			auto& bt = block_types[bid];

			if (bt.collision == CM_GAS) continue;
			//if (bid == B_NULL) break; // for debugging: colliding with unloaded chunk voxels is confusing for debugging, but in practice this might actually be desired
			
			float3 vox_origin = (float3)int3(x,y,z);
			float3 box0 = aabb0 - vox_origin;
			float3 box1 = aabb1 - vox_origin;
			float3 vox0 = float3(0);
			float3 vox1 = float3(1);
			
			float3 rel = obj.pos - vox_origin;

			VoxelCollisionCastHit hit;
			if (!voxel_box_cast(box0, box1, obj.vel, vox0, vox1, &hit)) {
				continue;
			}

			if (bt.collision == CM_SOLID) {
				
				// Ugh, another reason why earliest hit response sucks
				// Later hits can be the one that are closer to player feet and thus where we might want to play sounds from
				bool standing_directly_on = rel.x >= vox0.x && rel.x < vox1.x &&
				                            rel.y >= vox0.y && rel.y < vox1.y;
				bool is_grounded = hit.normal.z > 0.0f;
				if (standing_directly_on && is_grounded)
					obj.grounded.vox = int3(x,y,z);

				if (collision_debug)
					g_debugdraw.wire_cube((float3)int3(x,y,z) + 0.5f, 0.98f, hit.buried ? srgba(255,255,0,200) : srgba(255,0,0,200));

				if (hit.t0 < res_hit.t0) {
					res_hit = hit;
				}
			}
			else if (bt.collision == CM_LIQUID || bt.collision == CM_BREAKABLE) {
				if (bt.collision == CM_LIQUID && chunks.read_block(x,y,z+1) != bid)
					vox1.z = 0.9f; // water has lower top TODO: make this better
				// Ignoring gas above water entirely right now

				// overlap AABB
				float3 l = max(vox0, box0);
				float3 h = min(vox1, box1);
				if (l.x < h.x && l.y < h.y && l.z < h.z) {
					float3 overlap = h - l;
					float overlap_volume = overlap.x * overlap.y * overlap.z;

					// Fluids are allow buoyancy
					if (bt.collision == CM_LIQUID) {
						obj.total_fluid_volume += overlap_volume;
						obj.avg_fluid_mass += bt.fluid_density * overlap_volume;
					}
					// Fluids and permeable blocks 
					obj.avg_fluid_drag += bt.volume_drag * overlap_volume;

					float3 p = vox_origin + (l+h)/2;
					g_debugdraw.wire_cube(p, overlap, srgba(255,100,40,100));
				}
			}
			
			//g_debugdraw.wire_cube((float3)int3(x,y,z) + 0.5f, 0.98f, srgba(40,40,40,100));
		}

		return res_hit;
	}
	
	bool world_voxel_box_overlap (Chunks& chunks, AABB aabb_world) {
		auto& block_types = g->assets->block_types;

		int3 start = (int3)floor(aabb_world.lo);
		int3 end =   (int3)ceil( aabb_world.hi);

		bool overlap = false;
		for (int z=start.z; z<end.z; ++z)
		for (int y=start.y; y<end.y; ++y)
		for (int x=start.x; x<end.x; ++x) {
			auto bid = chunks.read_block(x,y,z);
			//g_debugdraw.wire_cube((float3)int3(x,y,z) + 0.5f, 0.98f, srgba(40,40,40,100));

			if (block_types[bid].collision == CM_SOLID) {
				float3 vox_origin = (float3)int3(x,y,z);
				
				float3 vox0 = vox_origin + float3(0);
				float3 vox1 = vox_origin + float3(1);

				bool overlapX = vox0.x < aabb_world.hi.x && vox1.x > aabb_world.lo.x;
				bool overlapY = vox0.y < aabb_world.hi.y && vox1.y > aabb_world.lo.y;
				bool overlapZ = vox0.z < aabb_world.hi.z && vox1.z > aabb_world.lo.z;

				if (overlapX && overlapX && overlapZ) {
					overlap = true;
					break;
					//g_debugdraw.wire_cube((float3)int3(x,y,z) + 0.5f, 0.8f, srgba(255,0,0,100));
				}
			}
		}

		return overlap;
	}

	void update_object (Input& I, Chunks& chunks, PhysicsObject& obj, bool collision_debug) {
		ZoneScoped;

		if (I.dt == 0) return;

		//// gravity
		obj.vel += grav_accel * I.dt;
		
		// clamp velocity
		// Ensure min_speed is low enough to let through gravity!!
		float speed_sq = length_sqr(obj.vel);
		if (speed_sq < min_speed*min_speed) obj.vel = 0;
		else if (speed_sq > max_speed*max_speed) obj.vel = max_speed * normalize(obj.vel);
		
		obj.grounded = {};
		obj.buried = false;

		////
		float remain_dt = I.dt;

		// Need at least 3 iterations to handle jumping into corners correctly, which sucks
		// This is because the player controller applies forces to the velocity and moving even slightly into a wall requires one iteration
		// This would likely not be the case for particles, seemingly allowing us to get away with less iterations for them
		// but would suddenly break the moment more external forces are introduced (or gravity points in other directions)
		for (int i=0; i<3; i++) {
			obj.total_fluid_volume = 0;
			obj.avg_fluid_mass = 0;
			obj.avg_fluid_drag = 0;
	
			auto hit = world_voxel_box_cast(chunks, obj, collision_debug);
			
			//if (hit) {
			//	float end_t = min(hit.t0, 1.0f);
			//	float3 collided_pos = obj.pos + obj.vel * end_t;
			//
			//	obj.dbgdraw_aabb(collided_pos, lrgba(1,1,0,1));
			//}

			if (hit && hit.buried) {
				//log("collision - buried");
				obj.vel = 0;
				obj.buried = true;
			}
			else if (hit && hit.t0 <= remain_dt) {
				//log("collision - handle_collison");
				handle_collison(obj, hit);

				if (hit.t0 >= 0.0f) remain_dt -= hit.t0;

				if (remain_dt <= 0.0f) break;
				else continue; // repeat collision with deflected or bounced velocity and position using remainging timestep
			}
			else {
				//log("collision - move without collision");
				obj.pos = obj.pos + obj.vel * remain_dt;
			}

			remain_dt = 0;
			break;
		}

		
		float3 aabb_sz = obj.local_aabb.hi - obj.local_aabb.lo;
		float obj_volume = aabb_sz.x * aabb_sz.y * aabb_sz.z;
		
		obj.submerged_ratio = obj.total_fluid_volume / obj_volume;
		
		float obj_dens = 0.97f;
		float avg_fluid_dens = obj.avg_fluid_mass / obj_volume;
		float avg_fluid_drag = obj.avg_fluid_drag / obj_volume;

		// TODO: do this at beginning of frame with previous frame data? That requires storing this data though...
		// Currently this results in 'wrong' velocities
		{ // really simple but basically correct buoyancy
			// TODO: write down formula and dirivation logic
			// hint: fluid_dens is relative to player, which is essentially water
			// mass cancels out, ratio of densities matters (I think)
			obj.vel -= grav_accel * (avg_fluid_dens / obj_dens) * I.dt;
		}

		// TODO: this applies air drag even when in water and is inefficient

		{ // apply drag for liquids
			float speedsq = length_sqr(obj.vel);
			if (speedsq > min_speed*min_speed) {

				// fluid drag
				float decel = fluid_drag_decel(speedsq, obj.drag_coeff, avg_fluid_drag);
				// air drag (technically not correct as this applies even if fully submerged in fluid)
				decel +=      fluid_drag_decel(speedsq, obj.drag_coeff);

				obj.vel -= (obj.vel / sqrt(speedsq)) * (decel * I.dt);
			}
		}

		if (obj.grounded) {
			obj.grounded.bid = g->chunks->read_block(obj.grounded.vox);
		}

		// if remaining time, ignore it to prefer non-clipping to executing full movement speed
		// but could consider capping velocity to reflect missing movement step


		//static int frame_counter = 0;
		//log("%5d: pos.z: %7.4f vel.z: %7.4f %s", frame_counter++, obj.pos.z, obj.vel.z,
		//	obj.grounded ? "(grounded)":"");
	}
};

class ListenerMovementWindSounds {
public:
	Sound flying_main = {"flying1.wav", true};
	Sound flying_whoosh = {"flying_whoosh.wav", true};
	Sound flying_cloth_flutter = {"flying_cloth_flutter.wav", true};
	
	bool test_active = true;
	float test_speed = 0;

	float overall_volume = .5f;

	float2 main_range = float2(12, 45);
	float2 main_volumeR = float2(0, 3.0f);
	float2 main_pitchR = float2(0.25f, 2.0f);
	
	float2 whoosh_range = float2(15, 40);
	float2 whoosh_volumeR = float2(0, 0.7f);
	float2 whoosh_pitchR = float2(0.2f, 1.2f);

	float2 flutter_range = float2(11, 40);
	float2 flutter_volumeR = float2(0, 0.8f);
	float2 flutter_pitchR = float2(0.5f, 1.4f);

	void imgui () {
		if (ImGui::Begin("ListenerMovementWindSounds")) {
			ImGui::Checkbox("test_active", &test_active);
			ImGui::SliderFloat("test_speed", &test_speed, 0, 100);
			ImGui::SliderFloat("overall_volume", &overall_volume, 0, 3);

			ImGui::DragFloat2("main_range", &main_range.x, 0.1f);
			ImGui::DragFloat2("main_volume", &main_volumeR.x, 0.1f);
			ImGui::DragFloat2("main_pitch", &main_pitchR.x, 0.1f);

			ImGui::DragFloat2("whoosh_range", &whoosh_range.x, 0.1f);
			ImGui::DragFloat2("whoosh_volumeR", &whoosh_volumeR.x, 0.1f);
			ImGui::DragFloat2("whoosh_pitchR", &whoosh_pitchR.x, 0.1f);

			ImGui::DragFloat2("flutter_range", &flutter_range.x, 0.1f);
			ImGui::DragFloat2("flutter_volumeR", &flutter_volumeR.x, 0.1f);
			ImGui::DragFloat2("flutter_pitchR", &flutter_pitchR.x, 0.1f);
		}
		ImGui::End();
	}

	void update (float player_speed) {
		float speed = max(player_speed, test_speed);

		float main_t = map_clamp(speed, main_range.x, main_range.y);
		float main_vol = lerp(main_volumeR.x, main_volumeR.y, main_t);
		float main_pitch = lerp(main_pitchR.x, main_pitchR.y, main_t);
		flying_main.set_volume(overall_volume * main_vol);
		flying_main.set_pitch(main_pitch);
		
		float whoosh_t = map_clamp(speed, whoosh_range.x, whoosh_range.y);
		float whoosh_vol = lerp(whoosh_volumeR.x, whoosh_volumeR.y, whoosh_t);
		float whoosh_pitch = lerp(whoosh_pitchR.x, whoosh_pitchR.y, whoosh_t);
		flying_whoosh.set_volume(overall_volume * whoosh_vol);
		flying_whoosh.set_pitch(whoosh_pitch);
		
		float flutter_t = map_clamp(speed, flutter_range.x, flutter_range.y);
		float flutter_vol = lerp(flutter_volumeR.x, flutter_volumeR.y, flutter_t);
		float flutter_pitch = lerp(flutter_pitchR.x, flutter_pitchR.y, flutter_t);
		flying_cloth_flutter.set_volume(overall_volume * flutter_vol);
		flying_cloth_flutter.set_pitch(flutter_pitch);

		flying_main.set_playing(test_active);
		flying_whoosh.set_playing(test_active);
		flying_cloth_flutter.set_playing(test_active);
	}
};
