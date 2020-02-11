#pragma once
#include "kissmath.hpp"
#include "util/random.hpp"
#include "chunks.hpp"
#include "player.hpp"
#include "audio/audio.hpp"
#include <string>

inline uint64_t get_seed (std::string_view str) {
	str = trim(str);

	if (str.size() == 0) // "" -> random seed
		return std::hash<uint64_t>()(random.uniform_u64());

	return std::hash<std::string_view>()(str);
}

struct WorldGenerator;

class World {

public:
	const std::string seed_str;
	const uint64_t seed;

	Player player;

	Chunks chunks;

	Sound break_sound = { "dig1", 1.2f, 0.8f };

	World (): seed_str{""}, seed{get_seed(this->seed_str)} {

	}
	World (std::string seed_str): seed_str{std::move(seed_str)}, seed{get_seed(this->seed_str)} {

	}

	void imgui (bool open) {
		if (open) ImGui::Text("seed: \"%s\" (0x%016p)", seed_str.c_str(), seed);
	}

	void update (WorldGenerator& world_gen);

	//// Raycasting into the world

	SelectedBlock raycast_solid_blocks (Ray ray, float max_dist, float* hit_dist=nullptr);

	void apply_damage (SelectedBlock const& block, float damage);
	bool try_place_block (bpos pos, block_type bt);
};
