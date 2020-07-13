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

	struct NodeCoord {
		int3		pos;
		int			scale;
	};

	//
	void grow_root (WorldOctree& oct) {
		auto old_root = oct.trunk.nodes[0];

		for (int i=0; i<8; ++i) {
			// alloc 8 new children nodes
			uint32_t index = (uint32_t)oct.trunk.nodes.size();
			
			// only create new root child nodes if they contain anything
			if (old_root.children[i].has_children || old_root.children[i].data != 0) {
				oct.trunk.nodes.push_back({0});

				// place old corner child in opposite slot of new child etc.
				oct.trunk.nodes[index].children[i ^ 0b111] = old_root.children[i];

				oct.trunk.nodes[0].children[i] = { true, index };
			} else {
				oct.trunk.nodes[0].children[i] = { false, 0 };
			}
		}

		oct.root_scale++;
		oct.root_pos = -(1 << (oct.root_scale - 1));
	}

	bool try_root_shrink (WorldOctree& oct) {
		auto& root = oct.trunk.nodes[0];
		
		OctreeChildren new_root = {0}; 

		for (int i=0; i<8; ++i) {
			if (root.children[i].has_children) {
				auto* child_children = oct.trunk.nodes[ root.children[i].data ].children;
				
				for (int j=0; j<8; ++j) {
					if ((j != (i ^ 0b111)) && (child_children[j].has_children || child_children[j].data != 0)) {
						return false; // could not shrink root
					}
				}

				new_root.children[i] = child_children[i ^ 0b111];
			}
		}

		root = new_root;

		oct.root_scale--;
		oct.root_pos = -(1 << (oct.root_scale - 1));

		return true; // did shrink
	}

	int get_child_index (int3 pos, int scale) {
		//return	(((pos.x >> scale) & 1) << 0) |
		//		(((pos.y >> scale) & 1) << 1) |
		//		(((pos.z >> scale) & 1) << 2);
		//return	((pos.x >> (scale  )) & 1) |
		//		((pos.y >> (scale-1)) & 2) |
		//		((pos.z >> (scale-2)) & 4);
		int ret = 0;
		if (pos.x & (1 << scale))	ret += 1;
		if (pos.y & (1 << scale))	ret += 2;
		if (pos.z & (1 << scale))	ret += 4;
		return ret;
	}

	void insert_node (WorldOctree& oct, NodeCoord insert_node) {
		int3 relpos = insert_node.pos - oct.root_pos;
		while (any(relpos < 0 || relpos >= (1 << oct.root_scale)) && oct.root_scale < 13) {
			grow_root(oct);
		}
		if (any(relpos < 0 || relpos >= (1 << oct.root_scale)))
			return;

		// start with root node
		int scale = oct.root_scale;
		uint32_t children_ptr = 0;
		
		OctreeNode* node_data = nullptr;

		for (;;) {
			// get child that contains insert_node
			scale--;
			
			int child_idx = get_child_index(insert_node.pos ^ oct.root_pos, scale); // ^ oct.root_pos to account for root node half-offset

			// 'recurse' into child node
			node_data = &oct.trunk.nodes[children_ptr].children[child_idx];

			if (scale == insert_node.scale)
				break;

			if (node_data->has_children) {
				children_ptr = node_data->data;
			} else {
				//// Insert children entry if not was a leaf before

				children_ptr = (uint32_t)oct.trunk.nodes.size();

				// write ptr to children into this nodes slot in parents children array
				*node_data = { true, children_ptr };

				// alloc children for node
				oct.trunk.nodes.push_back({0});
			}
		}

		*node_data = { false, 1 };
	}

	void remove_node (WorldOctree& oct, NodeCoord remove_node) {
		// start with root node
		int scale = oct.root_scale;
		uint32_t children_ptr = 0;

		OctreeNode* node_data = nullptr;

		// biggest node that only contains us as leaf node, this one will be deleted
		OctreeNode* highest_exclusive_parent = nullptr;
		int highest_exclusive_parent_scale = -1;

		for (;;) {
			// get child that contains insert_node
			scale--;

			int child_idx = get_child_index(remove_node.pos ^ oct.root_pos, scale); // ^ oct.root_pos to account for root node half-offset

			// 'recurse' into child node
			node_data = &oct.trunk.nodes[children_ptr].children[child_idx];

			{ // count children; if == 1 then set highest_exclusive_parent
				if (children_ptr == 0) {
					highest_exclusive_parent = node_data;
				} else {
					auto* children = oct.trunk.nodes[children_ptr].children;

					for (int i=0; i<8; ++i) {
						if ( !(i == child_idx || (!children[i].has_children && children[i].data == 0)) ) {
							highest_exclusive_parent = node_data;
							break;
						}
					}
				}
			}

			if (scale == remove_node.scale)
				break;

			assert(node_data->has_children);
			children_ptr = node_data->data;
		}

		assert(highest_exclusive_parent);
		*highest_exclusive_parent = { false, 0 };

		while (try_root_shrink(oct)); // shrink multiple times
	}

	Gradient colors = Gradient({
		{ 0.00f,	srgba(0x010934) },
		{ 0.12f,	srgba(0x0030c8) },
		{ 0.24f,	srgba(0x02f9ff) },
		{ 0.36f,	srgba(0x3eff64) },
		{ 0.47f,	srgba(0x63ff00) },
		{ 0.60f,	srgba(0xfaff27) },
		{ 0.73f,	srgba(0xffbc06) },
		{ 0.86f,	srgba(0xff5715) },
		{ 1.00f,	srgba(0xff00b5) },
	});

	void recurse_draw (WorldOctree& oct, OctreeNode node, int3 pos, int scale) {
		float size = (float)(1 << scale);

		auto col = colors.calc((float)scale / (oct.root_scale + 1));
		//if (!node.has_children && node.data == 0)
		//	col *= lrgba(.5f,.5f,.5f, .6f);
		//debug_graphics->push_wire_cube((float3)pos + 0.5f * size, size * 0.995f, col);
		if (node.has_children || node.data != 0)
			debug_graphics->push_wire_cube((float3)pos + 0.5f * size, size * 0.995f, col);

		if (node.has_children) {
			OctreeChildren children = oct.trunk.nodes[node.data];
			int child_scale = scale - 1;

			for (int i=0; i<8; ++i) {
				int3 child_pos = pos + (int3(i & 1, (i >> 1) & 1, (i >> 2) & 1) << child_scale);

				recurse_draw(oct, children.children[i], child_pos, child_scale);
			}
		}
	}

	void debug_draw (WorldOctree& oct) {
		recurse_draw(oct, { true, 0 }, oct.root_pos, oct.root_scale);
	}

	void WorldOctree::update () {
		if (trunk.nodes.size() == 0) {
			trunk.nodes.push_back({});
		}

		if (debug_draw_octree) {
			debug_draw(*this);
		}
	}

	void WorldOctree::add_chunk (Chunk& chunk) {
		int3 pos = chunk.coord * CHUNK_DIM;
		
		insert_node(*this, { pos, CHUNK_DIM_SHIFT });
	}

	void WorldOctree::remove_chunk (Chunk& chunk) {
		int3 pos = chunk.coord * CHUNK_DIM;
		
		remove_node(*this, { pos, CHUNK_DIM_SHIFT });
	}


	void WorldOctree::add_block (Chunk& chunk, int3 block) {

	}

	void WorldOctree::remove_block (Chunk& chunk, int3 block) {
	
	}
}
