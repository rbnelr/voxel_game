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
	static constexpr uint32_t PAGE_NODES = 4;//PAGE_SIZE / sizeof(OctreeChildren);

	struct OctreePage {
		OctreeChildren nodes[PAGE_NODES];
	};

	struct AllocatedPage {
		uint32_t	node_count;

		OctreePage*	page;
	};

	class WorldOctree {
	public:

		int			root_scale = 7;//10;
		int3		root_pos = -(1 << (root_scale - 1));

		BlockAllocator<OctreePage> allocator;

		std::vector<AllocatedPage> pages;

		AllocatedPage page_from_subtree (OctreePage const& srcpage, OctreeNode subroot);
		void split_page (AllocatedPage* page);

		//
		bool debug_draw_octree = true;//false;

		int debug_draw_octree_min = 0;//4;
		int debug_draw_octree_max = 20;

		int active_pages = -1;

		void imgui ();
		void pre_update (Player const& player);
		void post_update ();

		void add_chunk (Chunk& chunk);
		void remove_chunk (Chunk& chunk);

		void update_block (Chunk& chunk, int3 bpos, block_id id);
	};
}
using world_octree::WorldOctree;
