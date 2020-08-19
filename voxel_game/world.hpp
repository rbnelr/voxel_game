#pragma once
#include "stdafx.hpp"
#include "voxel_backend.hpp"
#include "player.hpp"
#include "audio/audio.hpp"
#include "world_generator.hpp"
#include "time_of_day.hpp"

class World {

public:
	WorldGenerator world_gen;
	Voxels voxels;

	Player player;

	TimeOfDay time_of_day;

	Sound break_sound = { "dig1", 1.2f, 0.8f };

	World (WorldGenerator gen): world_gen{gen} {

	}

	void imgui () {
		voxels.imgui();
		time_of_day.imgui();
	}

	//// Raycasting into the world

	SelectedBlock raycast_breakable_blocks (Ray ray, float max_dist, bool hit_at_max_dist=false, float* hit_dist=nullptr);

	void apply_damage (SelectedBlock& block, Item& item, bool creative_mode);
	bool try_place_block (int3 pos, block_id id);
};
