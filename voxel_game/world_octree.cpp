#include "world_octree.hpp"
#include "dear_imgui.hpp"
#include "chunks.hpp"
#include "player.hpp"
#include "util/timer.hpp"
#include <stack>

namespace world_octree {
	void WorldOctree::imgui () {
		if (!imgui_push("WorldOctree")) {
			ImGui::SameLine();
			ImGui::Checkbox("debug_draw_octree", &debug_draw_octree);
			return;
		}

		ImGui::Checkbox("debug_draw_octree", &debug_draw_octree);
		ImGui::SliderInt("debug_draw_octree_min", &debug_draw_octree_min, 0, 20);
		ImGui::SliderInt("debug_draw_octree_max", &debug_draw_octree_max, 0, 20);

		ImGui::Text("       trunk nodes: %d", (int)octree.nodes.size());
		ImGui::Text("active trunk nodes: %d", active_trunk_nodes);
		ImGui::Text("dead   trunk nodes: %d", (int)octree.nodes.size() - active_trunk_nodes);

		imgui_pop();
	}

	//
#if 0
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
#endif

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

	void octree_write (WorldOctree& oct, int3 pos, int scale, block_id val) {
		pos -= oct.root_pos;
		if (any(pos < 0 || pos >= (1 << oct.root_scale)))
			return;

		// start with root node
		int cur_scale = oct.root_scale;
		uint32_t* cur_node = nullptr;
		uint32_t children_ptr = 0;

		// biggest node that only contains us as leaf node, this one will be deleted
		uint32_t* write_node = nullptr;

		for (;;) {
			// get child node that contains target node
			cur_scale--;
			
			auto& siblings = oct.octree.nodes[children_ptr].children;

			int child_idx = get_child_index(pos, cur_scale);

			cur_node = &siblings[child_idx];

			{ // keep track of highest node that will end up with 8 children of the same leaf type once write happened (overwriting this collapses the subtree)
				if (children_ptr == 0) {
					// special case because we neverwant to collapse the root nodes children (because root node is implicit)
					write_node = cur_node;
				} else {
					// overwrite highest_exclusive_parent with current node if the siblings have children or are of other type
					
					for (int i=0; i<8; ++i) {
						if (i != child_idx && ((siblings[i] & LEAF_BIT) == 0 || (siblings[i] & ~LEAF_BIT) != val)) {
							write_node = cur_node;
							break;
						}
					}
				}
			}

			if (cur_scale == scale) {
				// reached target octree depth
				break;
			}

			if ((*cur_node & LEAF_BIT) == 0) {
				// recurse normally
				children_ptr = *cur_node;
			} else {
				//// Split node into 8 children of same type

				block_id leaf_val = (block_id)(*cur_node & ~LEAF_BIT);

				if (leaf_val == val) {
					// do not split node if leaf is already the correct type just at a higher scale
					assert(write_node == cur_node);
					break;
				}

				children_ptr = (uint32_t)oct.octree.nodes.size();

				// write ptr to children into this nodes slot in parents children array
				*cur_node = children_ptr;

				// alloc and init children for node
				oct.octree.nodes.emplace_back(); // invalidates node_data and highest_exclusive_parent
				
				auto& children = oct.octree.nodes.back().children;
				for (int i=0; i<8; ++i) {
					children[i] = LEAF_BIT | leaf_val;
				}
			}
		}

		// do the write
		*write_node = LEAF_BIT | val;
	}

	void update_root (WorldOctree& oct, Player const& player) {
		int shift = oct.root_scale - 1;
		float half_root_scalef = (float)(1 << shift);

		int3 center = roundi(player.pos / half_root_scalef);
		
		int3 pos = (center - 1) << shift;

		if (equal(pos, oct.root_pos))
			return;

		int3 move = (pos - oct.root_pos) >> shift;

		auto old_children = oct.octree.nodes[0];

		for (int i=0; i<8; ++i) {
			int3 child = int3(i & 1, (i >> 1) & 1, (i >> 2) & 1);

			child += move;

			if (any(child < 0 || child > 1)) {
				oct.octree.nodes[0].children[i] = LEAF_BIT | B_NULL;
			} else {
				oct.octree.nodes[0].children[i] = old_children.children[ child.x | (child.y << 1) | (child.z << 2) ];
			}
		}

		oct.root_pos = pos;
	}

#if 0
	uint32_t recurse_comapct (std::vector<OctreeChildren>& new_nodes, std::vector<OctreeChildren>& old_nodes, uint32_t old_node) {
		assert((old_node & LEAF_BIT) == 0);

		uint32_t children_ptr = (uint32_t)new_nodes.size();
		new_nodes.emplace_back();

		auto& old_children = old_nodes[ old_node ].children;
		for (int i=0; i<8; ++i) {
			new_nodes[ children_ptr ].children[i] = (old_children[i] & LEAF_BIT) ? old_children[i] : recurse_comapct(new_nodes, old_nodes, old_children[i]);
		}

		return children_ptr;
	}

	// 'compact' nodes, ie. iterate the octree nodes recursivly and write them in depth first order into a new array,
	//  this resorts the out-of-order nodes left by node insertions and the dead nodes left by node collapse
	void compact_nodes (std::vector<OctreeChildren>& nodes) {
		std::vector<OctreeChildren> new_nodes;
		
		uint32_t old_root = 0;
		recurse_comapct(new_nodes, nodes, old_root);
		
		nodes = new_nodes;
	}
#else
	void compact_nodes (std::vector<OctreeChildren>& nodes) {
		std::vector<OctreeChildren> new_nodes;
		new_nodes.reserve(4096);

		struct Stack {
			uint32_t	children_ptr;
			uint32_t	old_parent;
			uint32_t	child_indx;
		};

		static constexpr int MAX_DEPTH = 16;
		Stack stack[MAX_DEPTH];
		int depth = 0;

		Stack* s = &stack[depth];
		*s = {0,0,0};
		
		// Alloc root children
		new_nodes.emplace_back();

		for (;;) {
			if (s->child_indx == 8) {
				// Pop
				if (depth == 0)
					break;
				s = &stack[--depth];
			} else {
				uint32_t child_node = nodes[s->old_parent].children[s->child_indx];

				if (child_node & LEAF_BIT) {
					new_nodes[ s->children_ptr ].children[s->child_indx++] = child_node;
				} else {
					uint32_t alloc = (uint32_t)new_nodes.size();

					// Alloc
					new_nodes[ s->children_ptr ].children[s->child_indx++] = alloc;
					new_nodes.emplace_back();

					// Push
					s = &stack[++depth];
					*s = { alloc, child_node, 0 };
				}
			}
		}

		nodes = std::move(new_nodes);
	}
#endif

	lrgba cols[] = {
		srgba(255,0,0),
		srgba(0,255,0),
		srgba(0,0,255),
		srgba(255,255,0),
		srgba(255,0,255),
		srgba(0,255,255),
		srgba(127,0,255),
		srgba(255,0,127),
		srgba(255,127,255),
	};

	void recurse_draw (WorldOctree& oct, uint32_t node, int3 pos, int scale) {
		float size = (float)(1 << scale);

		//if (!node.has_children && node.data == 0)
		//	col *= lrgba(.5f,.5f,.5f, .6f);
		//debug_graphics->push_wire_cube((float3)pos + 0.5f * size, size * 0.995f, col);
		if (((node & LEAF_BIT)==0 || (node & ~LEAF_BIT) > B_AIR) && scale <= oct.debug_draw_octree_max)
			debug_graphics->push_wire_cube((float3)pos + 0.5f * size, size * 0.999f, cols[scale % ARRLEN(cols)]);

		if ((node & LEAF_BIT) == 0) {
			oct.active_trunk_nodes++;

			OctreeChildren children = oct.octree.nodes[node];
			int child_scale = scale - 1;

			if (child_scale >= oct.debug_draw_octree_min) {
				for (int i=0; i<8; ++i) {
					int3 child_pos = pos + (int3(i & 1, (i >> 1) & 1, (i >> 2) & 1) << child_scale);

					recurse_draw(oct, children.children[i], child_pos, child_scale);
				}
			}
		}
	}

	void debug_draw (WorldOctree& oct) {
		oct.active_trunk_nodes = 0;

		recurse_draw(oct, 0, oct.root_pos, oct.root_scale);
	}

	void WorldOctree::pre_update (Player const& player) {
		if (octree.nodes.size() == 0) {
			octree.nodes.emplace_back();

			for (int i=0; i<8; ++i) {
				octree.nodes[0].children[i] = LEAF_BIT | B_NULL;
			}
		}

		update_root(*this, player);
	}
	void WorldOctree::post_update () {
		active_trunk_nodes = -1;
		if (debug_draw_octree) {
			debug_draw(*this);
		}
	}

	void WorldOctree::add_chunk (Chunk& chunk) {
		int3 pos = chunk.coord * CHUNK_DIM;
		
		auto a = Timer::start();
		octree_write(*this, pos, CHUNK_DIM_SHIFT, B_AIR);
		auto at = a.end();

		auto c = Timer::start();
		{
			for (int z=0; z<CHUNK_DIM; ++z) {
				for (int y=0; y<CHUNK_DIM; ++y) {
					for (int x=0; x<CHUNK_DIM; ++x) {
						int3 bpos = pos + int3(x,y,z);

						octree_write(*this, bpos, 0, chunk.get_block(int3(x,y,z)).id);
					}
				}
			}
		}
		auto ct = c.end();

		auto b = Timer::start();
		compact_nodes(octree.nodes);
		auto bt = b.end();

		clog("WorldOctree::add_chunk:  octree_write: %f ms  reorder_nodes: %f ms", at * 1000, bt * 1000);
	}

	void WorldOctree::remove_chunk (Chunk& chunk) {
		int3 pos = chunk.coord * CHUNK_DIM;

		auto a = Timer::start();
		octree_write(*this, pos, CHUNK_DIM_SHIFT, B_NULL);
		auto at = a.end();

		auto b = Timer::start();
		compact_nodes(octree.nodes);
		auto bt = b.end();

		clog("WorldOctree::remove_chunk:  octree_write: %f ms  reorder_nodes: %f ms", at * 1000, bt * 1000);
	}

	void WorldOctree::update_block (Chunk& chunk, int3 bpos, block_id id) {
		int3 pos = chunk.coord * CHUNK_DIM;
		pos += bpos;

		auto a = Timer::start();
		octree_write(*this, pos, 0, id);
		auto at = a.end();

		auto b = Timer::start();
		compact_nodes(octree.nodes);
		auto bt = b.end();

		clog("WorldOctree::update_block:  octree_write: %f ms  reorder_nodes: %f ms", at * 1000, bt * 1000);
	}
}
