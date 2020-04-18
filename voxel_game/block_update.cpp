#include "block_update.hpp"
#include "chunks.hpp"
#include "util/random.hpp"

bool BlockUpdate::update_block (Chunks& chunks, Chunk& chunk, Block& b, bpos pos_world) {
	auto& above = chunks.query_block(pos_world +bpos(0,0,+1));

	bool changed = false;

	if (/*block_props[b->type].does_autoheal &&*/ b.hp < 255) {
		b.hp += min((uint8)ceili(1.0f/25 / block_update_frequency * 255), 255u - b.hp);
		changed = true;
	}
	if (b.id == B_GRASS && !blocks.grass_can_live_below(above.id)) {
		if (grass_die_prob > random.uniform()) {
			b.id = B_EARTH;
			b.hp = 255;
			changed = true;
		}
	}
	if (b.id == B_EARTH && blocks.grass_can_live_below(above.id)) {
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
			if (	 chunks.query_block(pos_world +bpos(v,+1)).id == B_GRASS) prob += grass_grow_side_prob * grass_grow_step_down_multipiler;
			else if (chunks.query_block(pos_world +bpos(v, 0)).id == B_GRASS) prob += grass_grow_side_prob;
			else if (chunks.query_block(pos_world +bpos(v,-1)).id == B_GRASS) prob += grass_grow_side_prob * grass_grow_step_up_multipiler;
		}

		for (bpos2 v : diagonals) {
			if (	 chunks.query_block(pos_world +bpos(v,+1)).id == B_GRASS) prob += grass_grow_diagonal_prob * grass_grow_step_down_multipiler;
			else if (chunks.query_block(pos_world +bpos(v, 0)).id == B_GRASS) prob += grass_grow_diagonal_prob;
			else if (chunks.query_block(pos_world +bpos(v,-1)).id == B_GRASS) prob += grass_grow_diagonal_prob * grass_grow_step_up_multipiler;
		}

		if (prob > random.uniform()) {
			b.id = B_GRASS;
			b.hp = 255;

			changed = true;
		}
	}

	return changed;
};

#if 0
static_assert(CHUNK_BLOCK_COUNT == (1 << 16), "");

uint16_t block_pattern (uint16_t i) {
	// reverse bits to turn normal x y z block iteration into a somewhat distributed pattern
	i = ((i & 0x00ff) << 8) | ((i & 0xff00) >> 8);
	i = ((i & 0x0f0f) << 4) | ((i & 0xf0f0) >> 4);
	i = ((i & 0x3333) << 2) | ((i & 0xcccc) >> 2);
	i = ((i & 0x5555) << 1) | ((i & 0xaaaa) >> 1);
	return i;
}
#elif 0
uint32_t block_pattern (uint32_t i) {
	return i;
}
#else
static_assert(CHUNK_BLOCK_COUNT == (1 << 18), "");

__declspec(noinline) uint32_t block_pattern (uint32_t i) {
	//// reverse bits to turn normal x y z block iteration into a somewhat distributed pattern
	// 32 bit reverse
	i = ((i & 0x0000ffff) << 16) | ((i & 0xffff0000) >> 16);
	i = ((i & 0x00ff00ff) <<  8) | ((i & 0xff00ff00) >>  8);
	i = ((i & 0x0f0f0f0f) <<  4) | ((i & 0xf0f0f0f0) >>  4);
	i = ((i & 0x33333333) <<  2) | ((i & 0xcccccccc) >>  2);
	i = ((i & 0x55555555) <<  1) | ((i & 0xaaaaaaaa) >>  1);
	i >>= 14; // shift away unwanted bits to get a 18 bit reverse
	return i;
}
#endif

void BlockUpdate::update_blocks (Chunks& chunks) {
	bpos_t blocks_to_update = (bpos_t)ceil((float)CHUNK_BLOCK_COUNT * block_update_frequency * input.dt);

	for (Chunk& chunk : chunks.chunks) {
		if (chunk.active) {
			for (bpos_t i=0; i<blocks_to_update; ++i) {
				uint32_t indx = (uint32_t)((cur_chunk_update_block_i +i) % CHUNK_BLOCK_COUNT);
				indx = block_pattern(indx);

				// reconstruct 3d index from flat index
				bpos bp;
				bp.z =  indx / (CHUNK_DIM * CHUNK_DIM);
				bp.y = (indx % (CHUNK_DIM * CHUNK_DIM)) / CHUNK_DIM;
				bp.x = (indx % (CHUNK_DIM * CHUNK_DIM)) % CHUNK_DIM;

				// get block with flat index
				Block b = chunk.get_block(bp);

				if (update_block(chunks, chunk, b, bp + chunk.chunk_pos_world())) {
					chunk.set_block(chunks, bp, b);
				}
			}
		}
	}

	cur_chunk_update_block_i = (cur_chunk_update_block_i +blocks_to_update) % CHUNK_BLOCK_COUNT;
}
