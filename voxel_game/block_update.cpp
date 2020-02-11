#include "block_update.hpp"
#include "chunks.hpp"
#include "util/random.hpp"

void BlockUpdate::update_block (Chunks& chunks, Chunk& chunk, Block* b, bpos pos_world) {
	Block* above = chunks.query_block(pos_world +bpos(0,0,+1));

	if (/*block_props[b->type].does_autoheal &&*/ b->hp < 255) {
		b->hp += min((uint8)ceili(1.0f/5 / block_update_frequency * 255), 255u - b->hp);

		chunk.block_only_texture_changed(pos_world);
	}
	if (b->type == BT_GRASS && !(above->type == BT_AIR || above->type == BT_OUT_OF_BOUNDS)) {
		if (grass_die_prob > random.uniform()) {
			b->type = BT_EARTH;
			b->hp = 255;
			chunk.block_only_texture_changed(pos_world);
		}
	}
	if (b->type == BT_EARTH && (above->type == BT_AIR || above->type == BT_OUT_OF_BOUNDS)) {
		float prob = 0;

		bpos2 sides[4] = {
			bpos2(-1,0),
			bpos2(+1,0),
			bpos2(0,-1),
			bpos2(0,+1),
		};
		bpos2 diagonals[4] = {
			bpos2(-1,-1),
			bpos2(+1,-1),
			bpos2(-1,+1),
			bpos2(+1,+1),
		};

		for (bpos2 v : sides) {
			if (	 chunks.query_block(pos_world +bpos(v,+1))->type == BT_GRASS) prob += grass_grow_side_prob * grass_grow_step_down_multipiler;
			else if (chunks.query_block(pos_world +bpos(v, 0))->type == BT_GRASS) prob += grass_grow_side_prob;
			else if (chunks.query_block(pos_world +bpos(v,-1))->type == BT_GRASS) prob += grass_grow_side_prob * grass_grow_step_up_multipiler;
		}

		for (bpos2 v : diagonals) {
			if (	 chunks.query_block(pos_world +bpos(v,+1))->type == BT_GRASS) prob += grass_grow_diagonal_prob * grass_grow_step_down_multipiler;
			else if (chunks.query_block(pos_world +bpos(v, 0))->type == BT_GRASS) prob += grass_grow_diagonal_prob;
			else if (chunks.query_block(pos_world +bpos(v,-1))->type == BT_GRASS) prob += grass_grow_diagonal_prob * grass_grow_step_up_multipiler;
		}

		if (prob > random.uniform()) {
			b->type = BT_GRASS;
			b->hp = 255;
			chunk.block_only_texture_changed(pos_world);
		}
	}
};

constexpr bpos_t CHUNK_BLOCK_COUNT = CHUNK_DIM_X * CHUNK_DIM_Y * CHUNK_DIM_Z;

static_assert(CHUNK_BLOCK_COUNT == (1 << 16), "");

uint16_t block_pattern (uint16_t i) {
	// reverse bits to turn normal x y z block iteration into a somewhat distributed pattern
	i = ((i & 0x00ff) << 8) | ((i & 0xff00) >> 8);
	i = ((i & 0x0f0f) << 4) | ((i & 0xf0f0) >> 4);
	i = ((i & 0x3333) << 2) | ((i & 0xcccc) >> 2);
	i = ((i & 0x5555) << 1) | ((i & 0xaaaa) >> 1);
	return i;
}

void BlockUpdate::update_blocks (Chunks& chunks) {
	bpos_t blocks_to_update = (bpos_t)ceil((float)CHUNK_BLOCK_COUNT * block_update_frequency * input.dt);

	for (Chunk& chunk : chunks) {

		for (bpos_t i=0; i<blocks_to_update; ++i) {
			uint16_t indx = (uint16_t)((cur_chunk_update_block_i +i) % CHUNK_BLOCK_COUNT);
			indx = block_pattern(indx);

			// get block with flat index
			Block* b = chunk.get_block_flat(indx);

			// reconstruct 3d index from flat index
			bpos bp;
			bp.z =  indx / (CHUNK_DIM.y * CHUNK_DIM.x);
			bp.y = (indx % (CHUNK_DIM.y * CHUNK_DIM.x)) / CHUNK_DIM.y;
			bp.x = (indx % (CHUNK_DIM.y * CHUNK_DIM.x)) % CHUNK_DIM.y;
			bp += chunk.chunk_pos_world();

			update_block(chunks, chunk, b, bp);
		}
	}

	cur_chunk_update_block_i = (cur_chunk_update_block_i +blocks_to_update) % CHUNK_BLOCK_COUNT;
}
