#pragma once
#include "blocks.hpp"
#include "stdint.h"
#include "util/block_allocator.hpp"
#include <vector>

class Chunk;
class Chunks;
class Player;

namespace world_octree {
	enum OctreeNode : uint32_t {
		LEAF_BIT = 0x80000000u,
		FARPTR_BIT = 0x40000000u,
	};

	struct OctreeChildren {
		OctreeNode children[8];
	};

	//static constexpr uint32_t PAGE_SIZE = 1024*128;
	static constexpr uint32_t PAGE_NODES = 2048;//PAGE_SIZE / sizeof(OctreeChildren);

	struct OctreePage {
		OctreeChildren nodes[PAGE_NODES];
	};

	struct AllocatedPage {
		OctreePage*	page;

		uint32_t	node_count = 0;
		bool		changed = false; // changed flag used for compacting the correct 
	};

	class WorldOctree {
	public:

		int			root_scale = 10;
		int3		root_pos = -(1 << (root_scale - 1));

		BlockAllocator<OctreePage> allocator;

		std::vector<AllocatedPage> pages;

		//
		bool debug_draw_octree = false;

		int debug_draw_octree_min = 4;
		int debug_draw_octree_max = 20;

		bool debug_draw_pages = true;

		int active_pages = -1;
		int active_nodes = -1;
		int last_modified_page = -1;

		void imgui ();
		void pre_update (Player const& player);
		void post_update ();

		void add_chunk (Chunk& chunk);
		void remove_chunk (Chunk& chunk);

		void update_block (Chunk& chunk, int3 bpos, block_id id);
	};
}
using world_octree::WorldOctree;
