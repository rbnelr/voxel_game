#include "player.hpp"
#include "world.hpp"
#include "input.hpp"
#include "physics.hpp"
#include "util/collision.hpp"

float3	player_spawn_point = float3(0,0,34);

void Tool::update (World& world, PlayerGraphics const& graphics, SelectedBlock const& selected_block) {
	auto& button = input.buttons[GLFW_MOUSE_BUTTON_LEFT];
	bool inp = selected_block ? button.is_down : button.went_down;

	if (anim_t > 0 || inp) {
		anim_t += anim_speed * input.dt;
	}
	if (!anim_triggered && anim_t >= graphics.anim_hit_t) {
		if (selected_block) {
			world.apply_damage(selected_block, damage);
		}
		anim_triggered = true;
	}
	if (anim_t >= 1) {
		anim_t = 0;
		anim_triggered = false;
	}
}

void BlockPlace::update (World& world, Player const& player, SelectedBlock const& selected_block) {

	bool trigger = input.buttons[GLFW_MOUSE_BUTTON_RIGHT].is_down && anim_t == 0;

	if (trigger || anim_t > 0) {
		anim_t += anim_speed * input.dt;

		if (anim_t >= 1) {
			anim_t = 0;
		}
	}

	if (trigger && selected_block) {
		bpos offs = 0;
		offs[selected_block.face / 2] = (selected_block.face % 2) ? +1 : -1;

		bpos block_place_pos = selected_block.pos + offs;

		bool block_place_is_inside_player = cylinder_cube_intersect(player.pos -(float3)block_place_pos, player.radius, player.height);

		if (!block_place_is_inside_player) {
			world.try_place_block(block_place_pos, BT_EARTH);
		}
	}
}

bool Player::calc_ground_contact (World& world, bool* stuck) {
	{ // Check block intersection to see if we are somehow stuck inside a block
		bpos start =	(bpos)floor(pos -float3(radius, radius, 0));
		bpos end =		(bpos)ceil(pos +float3(radius, radius, height));

		bool any_intersecting = false;

		bpos bp;
		for (bp.z=start.z; bp.z<end.z; ++bp.z) {
			for (bp.y=start.y; bp.y<end.y; ++bp.y) {
				for (bp.x=start.x; bp.x<end.x; ++bp.x) {

					auto* b = world.chunks.query_block(bp);
					bool block_solid = block_props[b->type].collision == CM_SOLID;

					bool intersecting = block_solid && cylinder_cube_intersect(pos -(float3)bp, radius, height);

					if (0) {
						lrgba col;

						if (!block_solid) {
							col = srgba(40,40,40,100);
						} else {
							col = intersecting ? srgba(255,40,40,200) : srgba(255,255,255,150);
						}

						debug_graphics->push_wire_cube(0.5f, 1, col);
					}

					any_intersecting = any_intersecting || (intersecting && block_solid);
				}
			}
		}

		*stuck = any_intersecting; // player somehow ended up inside a block
	}

	bool grounded = false;
	{ // for all blocks we could be standing on

		bpos_t pos_z = floori(pos.z);

		if ((pos.z -pos_z) <= COLLISION_SEPERATION_EPSILON * 1.05f && vel.z == 0) {

			bpos2 start =	(bpos2)floor((float2)pos - radius);
			bpos2 end =		(bpos2)ceil ((float2)pos + radius);

			bpos bp;
			bp.z = pos_z -1;

			for (bp.y=start.y; bp.y<end.y; ++bp.y) {
				for (bp.x=start.x; bp.x<end.x; ++bp.x) {

					auto* b = world.chunks.query_block(bp);

					bool block_solid = block_props[b->type].collision == CM_SOLID;
					if (block_solid && circle_square_intersect((float2)pos -(float2)(bpos2)bp, radius))
						grounded = true; // cylinder base touches at least one soild block
				}
			}
		}
	}

	return grounded;
}

void Player::update_movement_controls (World& world) {
	bool stuck;
	bool grounded = calc_ground_contact(world, &stuck);

	//// toggle camera view
	if (input.buttons[GLFW_KEY_F].went_down)
		third_person = !third_person;

	Camera& cam = third_person ? tps_camera : fps_camera;

	//// look
	rotate_with_mouselook(&rot_ae.x, &rot_ae.y, cam.vfov);

	//// walking
	float2x2 body_rotation = rotate2(rot_ae.x);

	{
		float2 input_dir = 0;
		if (input.buttons[GLFW_KEY_A].is_down) input_dir.x -= 1;
		if (input.buttons[GLFW_KEY_D].is_down) input_dir.x += 1;
		if (input.buttons[GLFW_KEY_S].is_down) input_dir.y -= 1;
		if (input.buttons[GLFW_KEY_W].is_down) input_dir.y += 1;
		input_dir = normalizesafe(input_dir);

		bool input_fast = input.buttons[GLFW_KEY_LEFT_SHIFT].is_down;

		float target_speed = input_fast ? run_speed : walk_speed;
		float2 target_vel = body_rotation * (input_dir * target_speed);

		
		float2 delta_vel = target_vel - (float2)vel;
		float delta_speed = length(delta_vel);

		float accel = delta_speed * walk_accel_proport + walk_accel_base;

		delta_vel = normalizesafe(delta_vel) * min(accel * input.dt, delta_speed);
		vel += float3(delta_vel, 0);
	}

#if 0 // movement speed plotting to better develop movement code
	{
		static constexpr int COUNT = 128;
		static float vels[COUNT] = {};
		static float poss[COUNT] = {};
		static int cur = 0;

		if (!input.pause_time) {
			vels[cur] = length((float2)vel);
			poss[cur++] = pos.x;
			cur %= COUNT;
		}

		ImGui::SetNextItemWidth(-1);
		ImGui::PlotLines("###_debug_vel", vels, COUNT, cur, "player.vel", 0, 15, ImVec2(0, 100));

		ImGui::SetNextItemWidth(-1);
		ImGui::PlotLines("###_debug_pos", poss, COUNT, cur, "player.pos", -7, 7, ImVec2(0, 100));
	}
#endif
	
	//// jumping
	// TODO: player_on_ground is not reliable because of a hack in the collision system, so went_down does not work yet
	if (input.buttons[GLFW_KEY_SPACE].is_down/*went_down*/ && grounded)
		vel += jump_impulse;
}

float3 Player::calc_third_person_cam_pos (World& world, float3x3 body_rotation, float3x3 head_elevation) {
	Ray ray;
	ray.pos = pos + body_rotation * (head_pivot + tps_camera_base_pos);
	ray.dir = body_rotation * head_elevation * tps_camera_dir;

	float dist = tps_camera_dist;
	
	{
		float hit_dist;
		if (world.raycast_solid_blocks(ray, dist, &hit_dist))
			dist = max(hit_dist - 0.05f, 0.0f);
	}

	return tps_camera_base_pos + tps_camera_dir * dist;
}

Camera_View Player::update_post_physics (World& world, PlayerGraphics const& graphics, SelectedBlock* selected_block) {
	float3x3 body_rotation = rotate3_Z(rot_ae.x);
	float3x3 body_rotation_inv = rotate3_Z(-rot_ae.x);

	float3x3 head_elevation = rotate3_X(rot_ae.y);
	float3x3 head_elevation_inv = rotate3_X(-rot_ae.y);

	float3 cam_pos = 0;
	if (third_person)
		cam_pos = calc_third_person_cam_pos(world, body_rotation, head_elevation);

	Camera& cam = third_person ? tps_camera : fps_camera;

	float3x4 world_to_head = head_elevation_inv * translate(-head_pivot) * body_rotation_inv * translate(-pos);
	         head_to_world = translate(pos) * body_rotation * translate(head_pivot) * head_elevation;
	
	Camera_View view;
	view.world_to_cam = rotate3_X(-deg(90)) * translate(-cam_pos) * world_to_head;
	view.cam_to_world = head_to_world * translate(cam_pos) * rotate3_X(deg(90));
	view.cam_to_clip = cam.calc_cam_to_clip();

	*selected_block = calc_selected_block(world);
	tool.update(world, graphics, *selected_block);
	block_place.update(world, *this, *selected_block);

	return view;
}

SelectedBlock Player::calc_selected_block (World& world) {
	Ray ray;
	ray.dir = (float3x3)head_to_world * float3(0,+1,0);
	ray.pos = head_to_world * float3(0,0,0);

	return world.raycast_solid_blocks(ray, tool.reach);
}
