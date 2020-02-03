
#define _CRT_SECURE_NO_WARNINGS 1

#include <cstdio>
#include <array>
#include <vector>
#include <string>


#include "util/collision.hpp"
#include "open_simplex_noise/open_simplex_noise.hpp"

#include "game.hpp"
#include "graphics/debug_graphics.hpp"
#include "input.hpp"
#include "glfw_window.hpp"

#include "kissmath.hpp"
#include "kissmath_colors.hpp"
#include "util/string.hpp"
#include "util/timer.hpp"
using namespace kiss;

#include "glad/glad.h"

#include "stb_image.hpp"

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

#define PROFILE_BEGIN(name)	auto __profile_##name = Timer::start()
#define PROFILE_END_PRINT(name, format, ...)	printf(">> PROFILE: %s took %8.3f ms  " format "\n", STRINGIFY(name), (__profile_##name).end() * 1000, __VA_ARGS__)

#define PROFILE_END_ACCUM(name)	name += (__profile_##name).end()
#define PROFILE_PRINT(name, format, ...)	printf(">> PROFILE: %s took %8.3f ms  " format "\n", STRINGIFY(name), name * 1000, __VA_ARGS__)

//
static bool controling_flycam =		1;
static bool viewing_flycam =		1;

static bool trigger_dbg_heightmap_visualize =	false;

bool FileExists (const char* path) {
	DWORD dwAttrib = GetFileAttributes(path);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}
bool _need_potatomode () {
	return FileExists("../../._need_potatomode.txt");
}
bool _use_potatomode = _need_potatomode();

//
Game::Game () {
	{ // GL state
		glEnable(GL_FRAMEBUFFER_SRGB);

		glEnable(GL_DEPTH_TEST);
		glClearDepth(1.0f);
		glDepthFunc(GL_LEQUAL);
		glDepthRange(0.0f, 1.0f);
		glDepthMask(GL_TRUE);

		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);

		glDisable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	}
}

void Game::frame () {

	{
		bool fullscreen = get_fullscreen();
		if (ImGui::Checkbox("fullscreen", &fullscreen)) {
			toggle_fullscreen();
		}

		ImGui::SameLine();
		bool vsync = get_vsync();
		if (ImGui::Checkbox("vsync", &vsync)) {
			set_vsync(vsync);
		}

		ImGui::SameLine();
		ImGui::Checkbox("ImGui Demo", &imgui.show_demo_window);

		ImGui::SameLine();
		if (ImGui::Button("exit")) {
			glfwSetWindowShouldClose(window, 1);
		}

		fps_display.display_fps();
	}

	input.imgui();

	if (ImGui::CollapsingHeader("Graphics")) {
		common_uniforms.imgui();

		chunk_graphics.imgui(world->chunks);

		ImGui::Separator();
	}

	{
		bool open = ImGui::CollapsingHeader("World", ImGuiTreeNodeFlags_DefaultOpen);
		
		if (open) {
			static std::string world_seed = world->seed_str;
			ImGui::InputText("seed", &world_seed, 0, NULL, NULL);

			ImGui::SameLine();
			if (ImGui::Button("Recreate")) {
				world = std::make_unique<World>(world_seed);
			}
		}

		world->imgui(open);
	}

	{
		bool open = ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen);
		
		if (open) ImGui::Checkbox("Toggle Flycam [P]", &activate_flycam);
		if (input.buttons[GLFW_KEY_P].went_down)
			activate_flycam = !activate_flycam;

		if (open) ImGui::DragFloat3("player_spawn_point", &player_spawn_point.x, 0.2f);
		if ((open && ImGui::Button("Respawn Player [Q]")) || input.buttons[GLFW_KEY_Q].went_down) {
			world->player.respawn();
		}

		if (open) ImGui::Separator();

		if (open) flycam.imgui();
		if (open) world->player.imgui();

		if (open) ImGui::Separator();
	}

	world->update(world_gen);

	{ // player position (collision and movement dynamics)
		constexpr float COLLISION_SEPERATION_EPSILON = 0.001f;

		// 
		bool player_stuck_in_solid_block;
		bool player_on_ground;

		auto check_blocks_around_player = [&] () {
			{ // for all blocks we could be touching
				bpos start =	(bpos)floor(world->player.pos -float3(world->player.radius, world->player.radius, 0));
				bpos end =		(bpos)ceil(world->player.pos +float3(world->player.radius, world->player.radius, world->player.height));

				bool any_intersecting = false;

				bpos bp;
				for (bp.z=start.z; bp.z<end.z; ++bp.z) {
					for (bp.y=start.y; bp.y<end.y; ++bp.y) {
						for (bp.x=start.x; bp.x<end.x; ++bp.x) {

							auto* b = world->chunks.query_block(bp);
							bool block_solid = block_props[b->type].collision == CM_SOLID;
							
							bool intersecting = block_solid && cylinder_cube_intersect(world->player.pos -(float3)bp, world->player.radius, world->player.height);

							if (0) {
								lrgba col;

								if (!block_solid) {
									col = srgb(40,40,40,100);
								} else {
									col = intersecting ? srgb(255,40,40,200) : srgb(255,255,255,150);
								}

								debug_graphics->push_wire_cube(0.5f, 1, col);
							}

							any_intersecting = any_intersecting || (intersecting && block_solid);
						}
					}
				}

				player_stuck_in_solid_block = any_intersecting; // player somehow ended up inside a block
			}

			{ // for all blocks we could be standing on

				bpos_t pos_z = floori(world->player.pos.z);

				player_on_ground = false;

				if ((world->player.pos.z -pos_z) <= COLLISION_SEPERATION_EPSILON*1.05f && world->player.vel.z == 0) {

					bpos2 start =	(bpos2)floor((float2)world->player.pos -world->player.radius);
					bpos2 end =		(bpos2)ceil((float2)world->player.pos +world->player.radius);

					bpos bp;
					bp.z = pos_z -1;

					for (bp.y=start.y; bp.y<end.y; ++bp.y) {
						for (bp.x=start.x; bp.x<end.x; ++bp.x) {

							auto* b = world->chunks.query_block(bp);
							bool block_solid = block_props[b->type].collision == CM_SOLID;

							if (block_solid && circle_square_intersect((float2)world->player.pos -(float2)(bpos2)bp, world->player.radius)) player_on_ground = true;
						}
					}
				}
			}

		};
		check_blocks_around_player();

		if (!activate_flycam) {
			world->player.update_controls(player_on_ground);
		}
		world->player.update_physics(player_on_ground);

		//option("draw_debug_overlay", &draw_debug_overlay);

		float3 pos_world = world->player.pos;
		float3 vel_world = world->player.vel;

		auto trace_player_collision_path = [&] () {
			float player_r = world->player.radius;
			float player_h = world->player.height;

			float t_remain = input.dt;

			bool draw_dbg = false;//draw_debug_overlay; // so that i only draw the debug block overlay once

			while (t_remain > 0) {

				struct {
					float dist = +INF;
					float3 hit_pos;
					float3 normal; // normal of surface/edge we collided with, the player always collides with the outside of the block since we assume the player can never be inside a block if we're doing this raycast
				} earliest_collision;

				auto find_earliest_collision_with_block_by_raycast_minkowski_sum = [&] (bpos bp) {
					bool hit = false;

					auto* b = world->chunks.query_block(bp);
					bool block_solid = block_props[b->type].collision == CM_SOLID;

					if (block_solid) {

						float3 local_origin = (float3)bp;

						float3 pos_local = pos_world -local_origin;
						float3 vel = vel_world;

						auto collision_found = [&] (float hit_dist, float3 hit_pos_local, float3 normal_world) {
							if (hit_dist < earliest_collision.dist) {
								float3 hit_pos_world = hit_pos_local +local_origin;

								earliest_collision.dist =		hit_dist;
								earliest_collision.hit_pos =	hit_pos_world;
								earliest_collision.normal =		normal_world;
							}
						};

						// this geometry we are raycasting our player position onto represents the minowski sum of the block and our players cylinder

						auto raycast_x_side = [&] (float3 ray_pos, float3 ray_dir, float plane_x, float normal_x) { // side forming yz plane
							if (ray_dir.x == 0 || (ray_dir.x * (plane_x -ray_pos.x)) < 0) return false; // ray parallel to plane or ray points away from plane

							float delta_x = plane_x -ray_pos.x;
							float2 delta_yz = delta_x * (float2(ray_dir.y,ray_dir.z) / ray_dir.x);

							float2 hit_pos_yz = float2(ray_pos.y,ray_pos.z) + delta_yz;

							float hit_dist = length(float3(delta_x, delta_yz[0], delta_yz[1]));
							if (!all(hit_pos_yz > float2(0,-player_h) && hit_pos_yz < 1)) return false;

							collision_found(hit_dist, float3(plane_x, hit_pos_yz[0], hit_pos_yz[1]), float3(normal_x,0,0));
							return true;
						};
						auto raycast_y_side = [&] (float3 ray_pos, float3 ray_dir, float plane_y, float normal_y) { // side forming xz plane
							if (ray_dir.y == 0 || (ray_dir.y * (plane_y -ray_pos.y)) < 0) return false; // ray parallel to plane or ray points away from plane

							float delta_y = plane_y -ray_pos.y;
							float2 delta_xz = delta_y * (float2(ray_dir.x,ray_dir.z) / ray_dir.y);

							float2 hit_pos_xz = float2(ray_pos.x,ray_pos.z) +delta_xz;

							float hit_dist = length(float3(delta_xz[0], delta_y, delta_xz[1]));
							if (!all(hit_pos_xz > float2(0,-player_h) && hit_pos_xz < 1)) return false;

							collision_found(hit_dist, float3(hit_pos_xz[0], plane_y, hit_pos_xz[1]), float3(0,normal_y,0));
							return true;
						};

						auto raycast_sides_edge = [&] (float3 ray_pos, float3 ray_dir, float2 cyl_pos2d, float cyl_r, float cyl_z_l,float cyl_z_h) { // edge between block sides which are cylinders in our minowski sum
																																					 // do 2d circle raycase using on xy plane
							float ray_dir2d_len = length((float2)ray_dir);
							if (ray_dir2d_len == 0) return false; // ray parallel to cylinder
							float2 unit_ray_dir2d = ((float2)ray_dir) / ray_dir2d_len;

							float2 circ_rel_p = cyl_pos2d -(float2)ray_pos;

							float closest_p_dist = dot(unit_ray_dir2d, circ_rel_p);
							float2 closest_p = unit_ray_dir2d * closest_p_dist;

							float2 circ_to_closest = closest_p -circ_rel_p;

							float r_sqr = cyl_r*cyl_r;
							float dist_sqr = length_sqr(circ_to_closest);

							if (dist_sqr >= r_sqr) return false; // ray does not cross cylinder

							float chord_half_length = sqrt( r_sqr -dist_sqr );
							float closest_hit_dist2d = closest_p_dist -chord_half_length;
							float furthest_hit_dist2d = closest_p_dist +chord_half_length;

							float hit_dist2d;
							if (closest_hit_dist2d >= 0)		hit_dist2d = closest_hit_dist2d;
							else if (furthest_hit_dist2d >= 0)	hit_dist2d = furthest_hit_dist2d;
							else								return false; // circle hit is on backwards direction of ray, ie. no hit

							float2 rel_hit_xy = hit_dist2d * unit_ray_dir2d;

							// calc hit z
							float rel_hit_z = length(rel_hit_xy) * (ray_dir.z / ray_dir2d_len);

							float3 rel_hit_pos = float3(rel_hit_xy, rel_hit_z);
							float3 hit_pos = ray_pos +rel_hit_pos;

							if (!(hit_pos.z > cyl_z_l && hit_pos.z < cyl_z_h)) return false;

							collision_found(length(rel_hit_pos), hit_pos, float3(normalize(rel_hit_xy -circ_rel_p), 0));
							return true;
						};

						auto raycast_cap = [&] (float3 ray_pos, float3 ray_dir, float plane_z, float normal_z) {
							// normal axis aligned plane raycast
							float delta_z = plane_z -ray_pos.z;

							if (ray_dir.z == 0 || (ray_dir.z * delta_z) < 0) return false; // if ray parallel to plane or ray points away from plane

							float2 delta_xy = delta_z * (((float2)ray_dir) / ray_dir.z);

							float2 plane_hit_xy = ((float2)ray_pos) +delta_xy;

							// check if cylinder base/top circle cap intersects with block top/bottom square
							float2 closest_p = clamp(plane_hit_xy, 0,1);

							float dist_sqr = length_sqr(closest_p -plane_hit_xy);
							if (dist_sqr >= player_r*player_r) return false; // hit outside

							float hit_dist = length(float3(delta_xy, delta_z));
							collision_found(hit_dist, float3(plane_hit_xy, plane_z), float3(0,0, normal_z));
							return true;
						};

						hit = raycast_cap(			pos_local, vel, 1,			+1) || hit; // block top
						hit = raycast_cap(			pos_local, vel, -player_h,	-1) || hit; // block bottom

						hit = raycast_x_side(		pos_local, vel, 0 -player_r, -1 ) || hit;
						hit = raycast_x_side(		pos_local, vel, 1 +player_r, +1 ) || hit;
						hit = raycast_y_side(		pos_local, vel, 0 -player_r, -1 ) || hit;
						hit = raycast_y_side(		pos_local, vel, 1 +player_r, +1 ) || hit;

						hit = raycast_sides_edge(	pos_local, vel, float2( 0, 0), player_r, -player_h, 1 ) || hit;
						hit = raycast_sides_edge(	pos_local, vel, float2( 0,+1), player_r, -player_h, 1 ) || hit;
						hit = raycast_sides_edge(	pos_local, vel, float2(+1, 0), player_r, -player_h, 1 ) || hit;
						hit = raycast_sides_edge(	pos_local, vel, float2(+1,+1), player_r, -player_h, 1 ) || hit;

					}

					lrgba col;

					if (!block_solid) {
						col = srgb(40,40,40,100);
					} else {
						col = hit ? srgb(255,40,40,200) : srgb(255,255,255,150);
					}

					if (draw_dbg) {
						debug_graphics->push_wire_cube((float3)bp + 0.5f, 1, col);
					}
				};

				if (length_sqr(vel_world) != 0) {
					// for all blocks we could be touching during movement by at most one block on each axis
					bpos start =	(bpos)floor(pos_world -float3(player_r,player_r,0)) -1;
					bpos end =		(bpos)ceil(pos_world +float3(player_r,player_r,player_h)) +1;

					bpos bp;
					for (bp.z=start.z; bp.z<end.z; ++bp.z) {
						for (bp.y=start.y; bp.y<end.y; ++bp.y) {
							for (bp.x=start.x; bp.x<end.x; ++bp.x) {
								find_earliest_collision_with_block_by_raycast_minkowski_sum(bp);
							}
						}
					}
				}

				//printf(">>> %f %f,%f %f,%f\n", earliest_collision.dist, pos_world.x,pos_world.y, earliest_collision.hit_pos.x,earliest_collision.hit_pos.y);

				float max_dt = min(t_remain, 1.0f / max_component(abs(vel_world))); // if we are moving so fast that we would move by more than one block on any one axis we will do sub steps of exactly one block

				float earliest_collision_t = earliest_collision.dist / length(vel_world); // inf if there is no collision

				if (earliest_collision_t >= max_dt) {
					pos_world += vel_world * max_dt;
					t_remain -= max_dt;
				} else {

					// handle block collision
					float friction;
					float bounciness;
					float min_bounce_speed;

					if (earliest_collision.normal.z == +1) {
						// hit top of block ie. ground
						friction = world->player.collison_response.falling_ground_friction;
						bounciness = world->player.collison_response.falling_bounciness;
						min_bounce_speed = world->player.collison_response.falling_min_bounce_speed;
					} else {
						// hit side of block or bottom of block ie. wall or ceiling
						friction = world->player.collison_response.wall_friction;
						bounciness = world->player.collison_response.wall_bounciness;
						min_bounce_speed = world->player.collison_response.wall_min_bounce_speed;
					}

					float3 normal = earliest_collision.normal;
					float norm_speed = dot(normal, vel_world); // normal points out of the wall
					float3 norm_vel = normal * norm_speed;

					float3 frict_vel = vel_world -norm_vel;
					frict_vel.z = 0; // do not apply friction on vertical movement
					float frict_speed = length(frict_vel);

					float3 remain_vel = vel_world -norm_vel -frict_vel;

					if (frict_speed != 0) {
						float3 frict_dir = frict_vel / frict_speed;

						float friction_dv = friction * max(-norm_speed, 0.0f); // change in speed due to kinetic friction (unbounded ie. can be larger than our actual velocity)
						frict_vel -= frict_dir * min(friction_dv, frict_speed);
					}

					norm_vel = bounciness * -norm_vel;

					if (length(norm_vel) <= min_bounce_speed) norm_vel = 0;

					vel_world = float3(norm_vel +frict_vel +remain_vel);

					pos_world = earliest_collision.hit_pos;

					pos_world += float3(earliest_collision.normal * COLLISION_SEPERATION_EPSILON); // move player a epsilon distance away from the wall to prevent problems, having this value be 1/1000 instead of sothing very samll prevents small intersection problems that i don't want to debug now

					t_remain -= earliest_collision_t;
				}

				draw_dbg = false;
			}
		};
		trace_player_collision_path();

		world->player.vel = vel_world;
		world->player.pos = pos_world;
	}

	Camera_View view;
	if (activate_flycam) {
		view = flycam.update();
	} else {
		view = world->player.update_post_physics(*world);
	}

	BlockHit highlighted_block_info;
	Block* highlighted_block;

	auto raycast_highlighted_block = [&] () {
		Ray ray;
		ray.dir = (float3x3)view.cam_to_world * float3(0,0,-1);
		ray.pos = view.cam_to_world * float3(0,0,0);
	
		highlighted_block = world->raycast_solid_blocks(ray, 5.0f, &highlighted_block_info);
	};
	raycast_highlighted_block();

	if (highlighted_block && input.buttons[GLFW_MOUSE_BUTTON_LEFT].is_down) { // block breaking

		Chunk* chunk;
		Block* b = world->chunks.query_block(highlighted_block_info.block, &chunk);
		assert( block_props[b->type].collision == CM_SOLID && chunk );

		b->hp_ratio -= 1.0f / 0.3f * input.dt;

		if (b->hp_ratio > 0) {
			chunk->block_only_texture_changed(highlighted_block_info.block);
		} else {

			b->hp_ratio = 0;
			b->type = BT_AIR;

			chunk->block_changed(world->chunks, highlighted_block_info.block);

			//dbg_play_sound();
		}
	}
	{ // block placing
		// keep trying to place block if it was inside player but rmb is still held down
		trigger_place_block = trigger_place_block && input.buttons[GLFW_MOUSE_BUTTON_RIGHT].is_down && highlighted_block;
		
		if (input.buttons[GLFW_MOUSE_BUTTON_RIGHT].went_down)
			trigger_place_block = true;

		if (trigger_place_block && highlighted_block && highlighted_block_info.face >= 0) {

			bpos dir = 0;
			dir[highlighted_block_info.face / 2] = highlighted_block_info.face % 2 ? +1 : -1;

			bpos block_place_pos = highlighted_block_info.block +dir;

			Chunk* chunk;
			Block* b = world->chunks.query_block(block_place_pos, &chunk);

			if (b && chunk) {
				bool block_place_is_inside_player = cylinder_cube_intersect(world->player.pos -(float3)block_place_pos, world->player.radius, world->player.height);

				if (block_props[b->type].collision != CM_SOLID && !block_place_is_inside_player) { // could be BT_NO_CHUNK or BT_OUT_OF_BOUNDS or BT_AIR 

					b->type = BT_EARTH;
					b->hp_ratio = 1;
					b->dbg_tint = 255;

					chunk->block_changed(world->chunks, block_place_pos);

					raycast_highlighted_block(); // make highlighted_block seem more responsive

					trigger_place_block = false;
				}
			}
		}

	}

	if (1) { // chunk update
		constexpr bpos_t chunk_block_count = CHUNK_DIM_X * CHUNK_DIM_Y * CHUNK_DIM_Z;
		bpos_t blocks_to_update = (bpos_t)ceil((float)chunk_block_count * block_update_frequency * input.dt);

		auto mtth_to_prob = [&] (float mtth) -> float {
			return 1 -pow(EULER, -0.693147f / (mtth * block_update_frequency));
		};

		float grass_die_mtth = 5; // seconds
		float grass_die_prob = mtth_to_prob(grass_die_mtth);

		float grass_grow_min_mtth = 1; // seconds
		float grass_grow_max_prob = mtth_to_prob(grass_grow_min_mtth);

		float grass_grow_diagonal_multipiler = 0.5f;
		float grass_grow_step_down_multipiler = 0.75f;
		float grass_grow_step_up_multipiler = 0.6f;

		// grass_grow_max_prob = 4*grass_grow_side_prob +4*grass_grow_diagonal_prob; grass_grow_diagonal_prob = grass_grow_side_prob * grass_grow_diagonal_multipiler
		float grass_grow_side_prob = grass_grow_max_prob / (4 * (1 +grass_grow_diagonal_multipiler));
		float grass_grow_diagonal_prob = grass_grow_side_prob * grass_grow_diagonal_multipiler;

		auto block_update = [&] (Block* b, bpos pos_world, Chunk& chunk) {
			Block* above = world->chunks.query_block(pos_world +bpos(0,0,+1));

			if (/*block_props[b->type].does_autoheal &&*/ b->hp_ratio < 1.0f) {
				b->hp_ratio += 1.0f/5 / block_update_frequency;
				b->hp_ratio = min(b->hp_ratio, 1.0f);

				chunk.block_only_texture_changed(pos_world);
			}
			if (b->type == BT_GRASS && !(above->type == BT_AIR || above->type == BT_OUT_OF_BOUNDS)) {
				if (grass_die_prob > random.uniform()) {
					b->type = BT_EARTH;
					b->hp_ratio = 1;
					chunk.block_only_texture_changed(pos_world);
				}
			}
			if (b->type == BT_EARTH && (above->type == BT_AIR || above->type == BT_OUT_OF_BOUNDS)) {
				float prob = 0;

				bpos2 sides[4] = {
					bpos2(-1,0),
					bpos2(+1,0),
					bpos2(0,-1),
					bpos2(0,+1),
				};
				bpos2 diagonals[4] = {
					bpos2(-1,-1),
					bpos2(+1,-1),
					bpos2(-1,+1),
					bpos2(+1,+1),
				};

				for (bpos2 v : sides) {
					if (	 world->chunks.query_block(pos_world +bpos(v,+1))->type == BT_GRASS) prob += grass_grow_side_prob * grass_grow_step_down_multipiler;
					else if (world->chunks.query_block(pos_world +bpos(v, 0))->type == BT_GRASS) prob += grass_grow_side_prob;
					else if (world->chunks.query_block(pos_world +bpos(v,-1))->type == BT_GRASS) prob += grass_grow_side_prob * grass_grow_step_up_multipiler;
				}

				for (bpos2 v : diagonals) {
					if (	 world->chunks.query_block(pos_world +bpos(v,+1))->type == BT_GRASS) prob += grass_grow_diagonal_prob * grass_grow_step_down_multipiler;
					else if (world->chunks.query_block(pos_world +bpos(v, 0))->type == BT_GRASS) prob += grass_grow_diagonal_prob;
					else if (world->chunks.query_block(pos_world +bpos(v,-1))->type == BT_GRASS) prob += grass_grow_diagonal_prob * grass_grow_step_up_multipiler;
				}

				if (prob > random.uniform()) {
					b->type = BT_GRASS;
					b->hp_ratio = 1;
					chunk.block_only_texture_changed(pos_world);
				}
			}
		};

		static_assert(chunk_block_count == (1 << 16), "");

		auto update_block_pattern = [&] (uint16_t i) -> uint16_t {
			// reverse bits to turn normal x y z block iteration into a somewhat distributed pattern
			i = ((i & 0x00ff) << 8) | ((i & 0xff00) >> 8);
			i = ((i & 0x0f0f) << 4) | ((i & 0xf0f0) >> 4);
			i = ((i & 0x3333) << 2) | ((i & 0xcccc) >> 2);
			i = ((i & 0x5555) << 1) | ((i & 0xaaaa) >> 1);
			return i;
		};

		//printf("frame %d\n", (int)frame_i);

		for (Chunk& chunk : world->chunks) {

			for (bpos_t i=0; i<blocks_to_update; ++i) {
				uint16_t indx = (uint16_t)((cur_chunk_update_block_i +i) % chunk_block_count);
				indx = update_block_pattern(indx);

				Block* b = &chunk.blocks[0][0][indx];

				bpos bp;
				bp.z =  indx / (CHUNK_DIM.y * CHUNK_DIM.x);
				bp.y = (indx % (CHUNK_DIM.y * CHUNK_DIM.x)) / CHUNK_DIM.y;
				bp.x = (indx % (CHUNK_DIM.y * CHUNK_DIM.x)) % CHUNK_DIM.y;

				//printf(">>> %d %d %d\n", (int)bp.x,(int)bp.y,(int)bp.z);

				block_update(b, bp +chunk.chunk_pos_world(), chunk);
			}
		}

		cur_chunk_update_block_i = (cur_chunk_update_block_i +blocks_to_update) % chunk_block_count;
	}

	world->chunks.update_chunk_mesh_and_light();

	//// Draw
	glViewport(0,0, input.window_size.x, input.window_size.y);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClear(GL_DEPTH_BUFFER_BIT);

	common_uniforms.set_view_uniforms(view);
	common_uniforms.set_debug_uniforms();

	gl_enable(GL_CULL_FACE, !(common_uniforms.dbg_wireframe && common_uniforms.wireframe_backfaces));

	{
		int count = 0;

		PROFILE_BEGIN(update_block_brighness);
		// update_block_brighness first because remesh accesses surrounding chunks for blocks on the edge
		for (Chunk& chunk : world->chunks) {
			if (chunk.needs_block_brighness_update) {
				chunk.update_block_brighness();
				++count;
			}
		}
		if (count != 0) PROFILE_END_PRINT(update_block_brighness, "frame: %3d count: %d", frame_counter, count);

		count = 0;

		PROFILE_BEGIN(remesh_upload);
		// remesh and draw
		for (Chunk& chunk : world->chunks) {

			if (chunk.needs_remesh) {
				chunk.remesh(world->chunks, chunk_graphics);
				++count;
			}
		}
		if (count != 0) PROFILE_END_PRINT(remesh_upload, "frame: %3d count: %d", frame_counter, count);
	}

	if (viewing_flycam || world->player.third_person)
		debug_graphics->push_cylinder(world->player.pos + float3(0,0, world->player.height/2), world->player.radius, world->player.height, srgb(255, 40, 255, 130), 32);

	// opaque draw
	chunk_graphics.draw_chunks(world->chunks);

	skybox_graphics.draw();

	// transparent draw
	glEnable(GL_BLEND);

	if (highlighted_block) {
		block_highlight_graphics.draw((float3)highlighted_block_info.block, (BlockFace)(highlighted_block_info.face >= 0 ? highlighted_block_info.face : 0));
	}

	//glCullFace(GL_FRONT);
	//chunk_graphics.draw_chunks_transparent(chunks);
	//glCullFace(GL_BACK);
	chunk_graphics.draw_chunks_transparent(world->chunks);

	glEnable(GL_CULL_FACE);
	debug_graphics->draw();

	glDisable(GL_BLEND);
}
