#include "stdafx.hpp"
#include "svo.hpp"
#include "voxel_system.hpp"
#include "world_generator.hpp"
#include "player.hpp"
#include "graphics/debug_graphics.hpp"

namespace svo {
#if 0
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

#endif

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

	uint16_t Chunk::alloc_node (SVO& svo, StackNode* stack) {
		if (alloc_ptr >= commit_ptr) {

			if (nodes == tinydata) {
				nodes = svo.node_allocator.alloc()->nodes;

				commit_pages(nodes, MAX_NODES*sizeof(Node));
				commit_ptr = MAX_NODES;

			#if DBG_MEMSET
				memset(nodes, DBG_MEMSET_VAL, MAX_NODES*sizeof(Node));
			#endif

				memcpy(nodes, tinydata, sizeof(tinydata));

				if (stack) { // fixup stack after we invalidated some of the nodes
					for (int i=0; i<MAX_DEPTH; ++i) {
						uintptr_t indx = stack[i].node - tinydata;
						if (indx < ARRLEN(tinydata)) {
							stack[i].node = nodes + indx;
						}
					}
				}

			#if DBG_MEMSET
				memset(tinydata, DBG_MEMSET_VAL, sizeof(tinydata));
			#endif
			}

		}
		return alloc_ptr++;
	}

	int get_child_index (int3 pos, int scale) {
		//// Subpar asm generated for a lot of these, at least from what I can tell, might need to look into this if I octree decent actually becomes a bottleneck
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

	bool can_collapse (Node* node) {
		if (node[0].leaf_mask != 0xff)
			return false; // not all leafs, cant collapse

		for (int i=1; i<8; ++i) {
			if (node->children[i] != node->children[0])
				return false; // not all leafs equal, cant collapse
		}

		return true;
	}

	void SVO::octree_write (int3 pos, int scale, uint16_t val) {
		ZoneScopedN("world_octree::octree_write");

		pos -= root->pos;
		if (any(pos < 0 || pos >= (1 << root->scale))) {
			// writes outside the root node are not possible, should never be attempted by caller
			assert(false);
		}

		// start with root node
		int cur_scale = root->scale;
		Node* node = &root->nodes[0];

		Chunk* chunk = root;

		StackNode stack[MAX_DEPTH];
	#if DBG_MEMSET
		memset(stack, DBG_MEMSET_VAL, sizeof(stack));
	#endif

		assert(scale < root->scale);

		for (;;) {
			cur_scale--;
			assert(cur_scale >= 0);

			// get child node that contains target node
			int child_idx = get_child_index(pos, cur_scale);

			// update stack
			stack[cur_scale].node = node;
			stack[cur_scale].child_indx = child_idx;

			if (cur_scale == scale) {
				// reached target octree depth
				break;
			}

			uint16_t* child_node = &node->children[child_idx];

			bool child_leaf = node->leaf_mask & (1u << child_idx);

			if (!child_leaf) { 
				// recurse into child node
				node = &chunk->nodes[*child_node];

			} else {
				//// Split node into 8 children of same type

				uint16_t leaf = *child_node;

				if (chunk == root && leaf != B_NULL) {
					// leafs in root svo (ie. svo of chunks) are chunks, so recurse into chunk nodes
					assert(*child_node != 0); // trying to write voxel into unloaded chunk, should never be attempted by caller

					chunk = chunk_allocator[leaf];
					node = chunk->nodes;
				}

				//if (leaf == val) {
				//	// write would be a no-op because the octree already has the desired value at that voxel
				//	return;
				//}

				uint16_t node_ptr = chunk->alloc_node(*this, stack);

				// write ptr to children into this nodes slot in parents children array
				stack[cur_scale].node->children[child_idx] = node_ptr;
				stack[cur_scale].node->leaf_mask ^= (1u << child_idx);

				// init and recurse into new node
				node = &chunk->nodes[node_ptr];

				node->leaf_mask = 0xff;
				// alloc and init children for node
				for (int i=0; i<8; ++i) {
					node->children[i] = leaf;
				}

				chunk->gpu_dirty = true;
			}
		}

		// do the write
		stack[cur_scale].node->children[ stack[cur_scale].child_indx ] = val;
		stack[cur_scale].node->leaf_mask |= 1u << stack[cur_scale].child_indx;

		chunk->gpu_dirty = true;

		// collapse octree nodes with same leaf values by walking up the stack, stop at chunk root or svo root or if can't collapse
		for (	auto i = cur_scale;
				i != CHUNK_SCALE-1 && i < root->scale-1 && can_collapse(stack[i].node);
				++i) {

			stack[i+1].node->children[ stack[i+1].child_indx ] = stack[i].node->children[0];
			stack[i+1].node->leaf_mask |= 1u << stack[i+1].child_indx;

			// free node
			chunk->dead_count++;
		}

	}

	void SVO::chunk_to_octree (Chunk* chunk, block_id* blocks) {
		ZoneScopedN("svo_chunk_to_octree");

		StackNode stack[MAX_DEPTH];

		Node dummy;
		stack[CHUNK_SCALE] = { &dummy, 0, 0 };

		uint16_t scale = CHUNK_SCALE;

		for (;;) {
			int3& pos		= stack[scale].pos;
			int& child_indx	= stack[scale].child_indx;
			Node* node		= stack[scale].node;

			if (child_indx >= 8) {
				// Pop
				scale++;

				if (scale == CHUNK_SCALE)
					break;

				if (can_collapse(node)) {
					// collapse node by writing leaf into parent
					stack[scale].node->children[ stack[scale].child_indx ] = node->children[0];
					stack[scale].node->leaf_mask |= 1u << stack[scale].child_indx;

					// free node
					uint16_t node_ptr = (uint16_t)(node - chunk->nodes);

				#if DBG_MEMSET
					memset(&chunk->nodes[node_ptr], DBG_MEMSET_VAL, sizeof(Node));
				#endif

					assert(node_ptr == chunk->alloc_ptr-1); // this algo should only ever free in a stack based order, this allows us to never generate dead nodes
					chunk->alloc_ptr--;
				}

			} else {

				int3 child_pos = pos + (children_pos[child_indx] << scale);

				if (scale == 0) {
					auto bid = blocks[child_pos.z * CHUNK_SIZE*CHUNK_SIZE + child_pos.y * CHUNK_SIZE + child_pos.x];

					node->leaf_mask |= 1u << child_indx;
					node->children[child_indx] = bid;
				} else {
					// Push
					uint16_t nodei = chunk->alloc_node(*this, stack); // invalidates node
					chunk->nodes[nodei].leaf_mask = 0;
					stack[scale].node->children[child_indx] = nodei;

					scale--;
					stack[scale] = { &chunk->nodes[nodei], child_pos, 0 };

					continue;
				}
			}

			stack[scale].child_indx++;
		}

	}

	struct ChunkLoader {
		SVO& svo;
		Voxels& voxels;
		float3 player_pos;

		struct ChunkToLoad {
			int3 coord;
			int scale;
			float dist;
		};
		std::vector<ChunkToLoad> chunks_to_load;
		std::vector<Chunk*> chunks_to_unload;

		int chunk_count = 0;

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

		void recurse_chunk_loading (Chunk* chunk, uint32_t node, bool leaf, int3 pos, int scale) {
			for (int i=0; i<8; ++i) {
				int child_scale = scale - 1;
				int3 child_pos = pos + (children_pos[i] << child_scale);

				int3 chunk_coord = child_pos + svo.root->pos;

				// need to recurse into areas that do not exist in the octree yet
				// just pretend the child nodes were the same (leaf) node as the parent
				uint32_t child_node = !leaf ? chunk->nodes[node].children[i] : node;
				bool	 child_leaf = leaf || (chunk->nodes[node].leaf_mask & (1u << i));

				Chunk* child_chunk = chunk;
				if (child_leaf && chunk == svo.root && child_node != B_NULL) {
					child_chunk = svo.chunk_allocator[node];
					child_node = 0;
					child_leaf = false;

					assert(child_chunk->scale == child_scale && equal(child_chunk->pos, chunk_coord));
				}

				bool is_loaded = !(child_leaf && child_node == B_NULL);

				float3 posf = (float3)child_pos;
				float sizef = (float)(1 << child_scale);

				float dist = calc_closest_dist(posf, sizef);
				bool want_loaded = dist <= voxels.load_radius;
				bool want_unloaded = dist > voxels.load_radius + voxels.unload_hyster;

				//int lod = max(floori(log2f( (dist - voxels.load_lod_start) / voxels.load_lod_unit )), 0);
				int lod = 0;

				if (child_scale <= CHUNK_SCALE + lod) {

					if (want_loaded) {
						sizef *= 0.99f;
						//bool pending = svo.is_chunk_load_queued(voxels, chunk_coord);
						//debug_graphics->push_wire_cube((float3)(child_pos + svo.root_pos) + 0.5f * sizef, sizef * 0.999f,
						//	is_loaded ? srgba(30,30,30,120) : (pending ? lrgba(0,0,1,1) : lrgba(1,0,0,1)));

						//if ((child_pos + svo.root->pos).z == 0)
						//debug_graphics->push_wire_cube((float3)(child_pos + svo.root->pos) + 0.5f * sizef, sizef * 0.999f, cols[lod % ARRLEN(cols)]);

						chunk_count++;

						if (!is_loaded && svo.pending_chunks.find(chunk_coord) == svo.pending_chunks.end()) {
							// chunk not generated yet
							chunks_to_load.push_back({ chunk_coord, scale, dist });
						}
					} else {
						if (is_loaded) {

							//float fardist = calc_furthest_dist(posf, sizef);
							if (want_unloaded) {
								// unload whole subtree
								assert(child_chunk != svo.root);
								chunks_to_unload.push_back(child_chunk);
							}

						}
					}
				} else {
					if (want_loaded || is_loaded) // skip parts of the octree that are 
						recurse_chunk_loading(chunk, child_node, child_leaf, child_pos, child_scale);
				}
			}
		}
	};

	void recurse_draw (SVO& svo, Chunk* chunk, uint32_t node, bool leaf, int3 pos, int scale) {
		if (leaf && chunk == svo.root) {
			if (node == B_NULL) return;

			chunk = svo.chunk_allocator[node];
			node = 0;
			leaf = false;
		}

		float size = (float)(1 << scale);
		
		auto col = cols[scale % ARRLEN(cols)];
		if ((!leaf || node > (svo.debug_draw_air ? B_NULL : B_AIR)) && scale <= svo.debug_draw_octree_max)
			debug_graphics->push_wire_cube((float3)pos + 0.5f * size, size * 0.999f, col);

		int child_scale = scale - 1;
		if (!leaf && child_scale >= svo.debug_draw_octree_min) {

			Node children = chunk->nodes[node];
			
			for (int i=0; i<8; ++i) {
				int3 child_pos = pos + (children_pos[i] << child_scale);

				recurse_draw(svo, chunk, children.children[i], children.leaf_mask & (1u << i), child_pos, child_scale);
			}
		}
	}

	void SVO::chunk_loading (Voxels& voxels, Player& player, WorldGenerator& world_gen) {
		ZoneScoped;

		//update_root(*this, player_pos);

		{ // finish first, so that pending_chunks becomes smaller before we cap the newly queued ones to limit this size
			ZoneScopedN("finish chunkgen jobs");

			std::vector< std::unique_ptr<ThreadingJob> > jobs (voxels.cap_chunk_load);

			size_t count = background_threadpool.results.pop_multiple(&jobs[0], voxels.cap_chunk_load);

			for (size_t i=0; i<count; ++i) {
				jobs[i]->finalize();
			}
		}

		ChunkLoader cl = { *this, voxels, player.pos - (float3)root->pos } ;
		{
			ZoneScopedN("recurse_chunk_loading");
			cl.recurse_chunk_loading(root, 0, false, 0, root->scale);

			ImGui::Text("chunk_count: %d", cl.chunk_count);
		}

		{
			ZoneScopedN("chunk unloading");
			for (auto chunk : cl.chunks_to_unload) {
				assert(pending_chunks.find(chunk->pos) == pending_chunks.end());
				assert(active_chunks.find(chunk->pos) != active_chunks.end());

				active_chunks.erase(chunk->pos);

				assert(chunk->scale == CHUNK_SCALE);
				octree_write(chunk->pos, chunk->scale, B_NULL);
			}
		}

		{
			ZoneScopedN("std::sort chunks_to_load");

			std::sort(cl.chunks_to_load.begin(), cl.chunks_to_load.end(), [] (ChunkLoader::ChunkToLoad& l, ChunkLoader::ChunkToLoad& r) {
				return std::less<float>()(l.dist, r.dist);
			});
		}

		{
			ZoneScopedN("queue chunk loads");

			int count = min((int)cl.chunks_to_load.size(), voxels.cap_chunk_load - (int)pending_chunks.size());

			int i=0;
			background_threadpool.jobs.push_multiple([&] () -> std::unique_ptr<WorldgenJob> {
				if (i >= count)
					return nullptr;

				auto c = cl.chunks_to_load[i++];

				auto* chunk = chunk_allocator.alloc();
				new (chunk) Chunk (c.coord, CHUNK_SCALE);

				ZoneScopedN("push WorldgenJob");

				assert(pending_chunks.find(c.coord) == pending_chunks.end());
				assert(active_chunks.find(c.coord) == active_chunks.end());

				pending_chunks.emplace(c.coord, chunk);

				return std::make_unique<WorldgenJob>(chunk, this, &world_gen);
			});
		}

		if (debug_draw_svo) {
			ZoneScopedN("SVO::debug_draw");
		
			recurse_draw(*this, root, 0, false, root->pos, root->scale);
		}
	}
}