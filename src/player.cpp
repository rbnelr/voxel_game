#include "common.hpp"
#include "player.hpp"
#include "game.hpp"
#include "engine/window.hpp"

void Player::update_controls (Input& I, Game& game) {
	memset(&inp, 0, sizeof(inp));

	if (!game.activate_flycam || game.creative_mode) {

		inp.attack      = !inventory.is_open && I.buttons[MOUSE_BUTTON_LEFT].went_down;
		inp.attack_held = !inventory.is_open && I.buttons[MOUSE_BUTTON_LEFT].is_down;

		inp.build_held  = !inventory.is_open && I.buttons[MOUSE_BUTTON_RIGHT].is_down;

		inventory.toolbar.selected -= I.mouse_wheel_delta;
		inventory.toolbar.selected = wrap(inventory.toolbar.selected, 0, 10);

		for (int i=0; i<10; ++i) {
			if (I.buttons[KEY_0 + i].went_down) {
				inventory.toolbar.selected = i == 0 ? 9 : i - 1; // key '1' is actually slot 0, key '0' is slot 9
				break; // lowest key counts
			}
		}

		if (I.buttons[KEY_E].went_down) {
			inventory.is_open = !inventory.is_open;
			I.set_cursor_mode(g_window, inventory.is_open);
		}
	}

	if (!game.activate_flycam) {
		//// toggle camera view
		if (I.buttons[KEY_F].went_down)
			third_person = !third_person;

		inp.move_dir = 0;
		if (I.buttons[KEY_A].is_down) inp.move_dir.x -= 1;
		if (I.buttons[KEY_D].is_down) inp.move_dir.x += 1;
		if (I.buttons[KEY_S].is_down) inp.move_dir.y -= 1;
		if (I.buttons[KEY_W].is_down) inp.move_dir.y += 1;
		inp.move_dir = normalizesafe(inp.move_dir);

		inp.jump_held = I.buttons[KEY_SPACE].is_down;
		inp.sprint    = I.buttons[KEY_LEFT_SHIFT].is_down;

		//// look
		Camera& cam = third_person ? tps_camera : fps_camera;

		rotate_with_mouselook(I, &rot_ae.x, &rot_ae.y, cam.vfov);
	}
}

void Player::update_movement (Input& I, Game& game) {
	bool stuck;
	bool grounded = calc_ground_contact(game.chunks, &stuck);

	//// walking
	float2x2 body_rotation = rotate2(rot_ae.x);

	{
		float target_speed = inp.sprint ? run_speed : walk_speed;
		float2 target_vel = body_rotation * (inp.move_dir * target_speed);


		float2 delta_vel = target_vel - (float2)vel;
		float delta_speed = length(delta_vel);

		float accel = delta_speed * walk_accel_proport + walk_accel_base;

		delta_vel = normalizesafe(delta_vel) * min(accel * I.dt, delta_speed);
		vel += float3(delta_vel, 0);
	}

#if 0 // movement speed plotting to better develop movement code
	{
		static constexpr int COUNT = 128;
		static float vels[COUNT] = {};
		static float poss[COUNT] = {};
		static int cur = 0;

		if (!I.pause_time) {
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
	if (inp.jump_held/*went_down*/ && grounded)
		vel += jump_impulse;
}

void Player::update (Input& I, Game& game) {

	game.player_view = calc_matricies(I, game.chunks);

	game.view = game.player_view;
	if (game.activate_flycam)
		game.view = game.flycam.update(I);

	
	auto& block = selected_block;

	bool was_selected = block.is_selected;
	int3 old_pos = block.hit.pos;

	block.is_selected = false;

	if (!game.activate_flycam || game.creative_mode) {
		calc_selected_block(block, game, game.view, break_block.reach);

		if (!was_selected || !block.is_selected || old_pos != block.hit.pos) {
			block.damage = 0;
		}

		break_block.update(I, game, *this);
		block_place.update(I, game, *this);
	}
}

Camera_View Player::calc_matricies (Input& I, Chunks& chunks) {
	float3x3 body_rotation = rotate3_Z(rot_ae.x);
	float3x3 body_rotation_inv = rotate3_Z(-rot_ae.x);

	float3x3 head_elevation = rotate3_X(rot_ae.y);
	float3x3 head_elevation_inv = rotate3_X(-rot_ae.y);

	float3 cam_pos = 0;
	if (third_person)
		cam_pos = calc_third_person_cam_pos(chunks, body_rotation, head_elevation);

	Camera& cam = third_person ? tps_camera : fps_camera;

	float3x4 world_to_head = head_elevation_inv * translate(-head_pivot) * body_rotation_inv * translate(-pos);
	head_to_world = translate(pos) * body_rotation * translate(head_pivot) * head_elevation;

	Camera_View view;
	view.world_to_cam = rotate3_X(-deg(90)) * translate(-cam_pos) * world_to_head;
	view.cam_to_world = head_to_world * translate(cam_pos) * rotate3_X(deg(90));
	view.cam_to_clip = cam.calc_cam_to_clip(I.window_size, &view.frustrum, &view.clip_to_cam);
	view.clip_near = cam.clip_near;
	view.clip_far = cam.clip_far;
	view.calc_frustrum();

	return view;
}

float3 Player::calc_third_person_cam_pos (Chunks& chunks, float3x3 body_rotation, float3x3 head_elevation) {
	Ray ray;
	ray.pos = pos + body_rotation * (head_pivot + tps_camera_base_pos);
	ray.dir = body_rotation * head_elevation * tps_camera_dir;

	float dist = tps_camera_dist;

	//{
	//	float hit_dist;
	//	if (world.raycast_breakable_blocks(world.player.selected_block, ray, dist, false, &hit_dist))
	//		dist = max(hit_dist - 0.05f, 0.0f);
	//}

	return tps_camera_base_pos + tps_camera_dir * dist;
}

void Player::calc_selected_block (SelectedBlock& block, Game& game, Camera_View& view, float reach) {
	Ray ray;
	ray.dir = (float3x3)view.cam_to_world * float3(0,0,-1);
	ray.pos = view.cam_to_world * float3(0,0,0);

	game.raycast_breakable_blocks(block, ray, reach, game.creative_mode);
}

void BreakBlock::update (Input& I, Game& game, Player& player) {
	auto& button = I.buttons[MOUSE_BUTTON_LEFT];
	bool inp = player.selected_block ? player.inp.attack_held : player.inp.attack;

	float anim_hit_t = 0;

	//clog(INFO, "[BreakBlock] anim_t: %f", anim_t);

	if (anim_t > 0 || inp) {
		anim_t += anim_speed * I.dt;
	}
	if (!anim_triggered && anim_t > anim_hit_t && inp) {
		//clog(INFO, "[BreakBlock] anim hit");
		if (player.selected_block) {
			game.apply_damage(player.selected_block, player.inventory.toolbar.get_selected(), game.creative_mode);
			hit_sound.play(1, /*random.uniform(0.95f, 1.05f)*/1);
		}
		anim_triggered = true;
	}
	if (anim_t >= 1) {
		//clog(INFO, "[BreakBlock] anim over");
		anim_t = 0;
		anim_triggered = false;
	}
}

void BlockPlace::update (Input& I, Game& game, Player& player) {
	auto& item = player.inventory.toolbar.get_selected();
	bool can_place = item.is_block() && item.block.count > 0;

	bool inp = player.inp.build_held && can_place;
	if (inp && anim_t >= anim_speed / repeat_speed) {
		anim_t = 0;
	}
	bool trigger = inp && anim_t == 0;

	if (trigger && player.selected_block && can_place) {
		int3 offs = 0;
		if (player.selected_block.hit.face >= 0)
			offs[player.selected_block.hit.face / 2] = (player.selected_block.hit.face % 2) ? +1 : -1;

		int3 block_place_pos = player.selected_block.hit.pos + offs;

		bool block_place_is_inside_player = cylinder_cube_intersect(player.pos -(float3)block_place_pos, player.radius, player.height);

		if (!block_place_is_inside_player || g_assets.block_types[(block_id)item.id].collision != CM_SOLID) {
			game.try_place_block(block_place_pos, (block_id)item.id);
		} else {
			trigger = false;
		}
	}
	
	if (trigger || anim_t > 0) {
		anim_t += anim_speed * I.dt;

		if (anim_t >= 1) {
			anim_t = 0;
		}
	}
}

//// Physics

bool Player::calc_ground_contact (Chunks& chunks, bool* stuck) {
	{ // Check block intersection to see if we are somehow stuck inside a block
		int3 start =	(int3)floor(pos -float3(radius, radius, 0));
		int3 end =		(int3)ceil(pos +float3(radius, radius, height));

		bool any_intersecting = false;

		for (int z=start.z; z<end.z; ++z) {
			for (int y=start.y; y<end.y; ++y) {
				for (int x=start.x; x<end.x; ++x) {

					auto b = chunks.read_block(x,y,z);
					bool block_solid = g_assets.block_types[b].collision == CM_SOLID;

					bool intersecting = block_solid && cylinder_cube_intersect(pos -(float3)int3(x,y,z), radius, height);

					//if (0) {
					//	lrgba col;
					//
					//	if (!block_solid) {
					//		col = srgba(40,40,40,100);
					//	} else {
					//		col = intersecting ? srgba(255,40,40,200) : srgba(255,255,255,150);
					//	}
					//
					//	debug_graphics->push_wire_cube(0.5f, 1, col);
					//}

					any_intersecting = any_intersecting || (intersecting && block_solid);
				}
			}
		}

		*stuck = any_intersecting; // player somehow ended up inside a block
	}

	bool grounded = false;
	{ // for all blocks we could be standing on

		int pos_z = floori(pos.z);

		//logf("pos.z fract: %10.8f  vel.z: %10.8f", (pos.z - pos_z), vel.z);
		if ((pos.z - pos_z) <= COLLISION_EPSILON * 1.5f && vel.z == 0) {

			int2 start =	(int2)floor((float2)pos - radius);
			int2 end =		(int2)ceil ((float2)pos + radius);

			int z = pos_z -1;

			for (int y=start.y; y<end.y; ++y) {
				for (int x=start.x; x<end.x; ++x) {

					auto b = chunks.read_block(x,y,z);

					bool block_solid = g_assets.block_types[b].collision == CM_SOLID;
					if (block_solid && circle_square_intersect((float2)pos -(float2)int2(x,y), radius))
						grounded = true; // cylinder base touches at least one soild block
				}
			}
		}
	}

	return grounded;
}

