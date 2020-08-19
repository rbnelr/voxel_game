#include "stdafx.hpp"
#include "svo.hpp"
#include "voxel_backend.hpp"
#include "world_generator.hpp"
#include "player.hpp"
#include "graphics/debug_graphics.hpp"

namespace svo {
	Node page_from_subtree (Page pages[], Page* dstpage, Page* srcpage, Node srcnode) {
		assert((srcnode & LEAF_BIT) == 0);

		uintptr_t children_ptr = dstpage->alloc_node();

		for (int i=0; i<8; ++i) {
			auto& child = srcpage->nodes[srcnode].children[i];

			if (child & FARPTR_BIT) {
				dstpage->add_child(&dstpage->nodes[children_ptr].children[i], &pages[child & ~FARPTR_BIT]);
			}
			
			dstpage->nodes[children_ptr].children[i] = child & (LEAF_BIT|FARPTR_BIT) ?
				child :
				page_from_subtree(pages, dstpage, srcpage, child);
		}

		return (Node)children_ptr;
	}

	void SVO::merge (Page* parent, Page* child) {
		ZoneScopedN("world_octree::merge");

		// remove children from child so they can be added to parent node
		child->remove_all_children();

		// create merged page, by adding nodes of child to parent, then replacing the farptr with the new subtree
		*child->info.farptr_ptr = page_from_subtree(allocator[0], parent, child, (Node)0);

		parent->remove_child(child);
		
		free_page(child);
	}
	void SVO::try_merge (Page* page) {
		if (!page->info.farptr_ptr)
			return; // root page

		Page* parent = page_from_node(page->info.farptr_ptr);

		uintptr_t sum = page->info.count + parent->info.count;

		if (sum <= PAGE_MERGE_THRES) {
			merge(parent, page);
		}
	}

	struct DescentStack {
		Node*	node;
		Page*	page;
	};

	struct PageSplitter {
		Page* page;

		int target_page_size;

		Node* split_node;
		uint8_t split_node_scale;
		int split_node_closeness = INT_MAX;

		uint16_t find_split_node_recurse (Node* node, uint8_t scale) {
			assert((*node & LEAF_BIT) == 0);

			uint16_t subtree_size = 1; // count ourself

			// cound children subtrees
			for (int i=0; i<8; ++i) {
				auto& children = page->nodes[*node].children;
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
				Node& child = srcpage->nodes[splitnode].children[i];

				// move over children to splitnode if node was farptr
				if (child & FARPTR_BIT) {
					srcpage->remove_child(&pages[child & ~FARPTR_BIT]);
					splitpage->add_child(&splitpage->nodes[children_ptr].children[i], &pages[child & ~FARPTR_BIT]);
				}

				if (stack[scale-1] == &child) {
					stack[scale-1] = &splitpage->nodes[children_ptr].children[i];
				} else {
					for (uint16_t i=0; i<MAX_DEPTH; ++i) {
						if (stack[scale] == &child) {
							assert(false);
						}
					}
				}

				splitpage->nodes[children_ptr].children[i] = child & (LEAF_BIT|FARPTR_BIT) ?
					child :
					split_subtree(child, scale-1);
			}

			// free children node
			srcpage->free_node(splitnode);

			return (Node)children_ptr;
		}
	};
	Page* SVO::split_page (Page* page, Node* stack[], bool active_page) {
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
		Page* childpage = alloc_page(active_page);
		childpage->info.scale = ps.split_node_scale;

		// create childpage from subtree
		_SplitSubtree{ allocator[0], childpage, page, stack }.split_subtree(*ps.split_node, ps.split_node_scale);
		
		// set childpage farptr in tmppage
		*ps.split_node = (Node)(allocator.indexof(childpage) | FARPTR_BIT);
		page->add_child(ps.split_node, childpage);

		return childpage;
	}

	void recurse_free_subtree (SVO& oct, Page* page, Node node) {
		assert((node & LEAF_BIT) == 0);

		if (node & FARPTR_BIT) {
			auto* page = oct.allocator[node & ~FARPTR_BIT];
			oct.free_page(page);
			return;
		}

		for (int i=0; i<8; ++i) {
			auto child = page->nodes[node].children[i];
			if ((child & LEAF_BIT) == 0) {
				recurse_free_subtree(oct, page, child);
			}
		}
		
		page->free_node(node);
	}
	void SVO::free_subtree (Page* page, Node node) {
		ZoneScopedN("world_octree::free_subtree");
		
		recurse_free_subtree(*this, page, node);
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
	void SVO::octree_write (int3 pos, int scale, Node val) {
		ZoneScopedN("world_octree::octree_write");

		pos -= root_pos;
		if (any(pos < 0 || pos >= (1 << root_scale)))
			return;

		// start with root node
		int cur_scale = root_scale;

		Node* stack[MAX_DEPTH];
	#if !NDEBUG
		memset(stack, 0, sizeof(stack));
	#endif

		Node* siblings = allocator[0]->nodes[0].children;

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
					page = allocator[ node_ptr & ~FARPTR_BIT ];
					node_ptr = (Node)0;
				}

				siblings = page->nodes[node_ptr].children;
			} else {
				//// Split node into 8 children of same type

				Node leaf = *child_node;

				if (leaf == val) {
					// do not split node if leaf is already the correct type just at a higher scale
					break;
				}

				if (page->nodes_full()) {
					auto* childpage = split_page(page, stack);

					child_node = stack[cur_scale];
					page = page_from_node(child_node);
				}

				Node node_ptr = (Node)page->alloc_node();

				// write ptr to children into this nodes slot in parents children array
				*child_node = node_ptr;

				siblings = page->nodes[node_ptr].children;

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

				if (scale == root_scale-1) {
					break;
				}

				Node* siblings = siblings_from_node(write_node)->children;
			
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
			free_subtree(page, old_val);
		} else if (wrote_subpage) {
			Page* childpage = allocator[val & ~FARPTR_BIT];

			page->add_child(write_node, childpage);

			page = childpage;
		}

		if (collapsed || wrote_subpage) {

			try_merge(page);
		}
	}

	block_id SVO::octree_read (int3 pos) {
		ZoneScopedN("world_octree::octree_write");

		pos -= root_pos;
		if (any(pos < 0 || pos >= (1 << root_scale)))
			return B_NULL;

		// start with root node
		int cur_scale = root_scale;

		Node* siblings = allocator[0]->nodes[0].children;

		for (;;) {
			cur_scale--;

			// get child node that contains target node
			int child_idx = get_child_index(pos, cur_scale);

			Node* child_node = siblings + child_idx;
			Page* page = page_from_node(child_node);

			if ((*child_node & LEAF_BIT) != 0) {
				return (block_id)(*child_node & ~LEAF_BIT);
			}

			Node node_ptr = *child_node;

			if (node_ptr & FARPTR_BIT) {
				page = allocator[ node_ptr & ~FARPTR_BIT ];
				node_ptr = (Node)0;
			}

			siblings = page->nodes[node_ptr].children;
		}
	}

	void SVO::update_block (int3 pos, block_id id) {
		ZoneScopedN("svo_update_block");

		octree_write(pos, 0, (Node)(LEAF_BIT | id));
	}

	// move octree root along with player through world, so that ideally all desired visible parts of the world are contained
	void update_root (SVO& oct, float3 player_pos) {
		int shift = oct.root_scale - 1;
		float half_root_scalef = (float)(1 << shift);
		
		int3 center = roundi((player_pos + oct.root_move_hister) / half_root_scalef);
		
		int3 pos = (center - 1) << shift;
		
		if (equal(pos, oct.root_pos))
			return;
		
		ZoneScoped;

		int3 move = (pos - oct.root_pos) >> shift;
		
		oct.root_move_hister = (float3)move * half_root_scalef * ROOT_MOVE_HISTER;

		Page* rootpage = oct.allocator[0];

		// copy old children
		NodeChildren old_children;
		memcpy(&old_children, rootpage->nodes[0].children, sizeof(old_children));
		
		for (int i=0; i<8; ++i) {
			int3 child = int3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
		
			child += move;
		
			int child_indx = child.x | (child.y << 1) | (child.z << 2);
			
			if (any(child < 0 || child > 1)) {
				// root moved over this area, no nodes loaded yet
				rootpage->nodes[0].children[i] = (Node)(LEAF_BIT | B_NULL);
			} else {
				// copy old children to new slot
				rootpage->nodes[0].children[i] = old_children.children[child_indx];
				old_children.children[child_indx] = (Node)0;
			}
		}
		
		// free old subtrees that were dropped (should not really happen all that often because that means our root node is too small)
		for (int i=0; i<8; ++i) {
			if (old_children.children[i] != 0 && (old_children.children[i] & LEAF_BIT) == 0) {
				oct.free_subtree(rootpage, old_children.children[i]);
			}
		}
		
		oct.root_pos = pos;
	}

	static constexpr int3 children_pos[8] = {
		int3(0,0,0),
		int3(1,0,0),
		int3(0,1,0),
		int3(1,1,0),
		int3(0,0,1),
		int3(1,0,1),
		int3(0,1,1),
		int3(1,1,1),
	};
	static constexpr float3 corners[8] = {
		float3(0,0,0),
		float3(1,0,0),
		float3(0,1,0),
		float3(1,1,0),
		float3(0,0,1),
		float3(1,0,1),
		float3(0,1,1),
		float3(1,1,1),
	};

	bool can_collapse (Node* children) {
		if ((children[0] & LEAF_BIT) == 0)
			return false;

		for (int i=1; i<8; ++i) {
			if (children[i] != children[0])
				return false;
		}

		return true;
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

	void recurse_draw (SVO& oct, Page* cur_page, Node node, int3 pos, int scale) {
		float size = (float)(1 << scale);

		auto col = cols[scale % ARRLEN(cols)];
		if (oct.debug_draw_page_color || oct.debug_draw_pages)
			col = cols[oct.allocator.indexof(cur_page) % ARRLEN(cols)];
			
		bool draw = ((node & LEAF_BIT)==0 || (node & ~LEAF_BIT) > (oct.debug_draw_air ? B_NULL : B_AIR)) && scale <= oct.debug_draw_octree_max;
		if (oct.debug_draw_pages ? (node & FARPTR_BIT) : draw)
			debug_graphics->push_wire_cube((float3)pos + 0.5f * size, size * 0.999f, col);

		if ((node & LEAF_BIT) == 0) {
			
			if (node & FARPTR_BIT) {
				cur_page = oct.allocator[node & ~FARPTR_BIT];
				node = (Node)0;
			}

			Node* children = cur_page->nodes[node].children;
			int child_scale = scale - 1;

			if (child_scale >= oct.debug_draw_octree_min) {
				for (int i=0; i<8; ++i) {
					int3 child_pos = pos + (int3(i & 1, (i >> 1) & 1, (i >> 2) & 1) << child_scale);

					recurse_draw(oct, cur_page, children[i], child_pos, child_scale);
				}
			}
		}
	}

	SVO::SVO () {
		auto* rootpage = alloc_page();
		rootpage->info.scale = root_scale;
		assert(allocator.indexof(rootpage) == 0);

		auto& root = rootpage->nodes[ rootpage->alloc_node() ];
		for (int i=0; i<8; ++i) {
			root.children[i] = (Node)(LEAF_BIT | B_NULL);
		}
	}

	Node SVO::chunk_to_octree (block_id* blocks) {
		ZoneScopedN("svo_chunk_to_octree");

		//int3 pos = chunk.coord * CHUNK_DIM;

		Page* chunk_page = alloc_page(false);
		chunk_page->info.scale = CHUNK_SCALE;

		Node* node_stack[MAX_DEPTH];
	#if !NDEBUG
		memset(node_stack, 0, sizeof(node_stack));
	#endif

		struct Stack {
			int3 pos;
			int child_indx;
		};
		Stack stack[MAX_DEPTH];

		uint16_t scale = CHUNK_SCALE-1;

		Node root = (Node)(FARPTR_BIT | allocator.indexof(chunk_page));

		node_stack[CHUNK_SCALE] = &root;
		stack[CHUNK_SCALE] = { 0, 0 };

		node_stack[scale] = chunk_page->nodes[ chunk_page->alloc_node() ].children;
		stack[scale] = { 0, 0 };

		for (int i=0; i<8; ++i) {
			chunk_page->nodes[0].children[i] = NULLNODE;
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
					uint16_t children_ptr = (uint16_t)(((char*)children - (char*)&page->nodes) / (sizeof(NodeChildren)));

					node_stack[scale][ stack[scale].child_indx ] = children[0];

					if (children_ptr == 0) {
						free_page(page, false);
					} else {
						page->free_node(children_ptr);
					}
				}

				if (scale == CHUNK_SCALE)
					break;

			} else {

				int3 child_pos = pos + (children_pos[child_indx] << scale);

				if (scale == 0) {
					auto bid = blocks[child_pos.z * CHUNK_SIZE*CHUNK_SIZE + child_pos.y * CHUNK_SIZE + child_pos.x];

					*node = (Node)(LEAF_BIT | bid);
				} else {
					// Push
					if (page->nodes_full()) {
						split_page(page, node_stack, false);

						children	= node_stack[scale];
						node = &children[child_indx];
						page = page_from_node(children);
					}

					uint16_t nodei = page->alloc_node();
					auto& child_children = page->nodes[nodei];

					*node = (Node)nodei;

					scale--;
					stack[scale] = { child_pos, 0 };
					node_stack[scale] = child_children.children;

					// init nodes to leaf nodes so future split_page won't fail if we had this uninitialized
					for (int i=0; i<8; ++i) {
						child_children.children[i] = NULLNODE;
					}

					continue;
				}
			}

			stack[scale].child_indx++;
		}

		return root;
	}

	struct ChunkLoader {
		SVO& svo;
		float3 player_pos;
		Voxels& voxels;

		struct Chunk {
			int3 coord;
			float dist;
		};
		std::vector<Chunk> chunks_to_load;
		std::vector<int3> chunks_to_unload;

		float calc_closest_dist (float3 pos, float size) {
			float3 pos_rel = player_pos - pos;

			float3 clamped = clamp(pos_rel, 0, size);
			return length(clamped - pos_rel);
		}
		float calc_furthest_dist (float3 pos, float size) {
			float3 pos_rel = player_pos - pos;

			float max_dist_sqr = -INF;
			for (auto corner : corners) {
				float3 p = corner * size;
				max_dist_sqr = max(max_dist_sqr, length_sqr(p - pos_rel));
			}

			return sqrt(max_dist_sqr);
		}

		void recurse_chunk_loading (Page* page, Node node, int3 pos, int scale) {
			if (node & FARPTR_BIT) {
				page = svo.allocator[node & ~FARPTR_BIT];
				node = (Node)0;
			}

			// need to recurse into areas that do not exist in the octree yet
			// -> children_nodes will be nullptr in that case
			Node* children_nodes = node & LEAF_BIT ? nullptr : page->nodes[node].children;

			int child_scale = scale - 1;

			for (int i=0; i<8; ++i) {
				int3 child_pos = pos + (children_pos[i] << child_scale);

				// need to recurse into areas that do not exist in the octree yet
				// just pretend the child nodes were the same (leaf) node as the parent
				Node child_node = children_nodes ? children_nodes[i] : node;

				bool is_loaded = child_node != (Node)(LEAF_BIT | B_NULL);

				float3 posf = (float3)child_pos;
				float sizef = (float)(1 << child_scale);

				float dist = calc_closest_dist(posf, sizef);
				bool want_loaded = dist <= voxels.load_radius;

				if (child_scale == CHUNK_SCALE) {

					int3 chunk_coord = child_pos + svo.root_pos;

					if (want_loaded) {
						//bool pending = svo.is_chunk_load_queued(chunks, chunk_coord);
						//debug_graphics->push_wire_cube((float3)(child_pos + svo.root_pos) + 0.5f * sizef, sizef * 0.999f,
						//	is_loaded ? srgba(30,30,30,120) : (pending ? lrgba(0,0,1,1) : lrgba(1,0,0,1)));

						if (!is_loaded && !svo.is_chunk_load_queued(voxels, chunk_coord)) {
							// chunk not generated yet
							chunks_to_load.push_back({ chunk_coord, dist });
						}
					} else {
						if (is_loaded) {

							float fardist = calc_furthest_dist(posf, sizef);
							if (fardist >= voxels.load_radius + voxels.unload_hyster) {
								// unload whole subtree
								chunks_to_unload.push_back(chunk_coord);
							}

						}
					}
				} else {
					if (want_loaded)
						recurse_chunk_loading(page, child_node, child_pos, child_scale);
				}
			}
		}
	};

	void SVO::recurse_add_active_pages (Page* page) {
		active_pages.insert(page);

		Page* child = page->info.children_ptr;
		while (child) {
			recurse_add_active_pages(child);
			child = child->info.sibling_ptr;
		}
	}

	void SVO::update_chunk_loading (Voxels& voxels, WorldGenerator& world_gen, float3 player_pos) {
		ZoneScoped;

		update_root(*this, player_pos);

		ChunkLoader cl = { *this, player_pos - (float3)root_pos, voxels } ;
		{
			ZoneScopedN("recurse_chunk_loading");
			cl.recurse_chunk_loading(nullptr, (Node)(0 | FARPTR_BIT), 0, root_scale);
		}

		{
			ZoneScopedN("chunk unloading");
			for (auto chunk : cl.chunks_to_unload) {
				octree_write(chunk, CHUNK_SCALE, (Node)(LEAF_BIT | B_NULL));
			}
		}

		{
			ZoneScopedN("std::sort chunks_to_load");

			std::sort(cl.chunks_to_load.begin(), cl.chunks_to_load.end(), [] (ChunkLoader::Chunk& l, ChunkLoader::Chunk& r) {
				return std::less<float>()(l.dist, r.dist);
			});
		}

		{
			ZoneScopedN("queue chunk loads");

			int count = min((int)cl.chunks_to_load.size(), voxels.cap_chunk_load);

			int i=0;
			background_threadpool.jobs.push_multiple([&] () -> std::unique_ptr<WorldgenJob> {
				if (i >= count)
					return nullptr;

				ZoneScopedN("push WorldgenJob");

				auto chunk = cl.chunks_to_load[i++];

				pending_chunks.emplace(chunk.coord);

				return std::make_unique<WorldgenJob>(chunk.coord, this, &world_gen);
			});
		}

		{
			ZoneScopedN("finish chunkgen jobs");

			std::vector< std::unique_ptr<ThreadingJob> > jobs (voxels.cap_chunk_load);

			size_t count = background_threadpool.results.pop_multiple(&jobs[0], voxels.cap_chunk_load);

			for (size_t i=0; i<count; ++i) {
				jobs[i]->finalize();
			}
		}

		if (debug_draw_octree) {
			ZoneScopedN("SVO::debug_draw");

			recurse_draw(*this, nullptr, (Node)(0 | FARPTR_BIT), root_pos, root_scale);
		}
	}
}
