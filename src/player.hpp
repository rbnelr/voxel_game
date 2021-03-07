#pragma once
#include "common.hpp"
#include "chunks.hpp"
#include "engine/camera.hpp"
#include "physics.hpp"
#include "items.hpp"
#include "audio/audio.hpp"

struct Block;
struct Game;

struct VoxelHit {
	int3		pos;
	block_id	bid;
	BlockFace	face; // -1 == no face
};
struct SelectedBlock {
	VoxelHit	hit;
	bool		is_selected = false;

	float		damage = 0; // damage is accumulated from prev frame if was_selected then and is_selected now and if pos is the same

	operator bool () const {
		return is_selected;
	}
};

struct BreakBlock {

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
	void update (Input& I, Game& game, Player& player);
};

struct BlockPlace {

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
	void update (Input& I, Game& game, Player& player);
};

struct Inventory {
	bool is_open = false;

	struct Backpack {
		Item slots[10][10];

		Backpack () {
			for (int bid=1; bid<g_assets.block_types.count(); ++bid) {
				int i = bid-1;
				if (i < 10*10) {
					slots[i/10][i%10] = Item::make_block((item_id)bid, 1);
				}
			}
		}
	};

	struct Toolbar {
		Item slots[10];

		int selected = 0;

		Item& get_selected () {
			return slots[selected];
		}

		Toolbar () {
			slots[0] = Item::make_item( I_WOOD_SWORD   );
			slots[1] = Item::make_item( I_WOOD_PICKAXE );
			slots[2] = Item::make_item( I_WOOD_SHOVEL  );
			slots[3] = Item::make_block( (item_id)g_assets.block_types.map_id("earth")    , 1 );
			slots[4] = Item::make_block( (item_id)g_assets.block_types.map_id("grass")    , 1 );
			slots[5] = Item::make_block( (item_id)g_assets.block_types.map_id("stone")    , 1 );
			slots[6] = Item::make_block( (item_id)g_assets.block_types.map_id("tree_log") , 1 );
			slots[7] = Item::make_block( (item_id)g_assets.block_types.map_id("leaves")   , 1 );
			slots[8] = Item::make_block( (item_id)g_assets.block_types.map_id("water")    , 1 );
			slots[9] = Item::make_block( (item_id)g_assets.block_types.map_id("glass")    , 1 );
		}
	};

	Backpack	backpack;
	Toolbar		toolbar;
	Item		hand; // drag&drop picks up items to the 'hand'

	Inventory () {
		hand = {};
	}
};

struct Player {
	SERIALIZE(Player, pos, vel, rot_ae, third_person)

	Player (float3 pos): pos{pos} {}

	// Player ground position
	float3	pos;

	// Player velocity
	float3	vel = 0;

	// Player look rotation
	float2	rot_ae =		float2(deg(0), deg(-10)); // azimuth elevation

	//// Cameras
	bool third_person = false;

	////
	BreakBlock		break_block;
	BlockPlace		block_place;
	Inventory		inventory;
	SelectedBlock	selected_block;

	struct PlayerInput {
		bool attack, attack_held;
		bool build_held;

		float2 move_dir;
		bool jump_held;
		bool sprint;
	};
	PlayerInput inp;

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

	float3x4 head_to_world;

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


	void update_controls (Input& I, Game& game);
	void update (Input& I, Game& game);

	void update_movement (Input& I, Game& game);

	Camera_View calc_matricies (Input& I, Chunks& chunks);

	bool calc_ground_contact (Chunks& chunks, bool* stuck);

	void calc_selected_block (SelectedBlock& block, Game& game, Camera_View& view, float reach);
	float3 calc_third_person_cam_pos (Chunks& chunks, float3x3 body_rotation, float3x3 head_elevation);

};
