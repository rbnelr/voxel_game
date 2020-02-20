#include "voxel_light.hpp"
#include "util/timer.hpp"
#include <queue>

struct LitBlock {
	bpos bp;
	uint8 light_level;
};

bool operator< (LitBlock const& l, LitBlock const& r) {
	return l.light_level < r.light_level;
}

std::vector<bpos> dbg_block_light_add_list;
std::vector<bpos> dbg_block_light_remove_list;

void light_propagate (Chunks& chunks, std::priority_queue<LitBlock>& q) {
	auto neighbour = [&] (LitBlock const& n, int3 pos) {
		Chunk* chunk;
		bpos pos_in_chunk;
		auto blk = chunks.query_block(pos, &chunk, &pos_in_chunk);
		uint8 l = (uint8)max((int)n.light_level - (int)BLOCK_PROPS[blk.id].absorb_light_level - 1, 0);

		if (l > blk.light_level) {
			blk.light_level = l;

			q.push({ pos, blk.light_level });

			if (chunk)
				chunk->_set_block_no_light_update(chunks, pos_in_chunk, blk);
		}
	};

	while (!q.empty()) {
		auto n = q.top();
		q.pop();
		dbg_block_light_add_list.push_back(n.bp);

		neighbour(n, n.bp - int3(1,0,0));
		neighbour(n, n.bp + int3(1,0,0));
		neighbour(n, n.bp - int3(0,1,0));
		neighbour(n, n.bp + int3(0,1,0));
		neighbour(n, n.bp - int3(0,0,1));
		neighbour(n, n.bp + int3(0,0,1));
	}
}

void update_block_light_add (Chunks& chunks, bpos bp, uint8 new_light_level) {
	std::priority_queue<LitBlock> q;

	q.push({ bp, new_light_level });

	light_propagate(chunks, q);
}

void update_block_light_remove (Chunks& chunks, bpos bp, uint8 old_light_level) {
	std::priority_queue<LitBlock> remove_q;
	std::priority_queue<LitBlock> add_q;
	
	remove_q.push({ bp, old_light_level });
	
	auto neighbour = [&] (LitBlock const& n, int3 pos) {
		Chunk* chunk;
		bpos pos_in_chunk;
		auto blk = chunks.query_block(pos, &chunk, &pos_in_chunk);
	
		if (blk.light_level > 0) {
			uint8 l = (uint8)max((int)n.light_level - (int)BLOCK_PROPS[blk.id].absorb_light_level - 1, 0);
			if (blk.light_level == l) {
				// block was lit by our source block, zero it and repropagate light into it from other light sources
				remove_q.push({ pos, blk.light_level });

				blk.light_level = 0;

				if (chunk)
					chunk->_set_block_no_light_update(chunks, pos_in_chunk, blk);
			} else {
				// block is too bright to be lit by us -> this is a source that can light up the blocks we are zeroing
				// add to a seperate queue and repropagate light in a second pass
				add_q.push({ pos, blk.light_level });
			}
		}
	};

	while (!remove_q.empty()) {
		auto n = remove_q.top();
		remove_q.pop();
		dbg_block_light_remove_list.push_back(n.bp);
	
		neighbour(n, n.bp - int3(1,0,0));
		neighbour(n, n.bp + int3(1,0,0));
		neighbour(n, n.bp - int3(0,1,0));
		neighbour(n, n.bp + int3(0,1,0));
		neighbour(n, n.bp - int3(0,0,1));
		neighbour(n, n.bp + int3(0,0,1));
	}

	light_propagate(chunks, add_q);
}

uint8 calc_block_light_level (Chunk* chunk, bpos pos_in_chunk, Block new_block) {
	// The light levels of the neighbours of this block are either the result of:
	//  A: the light coming in at that neighbour (still valid after placing this block)
	//  B: the light was the result of the light from A (this might not be valid anymore)
	// Since the B-light_level < A-light_level, the max light level of all the neighbours is always the correct light level to light this block with
	
	uint8 l = new_block.light_level; // glow level
	
	uint8 a = chunk->get_block(pos_in_chunk - int3(1,0,0)).light_level;
	uint8 b = chunk->get_block(pos_in_chunk + int3(1,0,0)).light_level;
	uint8 c = chunk->get_block(pos_in_chunk - int3(0,1,0)).light_level;
	uint8 d = chunk->get_block(pos_in_chunk + int3(0,1,0)).light_level;
	uint8 e = chunk->get_block(pos_in_chunk - int3(0,0,1)).light_level;
	uint8 f = chunk->get_block(pos_in_chunk + int3(0,0,1)).light_level;
	
	uint8 neighbour_light = max(
		max(max(a,b), max(c,d)),
		max(e,f)
	);
	
	return (uint8)max((int)l, (int)(neighbour_light - BLOCK_PROPS[new_block.id].absorb_light_level - 1));
}
void update_block_light (Chunks& chunks, bpos pos, uint8 old_light_level, uint8 new_light_level) {
	dbg_block_light_add_list.clear();
	dbg_block_light_remove_list.clear();
	
	auto timer = Timer::start();

	if (new_light_level != old_light_level) {
		if (new_light_level > old_light_level) {
			update_block_light_add(chunks, pos, new_light_level);
		} else {
			update_block_light_remove(chunks, pos, old_light_level);
		}
	}

	auto time = timer.end();
	chunks.block_light_time.push(time);
	logf("Block light update on set_block() (%4d,%4d,%4d) took %7.3f us", pos.x,pos.y,pos.z, time * 1000000);
}
