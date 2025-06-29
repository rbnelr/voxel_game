#pragma once
#include "common.hpp"
#include "assets.hpp"
#include "camera.hpp"
#include "physics.hpp"
#include "audio.hpp"
#include "engine.hpp"
#include "inventory.hpp"
#include "block_interaction.hpp"

struct Block;
class Game;

class Player {
public:
	SERIALIZE(Player, pos, vel, rot_ae, crouching_progress, third_person,
		visual_dynamics, movement_params)

	// Player ground position
	float3	pos = 0;

	// Player velocity
	float3	vel = 0;

	// Player look rotation
	float2	rot_ae = float2(deg(0), deg(-10)); // azimuth elevation

	float crouching_progress = 0;
	float crouching_change = 0;

	bool is_crouched () { return crouching_progress > 0.75f; }

	//// Physics
	float width = 0.6f;

	float collision_height = 1.7f;
	float crouch_height = 0.99f;

	// Collision box height (starting at player origin)
	AABB collision_local_aabb (float crouching_progress) {
		return {
			float3(-width*0.5f,-width*0.5f, 0),
			float3(+width*0.5f,+width*0.5f, lerp(collision_height, crouch_height, crouching_progress)),
		};
	}
	AABB collision_world_aabb () {
		return collision_local_aabb(crouching_progress) + pos;
	}

	// Camera position relative to player origin
	// Smoothly interpolates with head_pivot()
	float3 head_pivot_standing = float3(0, 0, collision_height - 0.15f);
	float3 head_pivot_crouching = float3(0, 0.16f, crouch_height - 0.05f);

	float3 head_pivot () {
		return lerp(head_pivot_standing, head_pivot_crouching, crouching_progress);
	}

	// Results from previous frame physics needed before physics for character movement
	GroundedInfo grounded = {};
	bool buried = true;
	float submerged_ratio = 0;

	// Visual direction of body (only for first and third person body and arms)
	float2 body_rot = INF;
	float3 head_bob_offset = 0;
	float3 head_bob_vel = 0;
	float walking_step_bob_counter = 0;
	bool walking_step_bob_foot = 0;

	void update_body_dynamics (Input& I, float facing_ang);

	// Make head bob to react to worldspace velocity change
	void apply_head_bob_impulse (float3 delta_vel);
	void update_walking_step_bob (Input& I, float2x2 body_rotation2d,
		float step_speed, GroundedInfo& grounded);
	void update_view_dynamics (Input& I);

	
	//// Cameras
	// Closest position the third person camera can go relative to head_pivot
	float3 tps_camera_base_pos = float3(0.5f, -0.15f, 0);
	// In which direction the camera moves back if no blocks are in the way
	float3 tps_camera_dir = float3(0,-1,0);
	// How far the camera will move back
	float tps_camera_dist = 4;
	
	bool third_person = false;

	Camera fps_camera;
	Camera tps_camera;
	
	////
	SelectedBlock	selected_block;
	BlockBreakAnim	break_block;
	BlockPlaceAnim	block_place;
	PlayerInventory	inventory;

	struct VisualDynamicsParams {
		SERIALIZE(VisualDynamicsParams, bob_strength, spring_k, spring_damp,
			offset_max0, offset_max1, vel_max,
			step_length, step_length_crouch, step_head_bob_strength)
			
		float bob_strength = 2;
		float spring_k = 1;
		float spring_damp = 1;
		float3 offset_max0 = float3(0.22f, 0.22f, 0.5f);
		float3 offset_max1 = float3(0.22f, 0.22f, 0.3f);
		float vel_max = 5;

		float step_length = 1.6f;
		float step_length_crouch = 0.75f;
		float step_length_sprint = 2.1f;
		float step_head_bob_strength = .5f;
		
		void imgui () {
			if (ImGui::TreeNode("Visual Dynamics")) {
				ImGui::DragFloat("bob_strength", &bob_strength, 0.1f);
				ImGui::DragFloat("spring_k", &spring_k, 0.1f);
				ImGui::DragFloat("spring_damp", &spring_damp, 0.1f);
				ImGui::DragFloat3("offset_max0", &offset_max0.x, 0.1f);
				ImGui::DragFloat3("offset_max1", &offset_max1.x, 0.1f);
				ImGui::DragFloat("vel_max", &vel_max, 0.1f);

				ImGui::DragFloat("step_length", &step_length, 0.1f);
				ImGui::DragFloat("step_length_crouch", &step_length_crouch, 0.1f);
				ImGui::DragFloat("step_length_sprint", &step_length_sprint, 0.1f);
				ImGui::DragFloat("step_head_bob_strength", &step_head_bob_strength, 0.1f);

				ImGui::TreePop();
			}
		}
	};
	VisualDynamicsParams visual_dynamics;

	struct MovementParams {
		SERIALIZE(MovementParams, walk_speed, crouch_speed, run_speed, allow_backwards_sprint,
			walk_accel_scaled_max, walk_accel_scaled, walk_accel_boost,
			air_control_accel_base, crouch_transition_speed, swimming_force)

		float walk_speed = 3.5f;
		float crouch_speed = 1.2f;
		float run_speed = 8;
		bool allow_backwards_sprint = false;

		float walk_accel_scaled_max = 20;
		float walk_accel_scaled = 2;
		float walk_accel_boost = 50;

		float air_control_accel_base = 1;

		float crouch_transition_speed = 10;

		float swimming_force = 4;
		
		void imgui () {
			if (ImGui::TreeNode("Movement Params")) {
				ImGui::DragFloat("walk_speed", &walk_speed, 0.05f);
				ImGui::DragFloat("crouch_speed", &crouch_speed, 0.05f);
				ImGui::DragFloat("run_speed", &run_speed, 0.05f);
				ImGui::Checkbox("allow_backwards_sprint", &allow_backwards_sprint);
				ImGui::DragFloat("walk_accel_scaled_max", &walk_accel_scaled_max, 0.05f);
				ImGui::DragFloat("walk_accel_scaled", &walk_accel_scaled, 0.05f);
				ImGui::DragFloat("walk_accel_boost", &walk_accel_boost, 0.05f);

				ImGui::DragFloat("air_control_accel_base", &air_control_accel_base, 0.05f);

				ImGui::DragFloat("crouch_transition_speed", &crouch_transition_speed, 0.1f);

				ImGui::DragFloat("swimming_force", &swimming_force, 0.1f);

				ImGui::TreePop();
			}
		}
	};
	MovementParams movement_params;

	CollisionResponse collison_response;

	float3x4 head_to_world;

	bool collision_debug = false;

	ListenerMovementWindSounds move_wind_sounds;

	void _dbg_apply_forw_impulse (Input& I) {
		static float _impulse = 20;
		ImGui::DragFloat("_impulse", &_impulse, 0.1f);

		if (I.buttons[KEY_U].went_down) {
			float3 iw = look_rotation() * float3(0,0,-_impulse);
			vel += iw;
		}
	}

	void imgui (const char* name=nullptr) {
		if (!ImGui::TreeNode("Player", name)) return;

		ImGui::DragFloat3("pos", &pos.x, 0.05f);

		ImGui::DragFloat3("vel", &vel.x, 0.03f);

		float2 rot_ae_deg = to_degrees(rot_ae);
		if (ImGui::DragFloat2("rot_ae", &rot_ae_deg.x, 0.05f))
			rot_ae = to_radians(rot_ae_deg);

		break_block.imgui("break_block");
		block_place.imgui("block_place");

		ImGui::Checkbox("third_person", &third_person);

		ImGui::DragFloat3("head_pivot_standing", &head_pivot_standing.x, 0.05f);
		ImGui::DragFloat3("head_pivot_crouching", &head_pivot_crouching.x, 0.05f);
		ImGui::DragFloat3("tps_camera_base_pos", &tps_camera_base_pos.x, 0.05f);
		ImGui::DragFloat3("tps_camera_dir", &tps_camera_dir.x, 0.05f);
		ImGui::DragFloat("tps_camera_dist", &tps_camera_dist, 0.05f);

		fps_camera.imgui("fps_camera");
		tps_camera.imgui("tps_camera");

		if (ImGui::TreeNode("Collision")) {
			ImGui::DragFloat("width", &width, 0.05f);
			ImGui::TreePop();
		}
		
		visual_dynamics.imgui();
		movement_params.imgui();

		collison_response.imgui();

		ImGui::TreePop();

		ImGui::Checkbox("collision_debug", &collision_debug);

		move_wind_sounds.imgui();
	}
	

	struct PlayerInput {
		ButtonState attack_button;
		ButtonState build_button;

		float2 move_dir;
		ButtonState jump_button;
		bool sprint;

		ButtonState crouch_button;
	};
	PlayerInput get_controls (Input& I) {
		PlayerInput inp = {};

		if (g->player_controls_active() || g->creative_mode) {

			if (!inventory.is_open) {
				inp.attack_button = I.buttons[MOUSE_BUTTON_LEFT];
				inp.build_button = I.buttons[MOUSE_BUTTON_RIGHT];
			}

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
				I.set_cursor_mode(*g, inventory.is_open);
			}
		}

		if (g->player_controls_active()) {
			//// toggle camera view
			if (I.buttons[KEY_F].went_down)
				third_person = !third_person;

			inp.move_dir = 0;
			if (I.buttons[KEY_A].is_down) inp.move_dir.x -= 1;
			if (I.buttons[KEY_D].is_down) inp.move_dir.x += 1;
			if (I.buttons[KEY_S].is_down) inp.move_dir.y -= 1;
			if (I.buttons[KEY_W].is_down) inp.move_dir.y += 1;

			inp.jump_button = I.buttons[KEY_SPACE];
			inp.sprint    = I.buttons[KEY_LEFT_SHIFT].is_down;

			//// look
			Camera& cam = third_person ? tps_camera : fps_camera;

			constexpr float look_down_limit = deg(-10);
			constexpr float look_up_limit = deg(0);
			rotate_with_mouselook(I, cam.vfov, g->cam_binds, &rot_ae, -deg(180), deg(180),
				deg(-90)+look_down_limit,
				deg(90)-look_up_limit);

			inp.crouch_button = I.buttons[KEY_LEFT_CONTROL];
		}

		return inp;
	}

	void update (Input& I) {
		auto inp = get_controls(I);

		update_movement(I, inp);

		g->player_view = calc_matricies(I.window_size, *g->chunks);

		g->view = g->player_view;
		if (g->activate_flycam)
			g->view = g->flycam->update(I, I.window_size, g->cam_binds, g->player_controls_active());

		//
		auto& sel = selected_block;

		bool was_selected = sel.is_selected;
		int3 old_pos = sel.hit.pos;

		sel.is_selected = false;

		if (!g->activate_flycam || g->creative_mode) {
			BlockInteraction::aimed_block_selection(sel, g->view, break_block.reach);

			if (!was_selected || !sel.is_selected || old_pos != sel.hit.pos) {
				sel.damage = 0;
			}

			auto& item = inventory.toolbar.get_selected();
			break_block.update(I, item, sel, inp.attack_button);
			block_place.update(I, item, sel, inp.build_button);
		}
	}

	void update_movement (Input& I, PlayerInput& inp);

	float3x3 body_rotation () { return rotate3_Z(rot_ae.x); }
	float3x3 look_rotation () {
		float3x3 body_rotation = rotate3_Z(rot_ae.x);
		float3x3 head_elevation = rotate3_X(rot_ae.y);
		return body_rotation * head_elevation * rotate3_X(deg(90));
	}

	float3 calc_third_person_cam_pos (Chunks& chunks, float3x3 body_rotation, float3x3 head_elevation) {
		Ray ray;
		ray.pos = pos + body_rotation * (head_pivot() + tps_camera_base_pos);
		ray.dir = body_rotation * head_elevation * tps_camera_dir;

		float dist = tps_camera_dist;

		{
			VoxelHit hit;
			if (BlockInteraction::raycast_breakable_blocks(*g->chunks, ray, dist, hit, false)) {
				dist = max(hit.dist - 0.05f, 0.0f);
			}
		}

		return tps_camera_base_pos + tps_camera_dir * dist;
	}
	Camera_View calc_matricies (int2 const& viewport_size, Chunks& chunks) {
		float3x3 body_rotation = rotate3_Z(rot_ae.x);
		float3x3 body_rotation_inv = rotate3_Z(-rot_ae.x);

		float3x3 head_elevation = rotate3_X(rot_ae.y);
		float3x3 head_elevation_inv = rotate3_X(-rot_ae.y);

		float3 cam_offs_local = 0;
		if (third_person)
			cam_offs_local = calc_third_person_cam_pos(chunks, body_rotation, head_elevation);

		float3 pos_world = pos + head_bob_offset;

		Camera& cam = third_person ? tps_camera : fps_camera;

		auto h_pivot = head_pivot();

		float3x4 world_to_head = head_elevation_inv * translate(-h_pivot) * body_rotation_inv * translate(-pos_world);
		head_to_world = translate(pos_world) * body_rotation * translate(h_pivot) * head_elevation;

		Camera_View view;
		view.world_to_cam = rotate3_X(-deg(90)) * translate(-cam_offs_local) * world_to_head;
		view.cam_to_world = head_to_world * translate(cam_offs_local) * rotate3_X(deg(90));
		view.cam_to_clip = cam.calc_cam_to_clip(viewport_size, &view.clip_to_cam, &view.frustrum, &view.frustrum_size);
		view.clip_near = cam.clip_near;
		view.clip_far = cam.clip_far;
		view.calc_frustrum();

		return view;
	}

	float3x4 body_to_world () {
		return translate(pos) * rotate3_Z(body_rot.x) * translate(head_pivot()) * rotate3_X(body_rot.y);
	}
};

inline bool BlockInteraction::entity_in_block (int3 block_place_pos, BlockTypes::Block const& bt) {
	auto& player = *g->player;
	auto a = bt.get_local_aabb() + (float3)block_place_pos;
	auto b = player.collision_world_aabb();
	return a.overlaps(b);
}
