#pragma once
#include "common.hpp"
#include "chunks.hpp"
#include "player.hpp"
#include "audio/audio.hpp"
#include "world_generator.hpp"

struct World {
	WorldGenerator world_gen;

	Chunks chunks;

	Player player = { float3(0.5f,0.5f,34) };

	Sound break_sound = { "dig1", 1.2f, 0.8f };

	World (WorldGenerator gen): world_gen{gen} {

	}

	~World () {
		// shutdown threads so we don't crash when destructing voxels.svo.allocator
		background_threadpool.flush();
	}

	void raycast_breakable_blocks (SelectedBlock& block, Ray const& ray, float max_dist, bool hit_at_max_dist=false);

	void apply_damage (SelectedBlock& block, Item& item, bool creative_mode);
	bool try_place_block (int3 pos, block_id id);
};
