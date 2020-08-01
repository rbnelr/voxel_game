#pragma once
#include "blocks.hpp"
#include "stdint.h"
#include "util/block_allocator.hpp"
#include <vector>
#include "assert.h"

class Chunk;
class Chunks;
class Player;

namespace world_octree {
	enum OctreeNode : uint32_t {
		LEAF_BIT = 0x80000000u,
		FARPTR_BIT = 0x40000000u,
	};

	static constexpr int MAX_DEPTH = 20;

	//static constexpr uint32_t PAGE_SIZE = 1024*128;
	static constexpr uint32_t PAGE_NODES = 2048 -1;//PAGE_SIZE / sizeof(OctreeChildren);

	static constexpr uint32_t PAGE_COMPACT_THRES = (uint32_t)(PAGE_NODES * 0.05f);
	static constexpr uint32_t PAGE_MERGE_THRES   = (uint32_t)(PAGE_NODES * 0.7f);

	struct OctreeChildren {
		OctreeNode children[8];
	};

	static constexpr uint32_t INTNULL = (uint32_t)-1;
	struct OctreePage {
		uint32_t		count;
		uint32_t		parent_page;
		uint32_t		freelist;

		alignas(sizeof(uint32_t)*8)
		OctreeChildren	nodes[PAGE_NODES];

		uint32_t alloc_node () {
			assert(count < PAGE_NODES);

			if (freelist == INTNULL) {
				return count++;
			}

			count++;
			uint32_t ret = freelist;
			freelist = *((uint32_t*)&nodes[freelist]);
			return ret;
		}
		void free_node (uint32_t node) {
			assert(count > 0 && (node & (FARPTR_BIT|LEAF_BIT)) == 0 && node < PAGE_NODES);

			*((uint32_t*)&nodes[node]) = freelist;
			freelist = node;
			count--;
		}
	};

	class WorldOctree {
	public:

		int			root_scale = 8;//10;
		int3		root_pos = -(1 << (root_scale - 1));

		BlockAllocator<OctreePage, sizeof(uint32_t) * 8> allocator;

		std::vector<OctreePage*> pages;

		//
		bool debug_draw_octree = false;

		int debug_draw_octree_min = 4;
		int debug_draw_octree_max = 20;

		bool debug_draw_pages = true;

		int active_pages = -1;
		int active_nodes = -1;

		void imgui ();
		void pre_update (Player const& player);
		void post_update ();

		void add_chunk (Chunk& chunk);
		void remove_chunk (Chunk& chunk);

		void update_block (Chunk& chunk, int3 bpos, block_id id);
	};
}
using world_octree::WorldOctree;
