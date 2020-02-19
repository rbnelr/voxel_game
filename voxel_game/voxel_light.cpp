#include "voxel_light.hpp"
#include <queue>

struct LightAddNode {
	bpos bp;
	Block blk;
};

void update_block_light_add (Chunks& chunks, bpos bp) {
	std::queue<LightAddNode> q;

	q.push({ bp, chunks.query_block(bp) });

	auto neighbour = [&] (LightAddNode const& n, int3 pos) {
		Chunk* chunk;
		bpos pos_in_chunk;
		auto blk = chunks.query_block(pos, &chunk, &pos_in_chunk);

		if (blk.light_level < n.blk.light_level-1) { // BLOCK_PROPS[blk.id].collision != CM_SOLID && 
			blk.light_level = n.blk.light_level-1;
			if (chunk)
				chunk->set_block(chunks, pos_in_chunk, blk);

			q.push({ pos, blk });
		}
	};

	while (!q.empty()) {
		auto n = q.front();
		q.pop();

		neighbour(n, n.bp - int3(1,0,0));
		neighbour(n, n.bp + int3(1,0,0));
		neighbour(n, n.bp - int3(0,1,0));
		neighbour(n, n.bp + int3(0,1,0));
		neighbour(n, n.bp - int3(0,0,1));
		neighbour(n, n.bp + int3(0,0,1));
	}
}

void update_block_light_remove (Chunks& chunks, bpos bp) {

}
