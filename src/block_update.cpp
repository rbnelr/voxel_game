#include "common.hpp"
#include "block_update.hpp"
#include "chunks.hpp"

bool BlockUpdate::update_block (Chunks& chunks, Chunk& chunk, Block& b, int3 pos_world) {
	bool changed = false;
#if 0
	auto above = chunks.query_block(pos_world +int3(0,0,+1));


	if (/*block_props[b->type].does_autoheal &&*/ b.hp < 255) {
		b.hp += min((uint8_t)ceili(1.0f/25 / effective_frequency * 255), 255u - b.hp);
		changed = true;
	}

	if (b.id == B_GRASS && !grass_can_live_below(above.id)) {
		if (grass_die_prob > random.uniform()) {
			b.id = B_EARTH;
			b.hp = 255;
			changed = true;
		}
	}
	if (b.id == B_EARTH && grass_can_live_below(above.id)) {
		float prob = 0;

		int2 sides[4] = {
			int2(-1,0),
			int2(+1,0),
			int2(0,-1),
			int2(0,+1),
		};
		int2 diagonals[4] = {
			int2(-1,-1),
			int2(+1,-1),
			int2(-1,+1),
			int2(+1,+1),
		};

		for (int2 v : sides) {
			if (	 chunks.query_block(pos_world +int3(v,+1)).id == B_GRASS) prob += grass_grow_side_prob * grass_grow_step_down_multipiler;
			else if (chunks.query_block(pos_world +int3(v, 0)).id == B_GRASS) prob += grass_grow_side_prob;
			else if (chunks.query_block(pos_world +int3(v,-1)).id == B_GRASS) prob += grass_grow_side_prob * grass_grow_step_up_multipiler;
		}

		for (int2 v : diagonals) {
			if (	 chunks.query_block(pos_world +int3(v,+1)).id == B_GRASS) prob += grass_grow_diagonal_prob * grass_grow_step_down_multipiler;
			else if (chunks.query_block(pos_world +int3(v, 0)).id == B_GRASS) prob += grass_grow_diagonal_prob;
			else if (chunks.query_block(pos_world +int3(v,-1)).id == B_GRASS) prob += grass_grow_diagonal_prob * grass_grow_step_up_multipiler;
		}

		if (prob > random.uniform()) {
			b.id = B_GRASS;
			b.hp = 255;

			changed = true;
		}
	}

#endif
	return changed;
};

#if 0
static_assert(CHUNK_BLOCK_COUNT == (1 << 15), "");

uint16_t block_pattern (uint16_t i) {
	// reverse bits to turn normal x y z block iteration into a somewhat distributed pattern
	;
	i = ((i & 0x0f0f) << 4) | ((i & 0xf0f0) >> 4);
	i = ((i & 0x3333) << 2) | ((i & 0xcccc) >> 2);
	i = ((i & 0x5555) << 1) | ((i & 0xaaaa) >> 1);
	i >>= 1;
	return i;
}

void BlockUpdate::update_blocks (Input& I, Chunks& chunks) {
	ZoneScoped;

	float tmp = ceil((float)CHUNK_BLOCK_COUNT * block_update_fraction);

	size_t blocks_to_update = (size_t)tmp;
	float rounded_fraction = tmp / (float)CHUNK_BLOCK_COUNT;

	effective_frequency = rounded_fraction / I.dt;

	recalc_probs();

	for (chunk_id id=0; id<chunks.max_id; ++id) {
		if ((chunks[id].flags & Chunk::LOADED) == 0) continue;
		
		//ZoneScopedN("update_blocks chunk");
		auto& chunk = chunks[id];

		for (size_t i=0; i<blocks_to_update; ++i) {
			size_t indx = cur_chunk_update_block_i++;
			if (cur_chunk_update_block_i == CHUNK_BLOCK_COUNT)
				cur_chunk_update_block_i = 0;

			//indx = block_pattern(indx);

			// reconstruct 3d index from flat index
			int3 bp;
			bp.z = (int)(  indx / (CHUNK_SIZE * CHUNK_SIZE)               );
			bp.y = (int)( (indx % (CHUNK_SIZE * CHUNK_SIZE)) / CHUNK_SIZE );
			bp.x = (int)( (indx % (CHUNK_SIZE * CHUNK_SIZE)) % CHUNK_SIZE );

			// get block with flat index
			Block b = chunk.get_block(bp);

			if (update_block(chunks, chunk, b, bp + chunk.pos * CHUNK_SIZE)) {
				chunk.set_block(chunks, bp, b);
			}
		}
	}
}
#endif

void BlockUpdate::update_blocks (Input& I, Chunks& chunks) {

}
