#include "stdafx.hpp"
#include "world_octree.hpp"
#include "chunks.hpp"
#include "player.hpp"

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
		ImGui::Checkbox("debug_draw_pages", &debug_draw_pages);
		ImGui::Checkbox("debug_draw_air", &debug_draw_air);

		ImGui::Text("pages: %d %6.2f MB", pages.allocator.size(), (float)(pages.allocator.size() * sizeof(Page)) / (1024 * 1024));
		
		bool show_all_pages = ImGui::TreeNode("all pages");

		int total_nodes = 0;
		int active_nodes = 0;
		for (uint32_t i=0; i<pages.allocator.freeset_size(); ++i) {
			if (!pages.allocator.is_allocated(i)) continue;

			auto& page = *pages.allocator[i];
			
			active_nodes += page.info.count;
			total_nodes += PAGE_NODES;

			if (show_all_pages)
				ImGui::Text("page: %d", pages[i].info.count);
		}

		if (show_all_pages)
			ImGui::TreePop();

		ImGui::Text("nodes: %d active / %d allocated   %6.2f / %d nodes avg (%6.2f%%)",
			active_nodes, total_nodes,
			(float)active_nodes / pages.size(), PAGE_NODES,
			(float)active_nodes / total_nodes * 100);

		ImGui::Text("page_counters:  splits: %5d  merges: %5d", page_split_counter, page_merge_counter);
		
		imgui_pop();
	}

	//
	Node page_from_subtree (Page pages[], Page* dstpage, Page* srcpage, Node srcnode) {
		assert((srcnode & LEAF_BIT) == 0);

		uintptr_t children_ptr = dstpage->alloc_node();

		for (int i=0; i<8; ++i) {
			auto& child = srcpage->nodes[srcnode][i];

			if (child & FARPTR_BIT) {
				dstpage->add_child(&dstpage->nodes[children_ptr][i], &pages[child & ~FARPTR_BIT]);
			}
			
			dstpage->nodes[children_ptr][i] = child & (LEAF_BIT|FARPTR_BIT) ?
				child :
				page_from_subtree(pages, dstpage, srcpage, child);
		}

		return (Node)children_ptr;
	}

	void merge (WorldOctree& oct, Page* parent, Page* child) {
		ZoneScopedN("world_octree::merge");

		// remove children from child so they can be added to parent node
		child->remove_all_children();

		// create merged page, by adding nodes of child to parent, then replacing the farptr with the new subtree
		*child->info.farptr_ptr = page_from_subtree(&oct.pages[0], parent, child, (Node)0);

		parent->remove_child(child);
		
		oct.pages.free_page(child);

		oct.page_merge_counter++;
	}
	void try_merge (WorldOctree& oct, Page* page) {
		if (!page->info.farptr_ptr)
			return; // root page

		Page* parent = page_from_node(page->info.farptr_ptr);

		uintptr_t sum = page->info.count + parent->info.count;

		if (sum <= PAGE_MERGE_THRES) {
			merge(oct, parent, page);
			// WARNING: page is invalidated, parent might be invalidated
		}
	}

	struct DecentStack {
		Node*	node;
		Page*	page;
	};

	struct PageSplitter {
		Page* page;

		int target_page_size;

		Node* split_node;
		uint16_t split_node_scale;
		int split_node_closeness = INT_MAX;

		uint16_t find_split_node_recurse (Node* node, uint16_t scale) {
			assert((*node & LEAF_BIT) == 0);

			uint16_t subtree_size = 1; // count ourself

			// cound children subtrees
			for (int i=0; i<8; ++i) {
				auto& children = page->nodes[*node];
				if ((children[i] & (LEAF_BIT|FARPTR_BIT)) == 0)
					subtree_size += find_split_node_recurse(&children[i], scale-1);
			}

			// keep track of node that splits the node in half the best
			int closeness = abs(subtree_size - target_page_size);
			if (closeness < split_node_closeness) {
				split_node = node;
				split_node_scale = scale;
				split_node_closeness = closeness;
			}

			return subtree_size;
		}
	};

	struct _SplitSubtree {
		Page* pages;
		Page* splitpage;
		Page* srcpage;
		Node** stack;

		Node split_subtree (Node splitnode, uint16_t scale) {
			assert((splitnode & LEAF_BIT) == 0);

			// alloc node in splitpage
			uint16_t children_ptr = splitpage->alloc_node();

			for (int i=0; i<8; ++i) {
				Node& child = srcpage->nodes[splitnode][i];

				// move over children to splitnode if node was farptr
				if (child & FARPTR_BIT) {
					srcpage->remove_child(&pages[child & ~FARPTR_BIT]);
					splitpage->add_child(&splitpage->nodes[children_ptr][i], &pages[child & ~FARPTR_BIT]);
				}

				if (stack[scale-1] == &child) {
					stack[scale-1] = &splitpage->nodes[children_ptr][i];
				} else {
					for (uint16_t i=0; i<MAX_DEPTH; ++i) {
						if (stack[scale] == &child) {
							assert(false);
						}
					}
				}

				splitpage->nodes[children_ptr][i] = child & (LEAF_BIT|FARPTR_BIT) ?
					child :
					split_subtree(child, scale-1);
			}

			// free children node
			srcpage->free_node(splitnode);

			return (Node)children_ptr;
		}
	};
	void split_page (WorldOctree& oct, Page* page, Node* stack[]) {
		ZoneScopedN("world_octree::split_page");
		
		// find splitnode
		PageSplitter ps = { page };
		ps.target_page_size = page->info.count / 2;

		Node root = (Node)0;
		ps.find_split_node_recurse(&root, page->info.scale);

		assert((*ps.split_node & LEAF_BIT) == 0);
		if (ps.split_node == &root) {
			// edge cases that are not allowed to happen because a split of the root node do not actually split the page in a useful way
			//  this should be impossible with an page subtree depth > 2, I think. (ie. root -> middle -> leaf), which a PAGE_NODES > 
			// But if the page count is invalid because nodes have become unreachable this might still happen
			assert(false);
		}

		// alloc childpage
		Page* childpage = oct.pages.alloc_page();
		childpage->info.scale = ps.split_node_scale;

		// create childpage from subtree
		_SplitSubtree{ &oct.pages[0], childpage, page, stack }.split_subtree(*ps.split_node, ps.split_node_scale);
		
		// set childpage farptr in tmppage
		*ps.split_node = (Node)(oct.pages.indexof(childpage) | FARPTR_BIT);
		page->add_child(ps.split_node, childpage);

		oct.page_split_counter++;
	}

	void recurse_free_subtree (WorldOctree& oct, Page* page, Node node) {
		assert((node & LEAF_BIT) == 0);

		if (node & FARPTR_BIT) {
			oct.pages.free_page(&oct.pages[node & ~FARPTR_BIT]);
			return;
		}

		for (int i=0; i<8; ++i) {
			auto child = page->nodes[node][i];
			if ((child & LEAF_BIT) == 0) {
				recurse_free_subtree(oct, page, child);
			}
		}
		
		page->free_node(node);
	}
	void free_subtree (WorldOctree& oct, Page* page, Node node) {
		ZoneScopedN("world_octree::free_subtree");
		
		recurse_free_subtree(oct, page, node);
	};

	int get_child_index (int3 pos, int scale) {
		//return	(((pos.x >> scale) & 1) << 0) |
		//		(((pos.y >> scale) & 1) << 1) |
		//		(((pos.z >> scale) & 1) << 2);
		//return	((pos.x >> (scale  )) & 1) |
		//		((pos.y >> (scale-1)) & 2) |
		//		((pos.z >> (scale-2)) & 4);
		int ret = 0;
		if (pos.x & (1 << scale))	ret  = 1;
		if (pos.y & (1 << scale))	ret += 2;
		if (pos.z & (1 << scale))	ret += 4;
		return ret;
	}

	// octree write, writes a single voxel at a desired pos, scale to be a particular val
	// this decends the octree from the root and inserts or deletes nodes when needed
	// (ex. writing a 4x4x4 area to be air will delete the nodes of smaller scale contained)
	void octree_write (WorldOctree& oct, int3 pos, int scale, Node val) {
		ZoneScopedN("world_octree::octree_write");

		pos -= oct.root_pos;
		if (any(pos < 0 || pos >= (1 << oct.root_scale)))
			return;

		// start with root node
		int cur_scale = oct.root_scale;

		Node* stack[MAX_DEPTH];
	#if !NDEBUG
		memset(stack, 0, sizeof(stack));
	#endif

		Node* siblings = oct.pages[0].nodes[0];

		for (;;) {
			cur_scale--;

			// get child node that contains target node
			int child_idx = get_child_index(pos, cur_scale);

			Node* child_node = siblings + child_idx;
			Page* page = page_from_node(child_node);

			// keep track of node path for collapsing of same type octree nodes (these get invalidated on split page, which do never need to collapse)
			stack[cur_scale] = child_node;

			if (cur_scale == scale) {
				// reached target octree depth
				break;
			}

			if ((*child_node & LEAF_BIT) == 0) {
				// recurse normally

				Node node_ptr = *child_node;

				if (node_ptr & FARPTR_BIT) {
					page = &oct.pages[ node_ptr & ~FARPTR_BIT ];
					node_ptr = (Node)0;
				}

				siblings = page->nodes[node_ptr];
			} else {
				//// Split node into 8 children of same type

				Node leaf = *child_node;

				if (leaf == val) {
					// do not split node if leaf is already the correct type just at a higher scale
					break;
				}

				if (page->nodes_full()) {
					split_page(oct, page, stack);

					child_node = stack[cur_scale];
					page = page_from_node(child_node);
				}

				Node node_ptr = (Node)page->alloc_node();

				// write ptr to children into this nodes slot in parents children array
				*child_node = node_ptr;

				siblings = page->nodes[node_ptr];

				// alloc and init children for node
				for (int i=0; i<8; ++i) {
					siblings[i] = leaf;
				}
			}
		}

		Node* write_node = stack[scale];

		if (val & LEAF_BIT) {
			// collapse nodes containing same leaf nodes
			for (int scale=cur_scale;; scale++) {
				write_node = stack[scale];

				if (scale == oct.root_scale-1) {
					break;
				}

				Node* siblings = siblings_from_node(write_node);
			
				for (int i=0; i<8; ++i) {
					if (&siblings[i] != write_node && siblings[i] != val) {
						goto end; // embrace the goto
					}
				}
			} end:;
		}

		Node old_val = *write_node;
		Page* page = page_from_node(write_node);

		// do the write
		*write_node = val;

		bool collapsed = (old_val & LEAF_BIT) == 0;
		bool wrote_subpage = val & FARPTR_BIT;

		if (collapsed) {
			// free subtree nodes if subtree was collapsed
			free_subtree(oct, page, old_val);
		} else if (wrote_subpage) {
			Page* childpage = &oct.pages[val & ~FARPTR_BIT];

			page->add_child(write_node, childpage);

			page = childpage;
		}

		if (collapsed || wrote_subpage) {

			try_merge(oct, page);
		}
	}

	void update_root (WorldOctree& oct, Player const& player) {
		//int shift = oct.root_scale - 1;
		//float half_root_scalef = (float)(1 << shift);
		//
		//int3 center = roundi(player.pos / half_root_scalef);
		//
		//int3 pos = (center - 1) << shift;
		//
		//if (equal(pos, oct.root_pos))
		//	return;
		//
		//int3 move = (pos - oct.root_pos) >> shift;
		//
		//auto old_children = oct.octree.nodes[0];
		//
		//for (int i=0; i<8; ++i) {
		//	int3 child = int3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
		//
		//	child += move;
		//
		//	if (any(child < 0 || child > 1)) {
		//		oct.octree.nodes[0].children[i] = LEAF_BIT | B_NULL;
		//	} else {
		//		oct.octree.nodes[0].children[i] = old_children.children[ child.x | (child.y << 1) | (child.z << 2) ];
		//	}
		//}
		//
		//oct.root_pos = pos;
	}

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

	struct RecurseDrawer {
		WorldOctree& oct;

		Node* get_node (Node node, Page*& cur_page) {
			if (node & FARPTR_BIT) {
				cur_page = &oct.pages[node & ~FARPTR_BIT];
				node = (Node)0;
			}
			return cur_page->nodes[node];
		}
		
		void recurse_draw (Page* cur_page, Node node, int3 pos, int scale) {
			float size = (float)(1 << scale);

			auto col = cols[scale % ARRLEN(cols)];
			if (oct.debug_draw_pages)
				col = cols[oct.pages.allocator.indexof(cur_page) % ARRLEN(cols)];
			
			if (((node & LEAF_BIT)==0 || (node & ~LEAF_BIT) > (oct.debug_draw_air ? B_NULL : B_AIR)) && scale <= oct.debug_draw_octree_max)
				debug_graphics->push_wire_cube((float3)pos + 0.5f * size, size * 0.999f, col);

			if ((node & LEAF_BIT) == 0) {
				Node* children = get_node(node, cur_page);
				int child_scale = scale - 1;

				if (child_scale >= oct.debug_draw_octree_min) {
					for (int i=0; i<8; ++i) {
						int3 child_pos = pos + (int3(i & 1, (i >> 1) & 1, (i >> 2) & 1) << child_scale);

						recurse_draw(cur_page, children[i], child_pos, child_scale);
					}
				}
			}
		}
	};

	void debug_draw (WorldOctree& oct) {
		ZoneScopedN("WorldOctree::debug_draw");

		RecurseDrawer rd = { oct };
		rd.recurse_draw(nullptr, (Node)(0 | FARPTR_BIT), oct.root_pos, oct.root_scale);
	}

	void WorldOctree::pre_update (Player const& player) {
		if (pages.size() == 0) {
			auto* rootpage = pages.alloc_page();
			rootpage->info.scale = root_scale;
			assert(pages.indexof(rootpage) == 0);

			auto& root = rootpage->nodes[ rootpage->alloc_node() ];
			for (int i=0; i<8; ++i) {
				root[i] = (Node)(LEAF_BIT | B_NULL);
			}
		}

		update_root(*this, player);
	}
	void WorldOctree::post_update () {
		if (debug_draw_octree) {
			debug_draw(*this);
		}
	}

	bool can_collapse (Node* children) {
		if ((children[0] & LEAF_BIT) == 0)
			return false;

		for (int i=1; i<8; ++i) {
			if (children[i] != children[0])
				return false;
		}

		return true;
	}

	Node recurse_chunk_to_octree (WorldOctree& oct, Chunk& chunk, int3 pos) {
		ZoneScopedN("WorldOctree::recurse_chunk_to_octree");

		Page* chunk_page = oct.pages.alloc_page();
		chunk_page->info.scale = CHUNK_DIM_SHIFT;
		
		Node* node_stack[MAX_DEPTH];
	#if !NDEBUG
		memset(node_stack, 0, sizeof(node_stack));
	#endif

		struct Stack {
			int3 pos;
			int child_indx;
		};
		Stack stack[MAX_DEPTH];

		uint16_t scale = CHUNK_DIM_SHIFT-1;

		Node root = (Node)(FARPTR_BIT | oct.pages.indexof(chunk_page));

		node_stack[CHUNK_DIM_SHIFT] = &root;
		stack[CHUNK_DIM_SHIFT] = { 0, 0 };

		node_stack[scale] = chunk_page->nodes[ chunk_page->alloc_node() ];
		stack[scale] = { 0, 0 };

		for (int i=0; i<8; ++i) {
			chunk_page->nodes[0][i] = (Node)(LEAF_BIT | B_NULL);
		}

		for (;;) {
			int3& pos		= stack[scale].pos;
			int& child_indx	= stack[scale].child_indx;
			Node*& children	= node_stack[scale];
			Node* node		= &children[child_indx];
			Page* page		= page_from_node(children);

			if (child_indx == 8) {
				// Pop
				scale++;

				if (can_collapse(children)) {
					uint16_t children_ptr = (uint16_t)(((char*)children - (char*)page->nodes[0]) / (sizeof(Node)*8));

					node_stack[scale][ stack[scale].child_indx ] = children[0];

					if (children_ptr == 0) {
						oct.pages.free_page(page);
					} else {
						page->free_node(children_ptr);
					}
				}

				if (scale == CHUNK_DIM_SHIFT)
					break;

			} else {

				int3 child_pos = pos;
				int x=pos.x, y=pos.y, z=pos.z;
				int size = 1 << scale;
				if (child_indx & 1) x += size;
				if (child_indx & 2) y += size;
				if (child_indx & 4) z += size;

				if (scale == 0) {
					*node = (Node)(LEAF_BIT | chunk.get_block(int3(x,y,z)).id);
				} else {
					// Push
					if (page->nodes_full()) {
						split_page(oct, page, node_stack);
						
						children	= node_stack[scale];
						node = &children[child_indx];
						page = page_from_node(children);
					}

					uint16_t nodei = page->alloc_node();
					Node* child_children = page->nodes[nodei];

					*node = (Node)nodei;

					scale--;
					stack[scale] = { int3(x,y,z), 0 };
					node_stack[scale] = child_children;

					// init nodes to leaf nodes so future split_page won't fail if we had this uninitialized
					for (int i=0; i<8; ++i) {
						child_children[i] = (Node)(LEAF_BIT | B_NULL);
					}

					continue;
				}
			}

			stack[scale].child_indx++;
		}

		return root;
	}

	void chunk_to_octree (WorldOctree& oct, Chunk& chunk) {
		int3 pos = chunk.coord * CHUNK_DIM;

		Node root = recurse_chunk_to_octree(oct, chunk, 0);

		octree_write(oct, pos, CHUNK_DIM_SHIFT, root);
	}

	void WorldOctree::add_chunk (Chunk& chunk) {
		ZoneScopedN("WorldOctree::add_chunk");

		int3 pos = chunk.coord * CHUNK_DIM;

		auto a = Timer::start();
		//octree_write(*this, pos, CHUNK_DIM_SHIFT, B_AIR);
		auto at = a.end();

		auto c = Timer::start();
		chunk_to_octree(*this, chunk);
		auto ct = c.end();

		clog("WorldOctree::add_chunk:  octree_write: %f ms", at * 1000);
	}

	void WorldOctree::remove_chunk (Chunk& chunk) {
		ZoneScopedN("WorldOctree::remove_chunk");

		int3 pos = chunk.coord * CHUNK_DIM;

		auto a = Timer::start();
		octree_write(*this, pos, CHUNK_DIM_SHIFT, (Node)(LEAF_BIT | B_NULL));
		auto at = a.end();

		clog("WorldOctree::remove_chunk:  octree_write: %f ms", at * 1000);
	}

	void WorldOctree::update_block (Chunk& chunk, int3 bpos, block_id id) {
		ZoneScopedN("WorldOctree::update_block");

		int3 pos = chunk.coord * CHUNK_DIM;
		pos += bpos;

		auto a = Timer::start();
		octree_write(*this, pos, 0, (Node)(LEAF_BIT | id));
		auto at = a.end();

		clog("WorldOctree::update_block:  octree_write: %f ms", at * 1000);
	}
}
