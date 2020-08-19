#pragma once
#include "stdafx.hpp"
#include "voxel_backend.hpp"

class BlockUpdate {

public:
	// how many blocks (fraction of total blocks) get updated per frame
	float block_update_fraction = 0.0002f;
	int cur_chunk_update_block_i = 0;

	float grass_die_mtth = 5; // seconds
	float grass_grow_min_mtth = 10; // seconds

	float grass_grow_diagonal_multipiler = 0.5f;
	float grass_grow_step_down_multipiler = 0.75f;
	float grass_grow_step_up_multipiler = 0.6f;

	//
	float grass_die_prob;
	float grass_grow_max_prob;

	// grass_grow_max_prob = 4*grass_grow_side_prob +4*grass_grow_diagonal_prob; grass_grow_diagonal_prob = grass_grow_side_prob * grass_grow_diagonal_multipiler
	float grass_grow_side_prob;
	float grass_grow_diagonal_prob;

	//
	float effective_frequency;

	// mean time to happen to a probalility that causes that avg time to happen when randomly rolled at freq
	float mtth_to_prob (float mtth) {
		return 1 -pow(EULER, -0.693147f / (mtth * effective_frequency));
	}

	void recalc_probs () {
		grass_die_prob = mtth_to_prob(grass_die_mtth);
		grass_grow_max_prob = mtth_to_prob(grass_grow_min_mtth);

		grass_grow_side_prob = grass_grow_max_prob / (4 * (1 +grass_grow_diagonal_multipiler));
		grass_grow_diagonal_prob = grass_grow_side_prob * grass_grow_diagonal_multipiler;
	}

	void imgui () {
		if (!imgui_push("BlockUpdate")) return;

		ImGui::DragFloat("block_update_fraction", &block_update_fraction, 0.00005f, 0.00000001f, 1);

		ImGui::DragFloat("grass_die_mtth", &grass_die_mtth, 0.05f);

		ImGui::DragFloat("grass_grow_min_mtth", &grass_grow_min_mtth, 0.05f);

		ImGui::DragFloat("grass_grow_diagonal_multipiler", &grass_grow_diagonal_multipiler, 0.05f);
		ImGui::DragFloat("grass_grow_step_down_multipiler", &grass_grow_step_down_multipiler, 0.05f);
		ImGui::DragFloat("grass_grow_step_up_multipiler", &grass_grow_step_up_multipiler, 0.05f);

		imgui_pop();
	}

	//bool update_block (Chunks& chunks, Chunk& chunk, Block& b, int3 pos_world);
	//void update_blocks (Chunks& chunks);
};

