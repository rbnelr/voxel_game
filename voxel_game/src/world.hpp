#pragma once
#include "stdafx.hpp"
#include "voxel_system.hpp"
#include "player.hpp"
#include "audio/audio.hpp"
#include "worldgen.hpp"
#include "time_of_day.hpp"

struct World {
	WorldGenerator world_gen;
	Voxels voxels;

	Player player;

	TimeOfDay time_of_day;

	Sound break_sound = { "dig1", 1.2f, 0.8f };

	World (WorldGenerator gen): world_gen{gen} {
		
	}

	~World () { // this gets called before our members are destructed, so threads won't access freed memory from dtored svo
		// shutdown threads so we don't crash when destructing voxels.svo.allocator
		background_threadpool.flush();
		parallelism_threadpool.flush();
	}


	void imgui () {
		voxels.imgui();
		time_of_day.imgui();
	}

	//// Raycasting into the world

	void raycast_breakable_blocks (SelectedBlock& block, Ray ray, float max_dist, bool hit_at_max_dist=false, float* hit_dist=nullptr);

	void apply_damage (SelectedBlock& block, Item& item, bool creative_mode);
	bool try_place_block (int3 pos, block_id id);
};