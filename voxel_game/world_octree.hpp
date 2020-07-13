#pragma once
#include "blocks.hpp"
#include "stdint.h"
#include <vector>

class Chunk;
class Chunks;
class Player;

namespace world_octree {
	struct OctreeNode {
		bool has_children : 1;
		uint32_t data : 31;
	};

	struct OctreeChildren {
		OctreeNode children[8];
	};

	struct Octree {
		std::vector<OctreeChildren> nodes;
	};

	class WorldOctree {
	public:

		int			root_scale = 8;
		int3		root_pos = -(1 << (root_scale - 1));

		Octree trunk;

		//
		bool debug_draw_octree = false;

		int debug_draw_octree_min = 0;
		int debug_draw_octree_max = 20;

		int active_trunk_nodes = -1;

		void imgui ();
		void pre_update (Player const& player);
		void post_update ();

		void add_chunk (Chunk& chunk);
		void remove_chunk (Chunk& chunk);

		void update_block (Chunk& chunk, int3 bpos, block_id id);
	};
}
using world_octree::WorldOctree;
