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
		ImGui::Checkbox("debug_draw_air", &debug_draw_air);

		ImGui::Text("pages: %d %6.2f MB", pages.allocator.size(), (float)(pages.allocator.size() * sizeof(Page)) / (1024 * 1024));
		
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

		ImGui::Text("nodes: %d active / %d allocated   %6.2f / %d nodes avg (%6.2f%%)",
			active_nodes, total_nodes,
			(float)active_nodes / pages.size(), PAGE_NODES,
			(float)active_nodes / total_nodes * 100);

		ImGui::Text("page_counters:  splits: %5d  merges: %5d  frees: %5d", page_split_counter, page_merge_counter, page_free_counter);
		
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
		// remove children from child so they can be added to parent node
		child->remove_all_children();

		// create merged page, by adding nodes of child to parent, then replacing the farptr with the new subtree
		*child->info.farptr_ptr = page_from_subtree(&oct.pages[0], parent, child, (Node)0);

		parent->remove_child(child);
		
		oct.pages.free_page(child); // WARNING: page will be invalidated, parent might be invalidated
		oct.page_free_counter++;

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

	Node split_subtree (Page pages[], Page* splitpage, Page* srcpage, Node splitnode) {
		assert((splitnode & LEAF_BIT) == 0);

		// alloc node in splitpage
		uintptr_t children_ptr = splitpage->alloc_node();

		// copy children nodes
		children_t children;
		memcpy(children, srcpage->nodes[splitnode], sizeof(children_t));

		// free children node
		srcpage->free_node(splitnode);

		for (int i=0; i<8; ++i) {
			Node child = children[i];

			// move over children to splitnode if node was farptr
			if (child & FARPTR_BIT) {
				srcpage->remove_child(&pages[child & ~FARPTR_BIT]);
				splitpage->add_child(&splitpage->nodes[children_ptr][i], &pages[child & ~FARPTR_BIT]);
			}

			splitpage->nodes[children_ptr][i] = child & (LEAF_BIT|FARPTR_BIT) ?
				child :
				split_subtree(pages, splitpage, srcpage, child);
		}

		return (Node)children_ptr;
	}
	void split_page (WorldOctree& oct, Page* page) {
		
		// find splitnode
		PageSplitter ps = { page };
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
		split_subtree(&oct.pages[0], childpage, page, *ps.split_node);
		
		// set childpage farptr in tmppage
		*ps.split_node = (Node)(oct.pages.indexof(childpage) | FARPTR_BIT);
		page->add_child(ps.split_node, childpage);

		oct.page_split_counter++;
	}

	void recurse_free_subpages (Page* page, std::vector<Page*>* pages_to_free) {
		pages_to_free->push_back(page);
		
		auto* cur = page->info.children_ptr;
		page->info.children_ptr = nullptr;
		while (cur) {
			auto* child = cur;
			cur = child->info.sibling_ptr;

			child->info.sibling_ptr = nullptr;
			child->info.farptr_ptr = nullptr;

			recurse_free_subpages(child, pages_to_free);
		}
	}
	void recurse_free_subtree (Page pages[], Page* page, Node node, std::vector<Page*>* pages_to_free) {
		assert((node & LEAF_BIT) == 0);

		if (node & FARPTR_BIT) {
			recurse_free_subpages(&pages[node & ~FARPTR_BIT], pages_to_free);
			return;
		}

		for (int i=0; i<8; ++i) {
			auto child = page->nodes[node][i];
			if ((child & LEAF_BIT) == 0) {
				recurse_free_subtree(pages, page, child, pages_to_free);
			}
		}
		
		page->free_node(node);
	}
	Page* free_subtree (WorldOctree& oct, Page* page, Node node) {
		
		std::vector<Page*> pages_to_free;
		pages_to_free.reserve(128);
		
		recurse_free_subtree(&oct.pages[0], page, node, &pages_to_free);

		// freeing pages in reverse order ensures that none of the pages to be freed are migrated, this would invlidate the pointer
		std::sort(pages_to_free.begin(), pages_to_free.end(), [] (Page* a, Page* b) {
			return std::greater<uintptr_t>()((uintptr_t)a, (uintptr_t)b); // decending ptr addresses
		});

		for (auto* p : pages_to_free) {
			Page* lastpage = oct.pages.allocator[(uint16_t)oct.pages.allocator.size() -1];

			// enable a page ptr to be preserved through free_page, which might invalidate it, required by caller
			if (page == lastpage) {
				page = p;
			}

			oct.pages.free_page(p);
			oct.page_free_counter++;
		}

		return page;
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
					assert(node_ptr != (Node)INTNULL);

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

		Node old_val = *write_node;
		Page* page = page_from_node(write_node);

		// do the write
		*write_node = (Node)(LEAF_BIT | val);

		// free subtree nodes if subtree was collapsed
		if ((old_val & LEAF_BIT) == 0) {
			page = free_subtree(oct, page, old_val);

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
			
			if (((node & LEAF_BIT)==0 || (node & ~LEAF_BIT) > (oct.debug_draw_air ? B_NULL : B_AIR)) && scale <= oct.debug_draw_octree_max)
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

		clog("WorldOctree::add_chunk:  octree_write: %f ms", at * 1000);
	}

	void WorldOctree::remove_chunk (Chunk& chunk) {
		int3 pos = chunk.coord * CHUNK_DIM;

		auto a = Timer::start();
		octree_write(*this, pos, CHUNK_DIM_SHIFT, B_NULL);
		auto at = a.end();

		clog("WorldOctree::remove_chunk:  octree_write: %f ms", at * 1000);
	}

	void WorldOctree::update_block (Chunk& chunk, int3 bpos, block_id id) {
		int3 pos = chunk.coord * CHUNK_DIM;
		pos += bpos;

		auto a = Timer::start();
		octree_write(*this, pos, 0, id);
		auto at = a.end();

		clog("WorldOctree::update_block:  octree_write: %f ms", at * 1000);
	}
}
