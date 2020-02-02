
#define _CRT_SECURE_NO_WARNINGS 1

#include <cstdio>
#include <array>
#include <vector>
#include <string>


#include "intersection_test.hpp"
#include "open_simplex_noise/open_simplex_noise.hpp"

#include "game.hpp"
#include "graphics/debug_graphics.hpp"
#include "input.hpp"
#include "glfw_window.hpp"

#include "kissmath.hpp"
#include "kissmath_colors.hpp"
#include "string.hpp"
using namespace kiss;

#include "glad/glad.h"

#include "stb_image.hpp"
#include "gl.hpp"

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

#define PROFILE_BEGIN(name)	auto __profile_##name = Timer::start()
#define PROFILE_END_PRINT(name, format, ...)	printf(">> PROFILE: %s took %8.3f ms  " format "\n", STRINGIFY(name), (__profile_##name).end() * 1000, __VA_ARGS__)

#define PROFILE_END_ACCUM(name)	name += (__profile_##name).end()
#define PROFILE_PRINT(name, format, ...)	printf(">> PROFILE: %s took %8.3f ms  " format "\n", STRINGIFY(name), name * 1000, __VA_ARGS__)

//
std::vector<Shader_old*>			shaders;

Shader_old* new_shader (std::string const& v, std::string const& f, std::initializer_list<old::Uniform> u, std::initializer_list<Shader_old::Uniform_Texture> t) {
	Shader_old* s = new Shader_old(v,f,u,t);

	s->load(); // NOTE: Load shaders instantly on creation

	shaders.push_back(s);
	return s;
}

//static void foliage_alpha_changed ();

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

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_aniso);

		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	}

	tex_breaking.load();
	tex_breaking.upload();

	breaking_frames_count = tex_breaking.dim.y / tex_breaking.dim.x;

	//

	player.respawn();

	inital_chunk( player.pos );

	{
		dbg_heightmap_visualize.alloc_cpu_single_mip(PT_LRGBA8, 512);

		regen_dbg_heightmap_visualize();
	}

	glGenVertexArrays(1, &vao);
}

Game::~Game () {
	glDeleteVertexArrays(1, &vao);
}

void Game::frame () {

	glBindVertexArray(vao);

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

	for (auto* s : shaders)			s->reload_if_needed();

	{
		bool open = ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen);
		
		if (open) ImGui::Checkbox("Toggle Flycam [P]", &activate_flycam);
		if (input.buttons[GLFW_KEY_P].went_down)
			activate_flycam = !activate_flycam;

		if (open) ImGui::DragFloat3("player_spawn_point", &player_spawn_point.x, 0.2f);
		if ((open && ImGui::Button("Respawn Player [Q]")) || input.buttons[GLFW_KEY_Q].went_down) {
			player.respawn();
		}

		ImGui::Separator();

		if (open) flycam.imgui();
		if (open) player.imgui();

		ImGui::Separator();
	}

	//if (option("noise_tree_desity_period", &noise_tree_desity_period))	trigger_regen_chunks = true;
	//if (option("noise_tree_density_amp", &noise_tree_density_amp))			trigger_regen_chunks = true;
	//
	//if (option("world_seed", &world_seed))				trigger_regen_chunks = true;

	if (trigger_regen_chunks) { // chunks currently being processed by thread pool will not be regenerated
		trigger_regen_chunks = false;
		delete_all_chunks();
		inital_chunk(player.pos);
		trigger_dbg_heightmap_visualize = true;
	}

	//option("chunk_drawing_radius", &chunk_drawing_radius);
	//option("chunk_drawing_radius", &chunk_drawing_radius);
	//
	//option("chunk_drawing_radius", &chunk_drawing_radius);
	//option("chunk_generation_radius", &chunk_generation_radius);

	//if (option("dbg_heightmap_visualize_radius", &dbg_heightmap_visualize_radius)) trigger_dbg_heightmap_visualize = true;

	if (trigger_dbg_heightmap_visualize) {
		regen_dbg_heightmap_visualize();
		trigger_dbg_heightmap_visualize = false;
	}

	{ // chunk generation
	  // check all chunk positions within a square of chunk_generation_radius
		chunk_pos_t start =	(chunk_pos_t)floor(	((float2)player.pos -chunk_generation_radius) / (float2)CHUNK_DIM_2D );
		chunk_pos_t end =	(chunk_pos_t)ceil(	((float2)player.pos +chunk_generation_radius) / (float2)CHUNK_DIM_2D );

		// check their actual distance to determine if they should be generated or not
		auto chunk_dist_to_player = [&] (chunk_pos_t pos) {
			bpos2 chunk_origin = pos * CHUNK_DIM_2D;
			return point_square_nearest_dist((float2)chunk_origin, (float2)CHUNK_DIM_2D, (float2)player.pos);
		};
		auto chunk_is_in_generation_radius = [&] (chunk_pos_t pos) {
			return chunk_dist_to_player(pos) <= chunk_generation_radius;
		};

		std::vector<chunk_pos_t> chunks_to_generate;

		chunk_pos_t cp;
		for (cp.x = start.x; cp.x<end.x; ++cp.x) {
			for (cp.y = start.y; cp.y<end.y; ++cp.y) {
				if (chunk_is_in_generation_radius(cp) && !query_chunk(cp)) {
					// chunk is within chunk_generation_radius and not yet generated
					chunks_to_generate.push_back(cp);
				}
			}
		}

		std::sort(chunks_to_generate.begin(), chunks_to_generate.end(),
			[&] (chunk_pos_t l, chunk_pos_t r) { return chunk_dist_to_player(l) < chunk_dist_to_player(r); }
		);

		PROFILE_BEGIN(generate_new_chunk);

		int count = 0;
		for (auto& cp : chunks_to_generate) {
			generate_new_chunk(cp);
			if (++count == max_chunks_generated_per_frame) break;
		}
		if (count != 0) PROFILE_END_PRINT(generate_new_chunk, "frame: %3d generated %d chunks", frame_counter, count);

	}

	//overlay_line(prints("chunks in ram:   %4d %6d MB (%d KB per chunk) chunk is %dx%dx%d blocks", (int)chunks.size(), (int)(sizeof(Chunk)*chunks.size()/1024/1024), (int)(sizeof(Chunk)/1024), (int)CHUNK_DIM.x,(int)CHUNK_DIM.y,(int)CHUNK_DIM.z));

	{ // player position (collision and movement dynamics)
		constexpr float COLLISION_SEPERATION_EPSILON = 0.001f;

		// 
		bool player_stuck_in_solid_block;
		bool player_on_ground;

		auto check_blocks_around_player = [&] () {
			{ // for all blocks we could be touching
				bpos start =	(bpos)floor(player.pos -float3(player.collision_r,player.collision_r,0));
				bpos end =		(bpos)ceil(player.pos +float3(player.collision_r,player.collision_r,player.collision_h));

				bool any_intersecting = false;

				bpos bp;
				for (bp.z=start.z; bp.z<end.z; ++bp.z) {
					for (bp.y=start.y; bp.y<end.y; ++bp.y) {
						for (bp.x=start.x; bp.x<end.x; ++bp.x) {

							auto* b = query_block(bp);
							bool block_solid = !block_props[b->type].traversable;

							bool intersecting = cylinder_cube_intersect(player.pos -(float3)bp, player.collision_r,player.collision_h);

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

				bpos_t pos_z = floori(player.pos.z);

				player_on_ground = false;

				if ((player.pos.z -pos_z) <= COLLISION_SEPERATION_EPSILON*1.05f && player.vel.z == 0) {

					bpos2 start =	(bpos2)floor((float2)player.pos -player.collision_r);
					bpos2 end =		(bpos2)ceil((float2)player.pos +player.collision_r);

					bpos bp;
					bp.z = pos_z -1;

					for (bp.y=start.y; bp.y<end.y; ++bp.y) {
						for (bp.x=start.x; bp.x<end.x; ++bp.x) {

							auto* b = query_block(bp);
							bool block_solid = !block_props[b->type].traversable;

							if (block_solid && circle_square_intersect((float2)player.pos -(float2)(bpos2)bp, player.collision_r)) player_on_ground = true;
						}
					}
				}
			}

		};
		check_blocks_around_player();

		if (!activate_flycam) {
			player.update_controls(player_on_ground);
		}
		player.update_physics(player_on_ground);

		//option("draw_debug_overlay", &draw_debug_overlay);

		float3 pos_world = player.pos;
		float3 vel_world = player.vel;

		auto trace_player_collision_path = [&] () {
			float player_r = player.collision_r;
			float player_h = player.collision_h;

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

					auto* b = query_block(bp);
					bool block_solid = !block_props[b->type].traversable;

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
						friction = player.falling_ground_friction;
						bounciness = player.falling_bounciness;
						min_bounce_speed = player.falling_min_bounce_speed;
					} else {
						// hit side of block or bottom of block ie. wall or ceiling
						friction = player.wall_friction;
						bounciness = player.wall_bounciness;
						min_bounce_speed = player.wall_min_bounce_speed;
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

		player.vel = vel_world;
		player.pos = pos_world;
	}



	Camera_View view;
	if (activate_flycam) {
		view = flycam.update();
	} else {
		view = player.update_post_physics();
	}

	common_uniforms.set_view_uniforms(view);
	common_uniforms.set_debug_uniforms();

	Block*	highlighted_block;
	bpos	highlighted_block_pos;
	int		highlighted_block_face;
	auto raycast_highlighted_block = [&] () {
		auto raycast_block = [&] (float3 ray_pos, float3 ray_dir, float max_dist, bpos* hit_block, int* hit_face) -> Block* {

			bpos step_delta = bpos(	(bpos_t)normalize(ray_dir.x),
				(bpos_t)normalize(ray_dir.y),
				(bpos_t)normalize(ray_dir.z) );


			float3 step = float3(		length(ray_dir / abs(ray_dir.x)),
				length(ray_dir / abs(ray_dir.y)),
				length(ray_dir / abs(ray_dir.z)) );

			float3 ray_pos_floor = floor(ray_pos);

			float3 pos_in_block = ray_pos -ray_pos_floor;

			float3 next = step * select(ray_dir > 0, 1 -pos_in_block, pos_in_block);
			next = select(ray_dir != 0, next, INF);

			auto find_next_axis = [&] (float3 next) {
				if (		next.x < next.y && next.x < next.z )	return 0;
				else if (	next.y < next.z )						return 1;
				else												return 2;
			};

			bpos cur_block = (bpos)ray_pos_floor;

			int first_axis = find_next_axis(next);
			int face = (first_axis*2 +(step_delta[first_axis] > 0 ? 1 : 0));

			for (;;) {

				//highlight_block(cur_block);
				Block* b = query_block(cur_block);
				if (block_props[b->type].breakable) {
					*hit_block = cur_block;
					*hit_face = face;
					return b;
				}

				int axis = find_next_axis(next);

				face = (axis*2 +(step_delta[axis] < 0 ? 1 : 0));

				if (next[axis] > max_dist) return nullptr;

				next[axis] += step[axis];
				cur_block[axis] += step_delta[axis];
			}

		};

		float3 ray_dir = (float3x3)view.cam_to_world * float3(0,0,-1);
		float3 ray_pos = view.cam_to_world * float3(0,0,0);

		highlighted_block = raycast_block(ray_pos, ray_dir, 4.5f, &highlighted_block_pos, &highlighted_block_face);
	};
	raycast_highlighted_block();

	if (highlighted_block && input.buttons[GLFW_MOUSE_BUTTON_LEFT].is_down) { // block breaking

		Chunk* chunk;
		Block* b = query_block(highlighted_block_pos, &chunk);
		assert( block_props[b->type].breakable && chunk );

		b->hp_ratio -= 1.0f / 0.3f * input.dt;

		if (b->hp_ratio > 0) {
			chunk->block_only_texture_changed(highlighted_block_pos);
		} else {

			b->hp_ratio = 0;
			b->type = BT_AIR;

			chunk->block_changed(highlighted_block_pos);

			//dbg_play_sound();
		}
	}
	{ // block placing
		// keep trying to place block if it was inside player but rmb is still held down
		trigger_place_block = trigger_place_block && input.buttons[GLFW_MOUSE_BUTTON_RIGHT].is_down && highlighted_block;
		
		if (input.buttons[GLFW_MOUSE_BUTTON_RIGHT].went_down)
			trigger_place_block = true;

		if (trigger_place_block && highlighted_block) {

			bpos dir = 0;
			dir[highlighted_block_face / 2] = highlighted_block_face % 2 ? +1 : -1;

			bpos block_place_pos = highlighted_block_pos +dir;

			Chunk* chunk;
			Block* b = query_block(block_place_pos, &chunk);

			bool block_place_is_inside_player = cylinder_cube_intersect(player.pos -(float3)block_place_pos, player.collision_r,player.collision_h);

			if (block_props[b->type].replaceable && !block_place_is_inside_player) { // could be BT_NO_CHUNK or BT_OUT_OF_BOUNDS or BT_AIR 

				b->type = BT_EARTH;
				b->hp_ratio = 1;
				b->dbg_tint = 255;

				//dbg_play_sound();

				chunk->block_changed(block_place_pos);

				raycast_highlighted_block(); // make highlighted_block seem more responsive

				trigger_place_block = false;
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

		auto block_update = [&] (Block* b, bpos pos_world, Chunk* chunk) {
			Block* above = query_block(pos_world +bpos(0,0,+1));

			if (block_props[b->type].does_autoheal && b->hp_ratio < 1.0f) {
				b->hp_ratio += 1.0f/5 / block_update_frequency;
				b->hp_ratio = min(b->hp_ratio, 1.0f);

				chunk->block_only_texture_changed(pos_world);
			}
			if (b->type == BT_GRASS && !(above->type == BT_AIR || above->type == BT_OUT_OF_BOUNDS)) {
				if (grass_die_prob > random::flt()) {
					b->type = BT_EARTH;
					b->hp_ratio = 1;
					chunk->block_only_texture_changed(pos_world);
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
					if (	 query_block(pos_world +bpos(v,+1))->type == BT_GRASS) prob += grass_grow_side_prob * grass_grow_step_down_multipiler;
					else if (query_block(pos_world +bpos(v, 0))->type == BT_GRASS) prob += grass_grow_side_prob;
					else if (query_block(pos_world +bpos(v,-1))->type == BT_GRASS) prob += grass_grow_side_prob * grass_grow_step_up_multipiler;
				}

				for (bpos2 v : diagonals) {
					if (	 query_block(pos_world +bpos(v,+1))->type == BT_GRASS) prob += grass_grow_diagonal_prob * grass_grow_step_down_multipiler;
					else if (query_block(pos_world +bpos(v, 0))->type == BT_GRASS) prob += grass_grow_diagonal_prob;
					else if (query_block(pos_world +bpos(v,-1))->type == BT_GRASS) prob += grass_grow_diagonal_prob * grass_grow_step_up_multipiler;
				}

				if (prob > random::flt()) {
					b->type = BT_GRASS;
					b->hp_ratio = 1;
					chunk->block_only_texture_changed(pos_world);
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

		for (auto& chunk_hash_pair : chunks) {
			auto& chunk = chunk_hash_pair.second;

			for (bpos_t i=0; i<blocks_to_update; ++i) {
				uint16_t indx = (uint16_t)((cur_chunk_update_block_i +i) % chunk_block_count);
				indx = update_block_pattern(indx);

				Block* b = &chunk->blocks[0][0][indx];

				bpos bp;
				bp.z =  indx / (CHUNK_DIM.y * CHUNK_DIM.x);
				bp.y = (indx % (CHUNK_DIM.y * CHUNK_DIM.x)) / CHUNK_DIM.y;
				bp.x = (indx % (CHUNK_DIM.y * CHUNK_DIM.x)) % CHUNK_DIM.y;

				//printf(">>> %d %d %d\n", (int)bp.x,(int)bp.y,(int)bp.z);

				block_update(b, bp +chunk->chunk_origin_block_world(), chunk);
			}
		}

		cur_chunk_update_block_i = (cur_chunk_update_block_i +blocks_to_update) % chunk_block_count;
	}

	//// Draw
	for (auto* s : shaders) { // set common uniforms
		if (s->valid()) {
			s->bind();
			s->set_unif("screen_dim", (float2)input.window_size);
			//s->set_unif("mcursor_pos", inp.bottom_up_mcursor_pos());
		}
	}

	glViewport(0,0, input.window_size.x, input.window_size.y);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	if (shad_blocks->valid()) {

		bind_texture_unit(0, tex_block_atlas);
		bind_texture_unit(1, &tex_breaking);

		shad_blocks->bind();
		shad_blocks->set_unif("draw_wireframe",	draw_wireframe);
		shad_blocks->set_unif("show_dbg_tint",	show_dbg_tint);

		shad_blocks->set_unif("world_to_cam",	(float4x4)view.world_to_cam);
		shad_blocks->set_unif("cam_to_world",	(float4x4)view.cam_to_world);
		shad_blocks->set_unif("cam_to_clip",	view.cam_to_clip);

		shad_blocks->set_unif("texture_res", texture_res);
		shad_blocks->set_unif("atlas_textures_count", atlas_textures_count);
		shad_blocks->set_unif("breaking_frames_count", breaking_frames_count);

		shad_blocks->set_unif("alpha_test", graphics_settings.foliage_alpha);

		int count = 0;

		PROFILE_BEGIN(update_block_brighness);
		// update_block_brighness first because remesh accesses surrounding chunks for blocks on the edge
		for (auto& chunk_hash_pair : chunks) {
			auto& chunk = chunk_hash_pair.second;

			if (chunk->needs_block_brighness_update) {
				chunk->update_block_brighness();
				++count;
			}
		}
		//if (count != 0) PROFILE_END_PRINT(update_block_brighness, "frame: %3d count: %d", frame_i, count);

		count = 0;

		PROFILE_BEGIN(remesh_upload);
		// remesh and draw
		for (auto& chunk_hash_pair : chunks) {
			auto& chunk = chunk_hash_pair.second;

			if (chunk->needs_remesh) {
				chunk->remesh();
				chunk->vbo.upload();
				chunk->vbo_transperant.upload();
				++count;
			}
		}
		//if (count != 0) PROFILE_END_PRINT(remesh_upload, "frame: %3d count: %d", frame_i, count);

		// draw opaque
		for (auto& chunk_hash_pair : chunks) {
			auto& chunk = chunk_hash_pair.second;

			chunk->vbo.draw_entire(shad_blocks);
		}

		glEnable(GL_BLEND);
		shad_blocks->set_unif("alpha_test", false);

		// draw transperant
		for (auto& chunk_hash_pair : chunks) {
			auto& chunk = chunk_hash_pair.second;

			chunk->vbo_transperant.draw_entire(shad_blocks);
		}

		glDisable(GL_BLEND);
	}

	if (highlighted_block) {
		block_highlight_graphics.draw((float3)highlighted_block_pos, (BlockFace)highlighted_block_face);
	}

	debug_graphics->push_cylinder(player.pos + float3(0,0, player.collision_h/2), player.collision_r, player.collision_h, srgb(255, 40, 255, 230), 32);
	
	debug_graphics->draw();
	skybox_graphics.draw();
}

float elev_freq = 400, elev_amp = 25;
float rough_freq = 220;
float detail0_freq = 70, detail0_amp = 12;
float detail1_freq = 20, detail1_amp = 3;
float detail2_freq = 3, detail2_amp = 0.14f;

#if 0
int main (int argc, char** argv) {
	
	
	//
	imgui_init();
	
	for (frame_i=0;; ++frame_i) {
		//printf("frame: %d\n", frame_i);
		
		imgui_begin(dt, inp.wnd_dim, inp.mcursor_pos_px, inp.mouse_wheel_diff, lmb_down, rmb_down);
		
		option("draw_wireframe", &draw_wireframe);
		option("show_dbg_tint", &show_dbg_tint);
		
		overlay_line(prints("mouse:   %4d %4d -> %.2f %.2f", inp.mcursor_pos_px.x,inp.mcursor_pos_px.y, mouse.x,mouse.y));
		
		{ // various options
			option("fixed_dt",			&fixed_dt);
			option("max_variable_dt",	&max_variable_dt);
			option("fixed_dt_dt",		&fixed_dt_dt);
			
			option("viewing_flycam",	&viewing_flycam);
			option("controling_flycam",	&controling_flycam);
			
			flycam.options();
			
			option("initial_player_pos_world",		&initial_player_pos_world);
			option("initial_player_vel_world",		&initial_player_vel_world);
			
			player.options();
			
			{
				bool tmp = block_props[BT_NO_CHUNK].traversable;
				option("unloaded_chunks_traversable", &tmp);
				block_props[BT_NO_CHUNK].traversable = tmp;
			}
			if (option("unloaded_chunks_dark",		&B_NO_CHUNK.dark)) for (auto& c : chunks) c.second->needs_remesh = true;
		}
	}
	
	return 0;
}
#endif
