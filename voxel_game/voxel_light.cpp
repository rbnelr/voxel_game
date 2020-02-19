#include "voxel_light.hpp"
#include <queue>

struct LightAddNode {
	bpos bp;
};

void update_block_light_add (Chunks& chunks, bpos bp) {
	std::queue<LightAddNode> add_q;

	add_q.push({ bp });

	while (!add_q.empty()) {

	}
}

void update_block_light_remove (Chunks& chunks, bpos bp) {

}
