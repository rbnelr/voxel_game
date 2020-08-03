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
		ImGui::Checkbox("debug_draw_pages", &debug_draw_pages);

		ImGui::Text("pages: %d", pages.allocator.size());
		
		bool show_all_pages = ImGui::TreeNode("all pages");

		int total_nodes = 0;
		int active_nodes = 0;
		for (int i=0; i<pages.size(); ++i) {
			if (pages[i].info.count == INTNULL) continue;

			active_nodes += pages[i].info.count;
			total_nodes += PAGE_NODES;

			if (show_all_pages)
				ImGui::Text("page: %d", pages[i].info.count);
		}

		if (show_all_pages)
			ImGui::TreePop();

		ImGui::Text("nodes: %d active / %d allocated  %6.2f%% active",
			active_nodes, total_nodes,
			(float)active_nodes / total_nodes * 100);
		
		imgui_pop();
	}

	//
	Node page_from_subtree (Page pages[], Page* dstpage, Page const* srcpage, Node srcnode) {
		assert((srcnode & LEAF_BIT) == 0);

		uint16_t children_ptr = dstpage->alloc_node();

		for (int i=0; i<8; ++i) {
			auto& old_child = srcpage->nodes[ srcnode ][i];
			
			if (old_child & FARPTR_BIT) {
				dstpage->add_child(&dstpage->nodes[children_ptr][i], &pages[old_child & ~FARPTR_BIT]);
			}
			
			dstpage->nodes[children_ptr][i] = (Node)( (old_child & (LEAF_BIT|FARPTR_BIT)) ?
				old_child :
				page_from_subtree(pages, dstpage, srcpage, old_child) );
		}

		return (Node)children_ptr;
	}

#if 0
	struct Merger {
		WorldOctree& oct;
		Page& newpage;
		uintptr_t merge_pagei;

		uintptr_t dbg_counter = 0;

		Node recurse (OctreePage& srcpage, OctreeNode srcnode) {
			assert((srcnode & LEAF_BIT) == 0);

			uintptr_t children_ptr = newpage.count < PAGE_NODES ? newpage.alloc_node() : (Node)(LEAF_BIT | B_NULL);
			//uint32_t children_ptr = dbg_counter++;

			for (int i=0; i<8; ++i) {
				auto& old_children = srcpage.nodes[ srcnode ].children;
				
				bool is_farptr = old_children[i] & FARPTR_BIT;
				auto farptr = old_children[i] & ~FARPTR_BIT;
				
				Node val;

				if (old_children[i] & LEAF_BIT || (is_farptr && farptr != merge_pagei)) {
					val = old_children[i];
				} else {
					if (is_farptr) {
						val = recurse(*oct.pages[farptr], (Node)0);
					} else {
						val = recurse(srcpage, old_children[i]);
					}
				}

				if ((children_ptr & LEAF_BIT) == 0)
					newpage.nodes[ children_ptr ].children[i] = val;
			}

			return (Node)children_ptr;
		}
	};
	void merge_with_parent (WorldOctree& oct, Page*& page, Page*& parent_page) {
		Page* newpage = oct.allocator.alloc();
	#if !NDEBUG
		memset(newpage->nodes, 0, sizeof(newpage->nodes));
	#endif

		uintptr_t child_page = (uintptr_t)(&page - &oct.pages[0]);

		Merger m = { oct, *newpage, child_page };
		m.recurse(*parent_page, (Node)0);

		oct.allocator.free(page);
		oct.allocator.free(parent_page);

	#if !NDEBUG
		memset(page, 0, sizeof(Page));
		memset(parent_page, 0, sizeof(Page));
	#endif

		// prefer crash on use after free
		page = nullptr;

		// free page

		parent_page = newpage;
	}
	void checked_merge_with_parent (WorldOctree& oct, Page*& page) {
		if (!page->parent_ptr)
			return; // root page

		auto* parent = page->parent_ptr;

		uintptr_t sum = page->count + parent->count;

		assert(page->count == page->_dbg_count_nodes());
		assert(parent->parent_ptr->count == parent->_dbg_count_nodes());

		if (sum <= PAGE_MERGE_THRES) {
			merge_with_parent(oct, page, *parent);

			assert(parent_page->count == parent_page->_dbg_count_nodes());

			// TODO: why does this fail? output seems visually correct
			assert(parent_page->count == sum);
		}
	}
#endif

#if 0 // Iterative version with explicit stack that was not any faster in my testing
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

	struct PageSplitter {
		Page* page;

		int target_page_size;

		Node* split_node;
		int split_node_closeness = INT_MAX;

		uint16_t find_split_node_recurse (Node* node) {
			assert((*node & LEAF_BIT) == 0);

			uint16_t subtree_size = 1; // count ourself

			// cound children subtrees
			for (int i=0; i<8; ++i) {
				auto& children = page->nodes[*node];
				if ((children[i] & (LEAF_BIT|FARPTR_BIT)) == 0)
					subtree_size += find_split_node_recurse(&children[i]);
			}

			// keep track of node that splits the node in half the best
			int closeness = abs(subtree_size - target_page_size);
			if (closeness < split_node_closeness) {
				split_node = node;
				split_node_closeness = closeness;
			}

			return subtree_size;
		}
	};

	void split_page (WorldOctree& oct, Page* page) {
		// copy page to temp buffer
		Page tmppage;
		memcpy(&tmppage, page, sizeof(Page));

		// find splitnode
		PageSplitter ps = { &tmppage };
		ps.target_page_size = page->info.count / 2;

		Node root = (Node)0;
		ps.find_split_node_recurse(&root);

		assert((*ps.split_node & LEAF_BIT) == 0);
		if (ps.split_node == &root) {
			// edge cases that are not allowed to happen because a split of the root node do not actually split the page in a useful way
			//  this should be impossible with an page subtree depth > 2, I think. (ie. root -> middle -> leaf), which a PAGE_NODES > 
			// But if the page count is invalid because nodes have become unreachable this might still happen
			assert(false);
		}

		// alloc childpage
		Page* childpage = oct.pages.alloc_page();

		// create childpage from subtree
		page_from_subtree(&oct.pages[0], childpage, &tmppage, *ps.split_node);
		
		// set childpage farptr in tmppage
		*ps.split_node = (Node)(oct.pages.indexof(childpage) | FARPTR_BIT);

		// remove children from page and clear all nodes
		page->remove_all_children();
		page->free_all_nodes();

	#if !NDEBUG
		memset(page->nodes, 0, sizeof(page->nodes));
	#endif

		// update page to not contain the subtree that was split off
		// this also links the childpage to page correctly
		page_from_subtree(&oct.pages[0], page, &tmppage, root);
	}

	void recurse_free_subtree (Page pages[], Page* page, Node node) {
		assert((node & LEAF_BIT) == 0);

		if (node & FARPTR_BIT) {
			// keep track of page to be freed?
			//assert(false);

			page = &pages[node & ~FARPTR_BIT];
			page->info.count = INTNULL;

			return;
		}

		for (int i=0; i<8; ++i) {
			auto child = page->nodes[node][i];
			if ((child & LEAF_BIT) == 0) {
				recurse_free_subtree(pages, page, child);
			}
		}
		
		page->free_node(node);
	};
	void free_subtree (Page pages[], Node* node) {
		Page* page = page_from_node(node);

		recurse_free_subtree(pages, page, *node);
	};

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

	// octree write, writes a single voxel at a desired pos, scale to be a particular val
	// this decends the octree from the root and inserts or deletes nodes when needed
	// (ex. writing a 4x4x4 area to be air will delete the nodes of smaller scale contained)
	void octree_write (WorldOctree& oct, int3 pos, int scale, block_id val) {
		pos -= oct.root_pos;
		if (any(pos < 0 || pos >= (1 << oct.root_scale)))
			return;

	restart_decent:
		// start with root node
		int cur_scale = oct.root_scale;
		Page* cur_page = oct.pages.rootpage;
		Node node_ptr = (Node)0;
		Node* child_node = nullptr;

		Node* node_path[MAX_DEPTH] = {};

		for (;;) {
			// get child node that contains target node
			cur_scale--;
			
			auto& children = cur_page->nodes[node_ptr];

			int child_idx = get_child_index(pos, cur_scale);

			child_node = &children[child_idx];

			// keep track of node path for collapsing of same type octree nodes (these get invalidated on split page, which do never need to collapse)
			node_path[cur_scale] = child_node;

			if (cur_scale == scale) {
				// reached target octree depth
				break;
			}

			if ((*child_node & LEAF_BIT) == 0) {
				// recurse normally

				node_ptr = *child_node;

				if (node_ptr & FARPTR_BIT) {
					cur_page = &oct.pages[ node_ptr & ~FARPTR_BIT ];
					node_ptr = (Node)0;
				}
			} else {
				//// Split node into 8 children of same type

				block_id leaf_val = (block_id)(*child_node & ~LEAF_BIT);

				if (leaf_val == val) {
					// do not split node if leaf is already the correct type just at a higher scale
					break;
				}

				if (cur_page->info.count == PAGE_NODES) {
					// page full

					// Do split page which will have created 2 new pages with about half the nodes out of the full page
					split_page(oct, cur_page);

					// cur_child and cur_page were invlidated, I'm not sure how to best fix the fact that the nodes we were iterating though have moved into new pages
					// I might attempt to keep track of cur_child and fix it in split_page, or redecend from the root of the cur page,
					// but a total redecent should always work, so let's just do that with a goto
					goto restart_decent;
				}

				node_ptr = (Node)cur_page->alloc_node();

				// write ptr to children into this nodes slot in parents children array
				*child_node = node_ptr;

				// alloc and init children for node
				auto& children = cur_page->nodes[node_ptr];
				for (int i=0; i<8; ++i) {
					children[i] = (Node)(LEAF_BIT | leaf_val);
				}
			}
		}

		Node* write_node;

		// collapse nodes containing same leaf nodes
		for (int scale=cur_scale;; scale++) {
			write_node = node_path[scale];

			if (scale == oct.root_scale-1) {
				break;
			}

			auto& siblings = *siblings_from_node(write_node);
				
			for (int i=0; i<8; ++i) {
				auto& sibling = siblings[i];
				if (&sibling != write_node && sibling != (LEAF_BIT | val)) {
					goto end; // embrace the goto
				}
			}
		} end:;

		if ((*write_node & LEAF_BIT) == 0) {
			// subtree collapse
			free_subtree(&oct.pages[0], write_node);
		}

		// do the write
		*write_node = (Node)(LEAF_BIT | val);
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

		children_t& get_node (Node node, Page*& cur_page) {
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
			
			if (((node & LEAF_BIT)==0 || (node & ~LEAF_BIT) > B_NULL) && scale <= oct.debug_draw_octree_max)
				debug_graphics->push_wire_cube((float3)pos + 0.5f * size, size * 0.999f, col);

			if ((node & LEAF_BIT) == 0) {
				children_t& children = get_node(node, cur_page);
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
		RecurseDrawer rd = { oct };
		rd.recurse_draw(nullptr, (Node)(0 | FARPTR_BIT), oct.root_pos, oct.root_scale);
	}

	void WorldOctree::pre_update (Player const& player) {
		if (!pages.rootpage) {
			pages.rootpage = pages.alloc_page();

			auto& root = pages.rootpage->nodes[ pages.rootpage->alloc_node() ];
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

	void WorldOctree::add_chunk (Chunk& chunk) {
		int3 pos = chunk.coord * CHUNK_DIM;

		auto a = Timer::start();
		octree_write(*this, pos, CHUNK_DIM_SHIFT, B_AIR);
		auto at = a.end();

		//return;

		auto c = Timer::start();
		{
			//for (int z=0; z<CHUNK_DIM; ++z) {
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
		//for (auto& page : pages)
		//	compact_page(*this, page);
		auto bt = b.end();

		clog("WorldOctree::add_chunk:  octree_write: %f ms  compact_page: %f ms", at * 1000, bt * 1000 / pages.size());
	}

	void WorldOctree::remove_chunk (Chunk& chunk) {
		int3 pos = chunk.coord * CHUNK_DIM;

		auto a = Timer::start();
		octree_write(*this, pos, CHUNK_DIM_SHIFT, B_NULL);
		auto at = a.end();

		auto b = Timer::start();
		//compact_nodes(octree.nodes);
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
		//compact_nodes(octree.nodes);
		auto bt = b.end();

		clog("WorldOctree::update_block:  octree_write: %f ms  reorder_nodes: %f ms", at * 1000, bt * 1000);
	}
}
