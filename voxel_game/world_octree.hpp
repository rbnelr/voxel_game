#pragma once
#include "blocks.hpp"
#include "stdint.h"
#include <vector>

class Chunk;
class Chunks;

namespace world_octree {
	template <typename T>
	struct BlockAllocator {
		union Block {
			int32_t next;
			T		data;
		};
	
		std::vector<Block> blocks;

		int32_t freelist = -1;

		int32_t _alloc () {
			if (freelist == -1)
				return -1;

			auto block = freelist;
			freelist = blocks[block].next;

			return block;
		}

		int32_t alloc () {
			auto block = _alloc();
			if (block == -1) {
				blocks.emplace_back();
				block = (int32_t)(blocks.size() - 1);
			}

			return block;
		}

		void free (int32_t block) {
			blocks[block].next = freelist;
			freelist = block;
		}

		T& operator[] (int32_t indx) {
			return blocks[indx].data;
		}
	};
	static constexpr uint32_t NONLEAF_MASK = 0x80000000u;

	struct OctreeNode {
		uint32_t data;	// NONLEAF_MASK | abs 31 bit child0 ptr
						// or 16 bit block_id
	};

	static constexpr size_t NODE_COUNT = 1 << 14;

	static constexpr uint32_t PTR_MASK  = (uint32_t)(NODE_COUNT -1);
	static constexpr uint32_t PTR_SHIFT = 14;

	static constexpr int tree_scale = 10;
	static constexpr int3 root_pos = 0;

	static constexpr uint32_t NULL_NODE = (uint32_t)-1;

	struct NodeStorage {
		struct Block {
			OctreeNode	nodes[NODE_COUNT];
		};

		BlockAllocator<Block> blocks;

		OctreeNode& get (uint32_t ptr) {
			uint32_t block_index = ptr &  PTR_MASK;
			uint32_t node_index  = ptr & ~PTR_MASK;

			return blocks[(int32_t)block_index].nodes[node_index];
		}
	};

	// Octree of all in-memory chunks
	struct OctreeTrunk {
		uint32_t root = NULL_NODE;
	};
	// Octree in each chunk
	struct ChunkOctree {
		uint32_t subroot;
	};

	class WorldOctree {
	public:

		NodeStorage storage;
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
