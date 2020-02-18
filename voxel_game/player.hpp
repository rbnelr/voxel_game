#pragma once
#include "kissmath.hpp"
#include "chunks.hpp"
#include "graphics/camera.hpp"
#include "physics.hpp"
#include "items.hpp"
#include "audio/audio.hpp"

// Global for now, the world should store this if it is not randomized
extern float3	player_spawn_point;

struct Block;
class World;

struct SelectedBlock {
	Block*		block = nullptr;
	bpos		pos;
	BlockFace	face = (BlockFace)-1; // -1 == no face

	operator bool () const {
		return block;
	}
};

class BreakBlock {
public:

	float anim_speed = 4;
	float damage = 0.25f;
	float reach = 4.5f;

	float anim_t = 0;
	bool anim_triggered;

	Sound hit_sound = Sound( "break1", 0.5f, 1 );

	void imgui (const char* name=nullptr) {
		if (!imgui_push("Fists", name)) return;

		ImGui::DragFloat("anim_speed", &anim_speed, 0.05f);
		ImGui::DragFloat("damage", &damage, 0.05f);
		ImGui::DragFloat("reach", &reach, 0.05f);

		imgui_pop();
	}
	void update (World& world, Player& player, PlayerGraphics const& graphics, SelectedBlock const& selected_block);
};

struct InventorySlot {
	// InventorySlot stores _one_ instance of an Item and a stack size which artifically 'duplicates' the item
	//  this means that only stateless Items should have a stack size above 1, or else the entire stack would share the state
	int stack_size = 0;
	Item item = {};
};

class Inventory {
public:
	bool is_open = false;

	struct Quickbar {
		InventorySlot slots[10];

		int selected = 0;

		InventorySlot& get_selected () {
			return slots[selected];
		}
		InventorySlot const& get_selected () const {
			return slots[selected];
		}

		Quickbar () {
			slots[0] = { 1, { I_WOOD_SWORD } };
			slots[1] = { 1, { I_WOOD_PICKAXE } };
			slots[2] = { 1, { I_WOOD_SHOVEL } };
			slots[3] = { 1, { (item_id)B_EARTH } };
			slots[4] = { 1, { (item_id)B_GRASS } };
			slots[5] = { 1, { (item_id)B_STONE } };
			slots[6] = { 1, { (item_id)B_TREE_LOG } };
			slots[7] = { 1, { (item_id)B_LEAVES } };
			slots[8] = { 1, { (item_id)B_WATER } };
			slots[9] = { 1, { (item_id)B_TORCH } };
		}
	};

	Quickbar quickbar;

	void update ();
};

class BlockPlace {
public:

	float repeat_speed = 5.5f;
	float anim_speed = 4;
	float reach = 4.5f;

	float anim_t = 0;

	void imgui (const char* name=nullptr) {
		if (!imgui_push("BlockPlace", name)) return;

		ImGui::DragFloat("anim_speed", &anim_speed, 0.05f);
		ImGui::DragFloat("reach", &reach, 0.05f);

		imgui_pop();
	}
	void update (World& world, Player const& player, SelectedBlock const& selected_block);
};

class Player {
public:

	Player () {
		respawn();
	}

	// Player ground position
	float3	pos;

	// Player velocity
	float3	vel;

	// Player look rotation
	float2	rot_ae =		float2(deg(0), deg(-10)); // azimuth elevation

	//// Cameras
	bool third_person = false;

	////
	BreakBlock	break_block;
	BlockPlace	block_place;
	Inventory	inventory;

	/////// These are more like settings that should possibly apply to all players, might make static later or move into PlayerSettings or something

	// Fps camera pivot for fps mode ie. where your eyes are
	//  First and third person cameras rotate around this
	float3 head_pivot = float3(0, 0, 1.6f);

	// Closest position the third person camera can go relative to head_pivot
	float3 tps_camera_base_pos = float3(0.5f, -0.15f, 0);
	// In which direction the camera moves back if no blocks are in the way
	float3 tps_camera_dir = float3(0,-1,0);
	// How far the camera will move back
	float tps_camera_dist = 4;

	Camera fps_camera;
	Camera tps_camera;

	//// Physics
	float radius = 0.4f;
	float height = 1.7f;

	float walk_speed = 5.0f;
	float run_speed = 13.0f;

	float walk_accel_base = 5;
	float walk_accel_proport = 10;

	CollisionResponse collison_response;

	float3 jump_impulse = float3(0,0, physics.jump_impulse_for_jump_height(1.2f, DEFAULT_GRAVITY)); // jump height based on the default gravity, tweaked gravity will change the jump height

	void imgui (const char* name=nullptr) {
		if (!imgui_push("Player", name)) return;

		ImGui::DragFloat3("pos", &pos.x, 0.05f);

		ImGui::DragFloat3("vel", &vel.x, 0.03f);

		float2 rot_ae_deg = to_degrees(rot_ae);
		if (ImGui::DragFloat2("rot_ae", &rot_ae_deg.x, 0.05f))
			rot_ae = to_radians(rot_ae_deg);

		break_block.imgui("break_block");
		block_place.imgui("block_place");

		ImGui::Checkbox("third_person", &third_person);

		ImGui::DragFloat3("head_pivot", &head_pivot.x, 0.05f);
		ImGui::DragFloat3("tps_camera_base_pos", &tps_camera_base_pos.x, 0.05f);
		ImGui::DragFloat3("tps_camera_dir", &tps_camera_dir.x, 0.05f);
		ImGui::DragFloat("tps_camera_dist", &tps_camera_dist, 0.05f);

		fps_camera.imgui("fps_camera");
		tps_camera.imgui("tps_camera");

		ImGui::DragFloat("radius", &radius, 0.05f);
		ImGui::DragFloat("height", &height, 0.05f);

		ImGui::DragFloat("walk_speed", &walk_speed, 0.05f);
		ImGui::DragFloat("run_speed", &run_speed, 0.05f);
		ImGui::DragFloat("walk_accel_base", &walk_accel_base, 0.05f);
		ImGui::DragFloat("walk_accel_proport", &walk_accel_proport, 0.05f);

		collison_response.imgui();

		imgui_pop();
	}

	bool calc_ground_contact (World& world, bool* stuck);

	void update_movement_controls (World& world);

	float3x4 head_to_world;

	Camera_View update_post_physics (World& world, PlayerGraphics const& graphics, bool active, SelectedBlock* highlighted_block);

	void respawn () {
		pos = player_spawn_point;
		vel = 0;
	}

	SelectedBlock calc_selected_block (World& world);
	float3 calc_third_person_cam_pos (World& world, float3x3 body_rotation, float3x3 head_elevation);
};
