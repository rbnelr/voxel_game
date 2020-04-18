#pragma once
#include "chunks.hpp"

class BlockUpdate {

public:
	//float block_update_frequency = 1.0f;
	float block_update_frequency = 1.0f / 25;
	bpos_t cur_chunk_update_block_i = 0;

	float grass_die_mtth = 5; // seconds
	float grass_die_prob = mtth_to_prob(grass_die_mtth);

	float grass_grow_min_mtth = 1; // seconds
	float grass_grow_max_prob = mtth_to_prob(grass_grow_min_mtth);

	float grass_grow_diagonal_multipiler = 0.5f;
	float grass_grow_step_down_multipiler = 0.75f;
	float grass_grow_step_up_multipiler = 0.6f;

	// grass_grow_max_prob = 4*grass_grow_side_prob +4*grass_grow_diagonal_prob; grass_grow_diagonal_prob = grass_grow_side_prob * grass_grow_diagonal_multipiler
	float grass_grow_side_prob = grass_grow_max_prob / (4 * (1 +grass_grow_diagonal_multipiler));
	float grass_grow_diagonal_prob = grass_grow_side_prob * grass_grow_diagonal_multipiler;


	// mean time to happen to a probalility that causes that avg time to happen when randomly rolled at block_update_frequency
	float mtth_to_prob (float mtth) {
		return 1 -pow(EULER, -0.693147f / (mtth * block_update_frequency));
	}

	void imgui () {
		if (!imgui_push("BlockUpdate")) return;

		ImGui::DragFloat("block_update_frequency", &block_update_frequency, 0.05f);

		ImGui::DragFloat("grass_die_mtth", &grass_die_mtth, 0.05f);
		grass_die_prob = mtth_to_prob(grass_die_mtth);

		ImGui::DragFloat("grass_grow_min_mtth", &grass_grow_min_mtth, 0.05f);
		grass_grow_max_prob = mtth_to_prob(grass_grow_min_mtth);

		ImGui::DragFloat("grass_grow_diagonal_multipiler", &grass_grow_diagonal_multipiler, 0.05f);
		ImGui::DragFloat("grass_grow_step_down_multipiler", &grass_grow_step_down_multipiler, 0.05f);
		ImGui::DragFloat("grass_grow_step_up_multipiler", &grass_grow_step_up_multipiler, 0.05f);

		float grass_grow_side_prob = grass_grow_max_prob / (4 * (1 +grass_grow_diagonal_multipiler));
		float grass_grow_diagonal_prob = grass_grow_side_prob * grass_grow_diagonal_multipiler;

		imgui_pop();
	}

	bool update_block (Chunks& chunks, Chunk& chunk, Block& b, bpos pos_world);
	void update_blocks (Chunks& chunks);
};

