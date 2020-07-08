#include "world_octree.hpp"
#include "dear_imgui.hpp"
#include "chunks.hpp"
#include <stack>

namespace world_octree {
	void WorldOctree::imgui () {
		if (!imgui_push("WorldOctree")) return;

		ImGui::Checkbox("debug_draw_octree", &debug_draw_octree);

		imgui_pop();
	}

	void WorldOctree::update () {

	}

	// insert a node into octree by rebuilding subtree from root to scale (copy tree with changes to new blocks inf NodeStorage, then free old blocks)
	uint32_t insert_node (WorldOctree& oct, uint32_t root, int3 pos, int scale) {
		if (any(pos < 0)) return NULL_NODE;

		auto block = oct.storage.blocks.alloc();
		uint32_t cur_node = 0;

		auto push_nodes = [&] (int count) -> uint32_t {
			if (cur_node > NODE_COUNT - count) {
				block = oct.storage.blocks.alloc();
				cur_node = 0;
			}
			auto ret = (block << PTR_SHIFT) | cur_node;
			cur_node += count;
			return ret;
		};

		int3 parent_pos = 0;
		int parent_scale = tree_scale;
		int child_indx = 0;
		int child_scale = parent_scale - 1;
		
		struct Stackframe {
			int child_indx = 0;
			uint32_t parent_node;
		};
		std::vector<Stackframe> stack;
		stack.reserve(16);

		uint32_t parent_node_old = root;
		uint32_t parent_node = push_nodes(1);
		auto parent_data = oct.storage.get(parent_node_old).data;
		
		// if has children or want children for insert
		uint32_t children = push_nodes(8);

		oct.storage.get(parent_node).data = NONLEAF_MASK | children;

		for (;;) {
			int3 child_pos = parent_pos | (int3(child_indx & 1, (child_indx >> 1) & 1, (child_indx >> 2) & 1) << child_scale);

			uint32_t child_node_old = (parent_data & ~NONLEAF_MASK) + child_indx;


		}
	}

	void WorldOctree::add_chunk (Chunk& chunk) {
		int3 pos = chunk.coord * CHUNK_DIM;
		
		auto indx = insert_node(*this, trunk.root, pos, CHUNK_DIM_SHIFT+1);

	}

	void WorldOctree::remove_chunk (Chunk& chunk) {

	}


	void WorldOctree::add_block (Chunk& chunk, int3 block) {

	}

	void WorldOctree::remove_block (Chunk& chunk, int3 block) {
	
	}
}
