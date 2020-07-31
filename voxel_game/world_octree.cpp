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

		ImGui::Text("pages: %d (%d unused)", active_pages, (int)pages.size() - active_pages);
		
		int total_nodes = (int)(pages.size() * PAGE_NODES);
		int active_nodes = 0;
		for (auto* page : pages) {
			active_nodes += page->count;
		}

		ImGui::Text("nodes: %d active / %d allocated  %6.2f%% active",
			active_nodes, total_nodes,
			(float)active_nodes / total_nodes * 100);
		
		imgui_pop();
	}

	//
	OctreeNode recurse_page_from_subtree (OctreePage& newpage, OctreePage const& srcpage, OctreeNode srcnode) {
		assert((srcnode & LEAF_BIT) == 0);

		uint32_t children_ptr = newpage.alloc_node();

		auto& old_children = srcpage.nodes[ srcnode ].children;
		for (int i=0; i<8; ++i) {
			newpage.nodes[ children_ptr ].children[i] = (OctreeNode)( (old_children[i] & (LEAF_BIT|FARPTR_BIT)) ?
				old_children[i] :
				recurse_page_from_subtree(newpage, srcpage, old_children[i]) );
		}

		return (OctreeNode)children_ptr;
	}
	OctreePage* page_from_subtree (WorldOctree& oct, OctreePage const& srcpage, OctreeNode subroot) {
		OctreePage* newpage = oct.allocator.alloc();
		newpage->count = 0;
		newpage->parent_page = INTNULL;
		newpage->freelist = INTNULL;

		recurse_page_from_subtree(*newpage, srcpage, subroot);

		return newpage;
	}

	struct Merger {
		WorldOctree& oct;
		OctreePage& newpage;
		uint32_t merge_page;

		OctreeNode recurse (OctreePage& srcpage, OctreeNode srcnode) {
			assert((srcnode & LEAF_BIT) == 0);

			uint32_t children_ptr = newpage.alloc_node();

			auto& old_children = srcpage.nodes[ srcnode ].children;
			for (int i=0; i<8; ++i) {
				
				bool is_farptr = old_children[i] & FARPTR_BIT;
				auto farptr = old_children[i] & ~FARPTR_BIT;
				
				OctreeNode val;

				if (old_children[i] & LEAF_BIT || (is_farptr && farptr != merge_page)) {
					val = old_children[i];
				} else {
					if (is_farptr) {
						val = recurse(*oct.pages[farptr], (OctreeNode)0);
					} else {
						val = recurse(srcpage, old_children[i]);
					}
				}

				newpage.nodes[ children_ptr ].children[i] = val;
			}

			return (OctreeNode)children_ptr;
		}
	};
	void merge_with_parent (WorldOctree& oct, OctreePage* page, OctreePage*& parent_page) {
		OctreePage* newpage = oct.allocator.alloc();

		uint32_t child_page = (uint32_t)(page - oct.pages[0]);

		Merger m = { oct, *newpage, child_page };
		m.recurse(*parent_page, (OctreeNode)0);

		oct.allocator.free(parent_page);
		oct.allocator.free(page);

		// free page

		parent_page = newpage;
	}
	void checked_merge_with_parent (WorldOctree& oct, OctreePage*& page) {
		if (page->parent_page == (uint32_t)-1)
			return; // root page

		OctreePage*& parent_page = oct.pages[ page->parent_page ];

		uint32_t sum = page->count - parent_page->count;

		if (sum <= PAGE_MERGE_THRES) {
			merge_with_parent(oct, page, parent_page);

			assert(parent_page->count == sum);
		}
	}

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
		OctreePage& page;

		int target_page_size;

		OctreeNode* split_node;
		int split_node_closeness = INT_MAX;

		uint32_t find_split_node_recurse (OctreeNode& node) {
			assert((node & LEAF_BIT) == 0);

			int subtree_size = 1; // count ourself

			// cound children subtrees
			auto& children = page.nodes[ node ].children;
			for (int i=0; i<8; ++i) {
				if ((children[i] & (LEAF_BIT|FARPTR_BIT)) == 0)
					subtree_size += find_split_node_recurse(children[i]);
			}

			// keep track of node that splits the node in half the best
			int closeness = abs(subtree_size - target_page_size);
			if (closeness < split_node_closeness) {
				split_node = &node;
				split_node_closeness = closeness;
			}

			return subtree_size;
		}
	};

	void split_page (WorldOctree& oct, OctreePage** page) {
		PageSplitter ps = { **page };
		ps.target_page_size = (*page)->count / 2;

		OctreeNode root = (OctreeNode)0;

		ps.find_split_node_recurse(root);

		assert((*ps.split_node & LEAF_BIT) == 0);
		if (ps.split_node == &root) {
			// edge cases that are not allowed to happen because a split of the root node do not actually split the page in a useful way
			//  this should be impossible with an page subtree depth > 2, I think. (ie. root -> middle -> leaf), which a PAGE_NODES > 
			// But if the page is in an uncompacted state this might still happen (because the actual active nodes might be as low as 2
			assert(false);
		}

		OctreeNode subroot = *ps.split_node;
		*ps.split_node = (OctreeNode)((uint32_t)oct.pages.size() | FARPTR_BIT);

		auto* childpage = page_from_subtree(oct, **page, subroot);
		auto* newpage = page_from_subtree(oct, **page, root);
		childpage->parent_page = (uint32_t)( page - &oct.pages[0] );
		newpage->parent_page = (*page)->parent_page;

		oct.allocator.free(*page);

		*page = newpage;
		oct.pages.push_back(childpage);
	}

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

	// octree write, writes a single voxel at a desired pos, scale to be a particular val
	// this decends the octree from the root and inserts or deletes nodes when needed
	// (ex. writing a 4x4x4 area to be air will delete the nodes of smaller scale contained)
	// NOTE: nodes are deleted by becoming unreachable and nodes are inserted at the end, compact page should be called for all touched pages at some point
	// NOTE: pages are split as needed if they become full
	void octree_write (WorldOctree& oct, int3 pos, int scale, block_id val) {
		pos -= oct.root_pos;
		if (any(pos < 0 || pos >= (1 << oct.root_scale)))
			return;

	restart_decent:
		// start with root node
		int cur_scale = oct.root_scale;
		OctreePage** pcur_page = &oct.pages[0];
		OctreePage* cur_page = oct.pages[0];
		OctreeNode node_ptr = (OctreeNode)0;
		OctreeNode* child_node = nullptr;

		struct Path {
			OctreeNode* node;
			OctreePage** page;
		};
		Path node_path[MAX_DEPTH];

		for (;;) {
			// get child node that contains target node
			cur_scale--;
			
			auto& children = cur_page->nodes[node_ptr].children;

			int child_idx = get_child_index(pos, cur_scale);

			child_node = &children[child_idx];

			// keep track of node path for collapsing of same type octree nodes (these get invalidated on split page, which do never need to collapse)
			node_path[cur_scale] = { child_node, pcur_page };

			if (cur_scale == scale) {
				// reached target octree depth
				break;
			}

			if ((*child_node & LEAF_BIT) == 0) {
				// recurse normally

				node_ptr = *child_node;

				if (node_ptr & FARPTR_BIT) {
					pcur_page = &oct.pages[ node_ptr & ~FARPTR_BIT ];
					cur_page = *pcur_page;
					node_ptr = (OctreeNode)0;
				}
			} else {
				//// Split node into 8 children of same type

				block_id leaf_val = (block_id)(*child_node & ~LEAF_BIT);

				if (leaf_val == val) {
					// do not split node if leaf is already the correct type just at a higher scale
					break;
				}

				if (cur_page->count == PAGE_NODES) {
					// page full

					// Do split page which will have created 2 new pages with about half the nodes out of the full page
					split_page(oct, pcur_page);

					// cur_child and cur_page were invlidated, I'm not sure how to best fix the fact that the nodes we were iterating though have moved into new pages
					// I might attempt to keep track of cur_child and fix it in split_page, or redecend from the root of the cur page,
					// but a total redecent should always work, so let's just do that with a goto
					goto restart_decent;
				}

				node_ptr = (OctreeNode)cur_page->alloc_node();

				// write ptr to children into this nodes slot in parents children array
				*child_node = node_ptr;

				// alloc and init children for node
				auto& children = cur_page->nodes[node_ptr].children;
				for (int i=0; i<8; ++i) {
					children[i] = (OctreeNode)(LEAF_BIT | leaf_val);
				}
			}
		}

		auto* write_node = child_node;

		{ // collapse nodes containing same leaf nodes
			for (int scale=cur_scale;; scale++) {
				auto path = node_path[scale];

				if (scale == oct.root_scale-1) {
					write_node = write_node = path.node;
				}

				auto* siblings = (OctreeNode*)((uintptr_t)path.node & ~(sizeof(OctreeChildren) -1));
				
				for (int i=0; i<8; ++i) {
					auto& sibling = siblings[i];
					if (&sibling != path.node && sibling != (LEAF_BIT | val)) {
						write_node = path.node;
						goto end; // embrace the goto
					}
				}
			}
			end:;
		}

		// do the write
		*write_node = (OctreeNode)(LEAF_BIT | val);

		//for (int scale=cur_scale; scale<oct.root_scale; scale++) {
		//	checked_merge_with_parent(oct, *node_path[scale].page);
		//}
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
		int active_pages = 0;
		int active_nodes = 0;

		OctreeChildren& get_node (OctreeNode node, OctreePage*** cur_page) {
			if (node & FARPTR_BIT) {
				*cur_page = &oct.pages[ node & ~FARPTR_BIT ];
				node = (OctreeNode)0;
				active_pages++;
			}
			return (**cur_page)->nodes[node];
		}
		
		void recurse_draw (OctreePage** cur_page, OctreeNode node, int3 pos, int scale) {
			float size = (float)(1 << scale);

			auto col = cols[scale % ARRLEN(cols)];
			if (oct.debug_draw_pages)
				col = cols[(cur_page - &oct.pages[0]) % ARRLEN(cols)];
			
			if (((node & LEAF_BIT)==0 || (node & ~LEAF_BIT) > B_NULL) && scale <= oct.debug_draw_octree_max)
				debug_graphics->push_wire_cube((float3)pos + 0.5f * size, size * 0.999f, col);

			if ((node & LEAF_BIT) == 0) {
				active_nodes++;

				OctreeChildren& children = get_node(node, &cur_page);
				int child_scale = scale - 1;

				if (child_scale >= oct.debug_draw_octree_min) {
					for (int i=0; i<8; ++i) {
						int3 child_pos = pos + (int3(i & 1, (i >> 1) & 1, (i >> 2) & 1) << child_scale);

						recurse_draw(cur_page, children.children[i], child_pos, child_scale);
					}
				}
			}
		}
	};

	void debug_draw (WorldOctree& oct) {
		RecurseDrawer rd = { oct };
		rd.recurse_draw(nullptr, (OctreeNode)(0 | FARPTR_BIT), oct.root_pos, oct.root_scale);

		oct.active_pages = rd.active_pages;
		oct.active_nodes = rd.active_nodes;
	}

	void WorldOctree::pre_update (Player const& player) {
		if (pages.size() == 0) {
			pages.push_back( allocator.alloc() );

			pages[0]->count = 0;
			pages[0]->parent_page = INTNULL;
			pages[0]->freelist = INTNULL;

			auto& root = pages[0]->nodes[ pages[0]->alloc_node() ];
			for (int i=0; i<8; ++i) {
				root.children[i] = (OctreeNode)(LEAF_BIT | B_NULL);
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
