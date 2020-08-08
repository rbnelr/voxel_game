#include "stdafx.hpp"
#include "voxel_light.hpp"

struct LitBlock {
	bpos bp;
	unsigned block_light : 8;
};

bool operator< (LitBlock const& l, LitBlock const& r) {
	return l.block_light < r.block_light;
}

std::vector<bpos> dbg_block_light_add_list;
std::vector<bpos> dbg_block_light_remove_list;

void light_propagate (Chunks& chunks, std::priority_queue<LitBlock>& q) {
	while (!q.empty()) {
		auto n = q.top();
		q.pop();
		dbg_block_light_add_list.push_back(n.bp);

		static constexpr int3 dirs[] = {
			int3(-1,0,0), int3(+1,0,0),
			int3(0,-1,0), int3(0,+1,0),
			int3(0,0,-1), int3(0,0,+1),
		};

		for (auto dir : dirs) {
			int3 pos = n.bp + dir;

			Chunk* chunk;
			bpos pos_in_chunk;
			auto blk = chunks.query_block(pos, &chunk, &pos_in_chunk);
			unsigned l = (unsigned)max((int)n.block_light - (int)blocks.absorb[blk.id] - 1, 0);

			if (l > blk.block_light) {
				blk.block_light = l;

				q.push({ pos, blk.block_light });

				if (chunk)
					chunk->_set_block_no_light_update(chunks, pos_in_chunk, blk);
			}
		}
	}
}

void update_block_light_add (Chunks& chunks, bpos bp, unsigned new_light_level) {
	std::priority_queue<LitBlock> q;

	q.push({ bp, new_light_level });

	light_propagate(chunks, q);
}

void update_block_light_remove (Chunks& chunks, bpos bp, unsigned old_light_level) {
	std::priority_queue<LitBlock> remove_q;
	std::priority_queue<LitBlock> add_q;
	
	remove_q.push({ bp, old_light_level });
	
	while (!remove_q.empty()) {
		auto n = remove_q.top();
		remove_q.pop();
		dbg_block_light_remove_list.push_back(n.bp);

		static constexpr int3 dirs[] = {
			int3(-1,0,0), int3(+1,0,0),
			int3(0,-1,0), int3(0,+1,0),
			int3(0,0,-1), int3(0,0,+1),
		};

		for (auto dir : dirs) {
			int3 pos = n.bp + dir;

			Chunk* chunk;
			bpos pos_in_chunk;
			auto blk = chunks.query_block(pos, &chunk, &pos_in_chunk);

			if (blk.block_light > 0) {
				unsigned l = (unsigned)max((int)n.block_light - (int)blocks.absorb[blk.id] - 1, 0);
				if (blk.block_light == l) {
					// block was lit by our source block, zero it and repropagate light into it from other light sources
					remove_q.push({ pos, blk.block_light });

					blk.block_light = 0;

					if (chunk)
						chunk->_set_block_no_light_update(chunks, pos_in_chunk, blk);
				} else {
					// block is too bright to be lit by us -> this is a source that can light up the blocks we are zeroing
					// add to a seperate queue and repropagate light in a second pass
					add_q.push({ pos, blk.block_light });
				}
			}
		}
	}

	light_propagate(chunks, add_q);
}

unsigned calc_block_light_level (Chunk* chunk, bpos pos_in_chunk, Block new_block) {
	// The light levels of the neighbours of this block are either the result of:
	//  A: the light coming in at that neighbour (still valid after placing this block)
	//  B: the light was the result of the light from A (this might not be valid anymore)
	// Since the B-light_level < A-light_level, the max light level of all the neighbours is always the correct light level to light this block with
	
	int l = new_block.block_light; // glow level
	
	int a = chunk->get_block(pos_in_chunk - int3(1,0,0)).block_light;
	int b = chunk->get_block(pos_in_chunk + int3(1,0,0)).block_light;
	int c = chunk->get_block(pos_in_chunk - int3(0,1,0)).block_light;
	int d = chunk->get_block(pos_in_chunk + int3(0,1,0)).block_light;
	int e = chunk->get_block(pos_in_chunk - int3(0,0,1)).block_light;
	int f = chunk->get_block(pos_in_chunk + int3(0,0,1)).block_light;
	
	int neighbour_light = max(
		max(max(a,b), max(c,d)),
		max(e,f)
	);
	
	return (unsigned)max((int)l, (int)(neighbour_light - blocks.absorb[new_block.id] - 1));
}
void update_block_light (Chunks& chunks, bpos pos, unsigned old_light_level, unsigned new_light_level) {
	ZoneScopedN("update_block_light");

	if (new_light_level != old_light_level) {
		dbg_block_light_add_list.clear();
		dbg_block_light_remove_list.clear();
	
		auto timer = Timer::start();
	
		if (new_light_level > old_light_level) {
			update_block_light_add(chunks, pos, new_light_level);
		} else {
			update_block_light_remove(chunks, pos, old_light_level);
		}
	
		auto time = timer.end();
		chunks.block_light_time.push(time);
		clog("Block light update on set_block() (%4d,%4d,%4d) took %7.3f us", pos.x,pos.y,pos.z, time * 1000000);
	}
}

void update_sky_light_column (Chunk* chunk, bpos pos_in_chunk) {

	//OPTICK_EVENT();

	int sky_light = pos_in_chunk.z >= CHUNK_DIM-1 ? MAX_LIGHT_LEVEL : chunk->get_block(pos_in_chunk + bpos(0,0,1)).sky_light;
	bpos pos = pos_in_chunk;

	for (; pos.z>=0 && sky_light>0; --pos.z) {
		auto indx = ChunkData::pos_to_index(pos);
		auto* sl = &chunk->blocks->sky_light[ indx ];
		auto id = chunk->blocks->id[ indx ];

		sky_light = max(sky_light - blocks.absorb[id], 0);
		*sl = sky_light;
	}

	for (; pos.z>=0; --pos.z) {
		auto* sl = &chunk->blocks->sky_light[ ChunkData::pos_to_index(pos) ];

		if (*sl == 0)
			break;

		*sl = 0;
	}

	chunk->needs_remesh = true;
}
void update_sky_light_chunk (Chunk* chunk) {
	ZoneScopedN("update_sky_light_chunk");
	
	for (int y=0; y<CHUNK_DIM; ++y) {
		for (int x=0; x<CHUNK_DIM; ++x) {
			update_sky_light_column(chunk, bpos(x,y, CHUNK_DIM-1));
		}
	}
}
