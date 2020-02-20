#pragma once
#include "kissmath.hpp"
#include "chunks.hpp"
#include "player.hpp"
#include "audio/audio.hpp"
#include "world_generator.hpp"

class World {

public:
	const WorldGenerator world_gen;

	Player player;

	Chunks chunks;

	Sound break_sound = { "dig1", 1.2f, 0.8f };

	World (WorldGenerator const gen): world_gen{gen} {

	}

	//// Raycasting into the world

	SelectedBlock raycast_breakable_blocks (Ray ray, float max_dist, float* hit_dist=nullptr);

	void apply_damage (SelectedBlock const& block, Item& item);
	bool try_place_block (bpos pos, block_id id);
};
