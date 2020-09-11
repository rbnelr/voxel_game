#include "stdafx.hpp"
#include "block_update.hpp"
#include "voxel_system.hpp"

//bool BlockUpdate::update_block (Chunks& chunks, Chunk& chunk, Block& b, int3 pos_world) {
//	auto above = chunks.query_block(pos_world +int3(0,0,+1));
//
//	bool changed = false;
//
//	if (/*block_props[b->type].does_autoheal &&*/ b.hp < 255) {
//		b.hp += min((uint8)ceili(1.0f/25 / effective_frequency * 255), 255u - b.hp);
//		changed = true;
//	}
//	if (b.id == B_GRASS && !blocks.grass_can_live_below(above.id)) {
//		if (grass_die_prob > random.uniform()) {
//			b.id = B_EARTH;
//			b.hp = 255;
//			changed = true;
//		}
//	}
//	if (b.id == B_EARTH && blocks.grass_can_live_below(above.id)) {
//		float prob = 0;
//
//		int2 sides[4] = {
//			int2(-1,0),
//			int2(+1,0),
//			int2(0,-1),
//			int2(0,+1),
//		};
//		int2 diagonals[4] = {
//			int2(-1,-1),
//			int2(+1,-1),
//			int2(-1,+1),
//			int2(+1,+1),
//		};
//
//		for (int2 v : sides) {
//			if (	 chunks.query_block(pos_world +int3(v,+1)).id == B_GRASS) prob += grass_grow_side_prob * grass_grow_step_down_multipiler;
//			else if (chunks.query_block(pos_world +int3(v, 0)).id == B_GRASS) prob += grass_grow_side_prob;
//			else if (chunks.query_block(pos_world +int3(v,-1)).id == B_GRASS) prob += grass_grow_side_prob * grass_grow_step_up_multipiler;
//		}
//
//		for (int2 v : diagonals) {
//			if (	 chunks.query_block(pos_world +int3(v,+1)).id == B_GRASS) prob += grass_grow_diagonal_prob * grass_grow_step_down_multipiler;
//			else if (chunks.query_block(pos_world +int3(v, 0)).id == B_GRASS) prob += grass_grow_diagonal_prob;
//			else if (chunks.query_block(pos_world +int3(v,-1)).id == B_GRASS) prob += grass_grow_diagonal_prob * grass_grow_step_up_multipiler;
//		}
//
//		if (prob > random.uniform()) {
//			b.id = B_GRASS;
//			b.hp = 255;
//
//			changed = true;
//		}
//	}
//
//	return changed;
//};
//
//static_assert(CHUNK_BLOCK_COUNT == (1 << 15), "");

/*
	
n = 8

def bit_reverse(i):
	#i = ((i & 0x00ff) << 8) | ((i & 0xff00) >> 8)
	i = ((i & 0x0f0f) << 4) | ((i & 0xf0f0) >> 4)
	i = ((i & 0x3333) << 2) | ((i & 0xcccc) >> 2)
	i = ((i & 0x5555) << 1) | ((i & 0xaaaa) >> 1)
	i = i >> 2
	return i
	
def bit_interleave(a, b):
    a = (a | (a << 8)) & 0x00FF00FF;
    a = (a | (a << 4)) & 0x0F0F0F0F;
    a = (a | (a << 2)) & 0x33333333;
    a = (a | (a << 1)) & 0x55555555;
    
    b = (b | (b << 8)) & 0x00FF00FF;
    b = (b | (b << 4)) & 0x0F0F0F0F;
    b = (b | (b << 2)) & 0x33333333;
    b = (b | (b << 1)) & 0x55555555;
    
    return a | (b << 1)
	
for y in range(n):
    for x in range(n):
        val = bit_reverse(bit_interleave(x ^ y, y))
        #val = val ^ 0b10110111
        #val = val / 256
        print('{0:7.3f}, '.format(val), end='')
    print("")

*/

uint16_t block_pattern (uint16_t i) {
	// reverse bits to turn normal x y z block iteration into a somewhat distributed pattern
	i = ((i & 0x00ff) << 8) | ((i & 0xff00) >> 8);
	i = ((i & 0x0f0f) << 4) | ((i & 0xf0f0) >> 4);
	i = ((i & 0x3333) << 2) | ((i & 0xcccc) >> 2);
	i = ((i & 0x5555) << 1) | ((i & 0xaaaa) >> 1);
	i >>= 1;
	return i;
}

//void BlockUpdate::update_blocks (Chunks& chunks) {
//	ZoneScopedN("BlockUpdate::update_blocks");
//
//	float tmp = ceil((float)CHUNK_BLOCK_COUNT * block_update_fraction);
//
//	int blocks_to_update = (int)tmp;
//	float rounded_fraction = tmp / (float)CHUNK_BLOCK_COUNT;
//
//	effective_frequency = rounded_fraction / input.dt;
//
//	recalc_probs();
//	
//	for (Chunk& chunk : chunks.chunks) {
//		if (chunk.active) {
//			for (int i=0; i<blocks_to_update; ++i) {
//				uint32_t indx = cur_chunk_update_block_i++;
//				if (cur_chunk_update_block_i == CHUNK_BLOCK_COUNT)
//					cur_chunk_update_block_i = 0;
//
//				//indx = block_pattern(indx);
//
//				// reconstruct 3d index from flat index
//				int3 bp;
//				bp.z =  indx / (CHUNK_DIM * CHUNK_DIM);
//				bp.y = (indx % (CHUNK_DIM * CHUNK_DIM)) / CHUNK_DIM;
//				bp.x = (indx % (CHUNK_DIM * CHUNK_DIM)) % CHUNK_DIM;
//
//				// get block with flat index
//				Block b = chunk.get_block(bp);
//
//				if (update_block(chunks, chunk, b, bp + chunk.chunk_pos_world())) {
//					chunk.set_block(chunks, bp, b);
//				}
//			}
//		}
//	}
//}
