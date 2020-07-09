#pragma once
#include "blocks.hpp"
#include "stdint.h"
#include <vector>

class Chunk;
class Chunks;

namespace world_octree {
	static constexpr uint32_t NONLEAF_MASK = 0x80000000u;

	static constexpr int tree_scale = 12;
	static constexpr int3 root_pos = 0;

	struct OctreeNode {
		bool has_children : 1;
		uint32_t data : 31;
	};

	struct OctreeChildren {
		OctreeNode data[8];
	};

	// Octree of all in-memory chunks
	struct OctreeTrunk {
		std::vector<OctreeChildren> nodes;
	};

	class WorldOctree {
	public:

		OctreeTrunk trunk;

		bool debug_draw_octree = true;

		void imgui ();
		void update ();

		void add_chunk (Chunk& chunk);
		void remove_chunk (Chunk& chunk);

		void add_block (Chunk& chunk, int3 block);
		void remove_block (Chunk& chunk, int3 block);
	};
}
using world_octree::WorldOctree;
