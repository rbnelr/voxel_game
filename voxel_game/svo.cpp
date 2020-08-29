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

				uint32_t commit_block = os_page_size / sizeof(Node);
				commit_ptr = commit_block;
				commit_pages(nodes, os_page_size);

			#if DBG_MEMSET
				memset(nodes, DBG_MEMSET_VAL, os_page_size);
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
			} else {
				if (commit_ptr < MAX_NODES) {
					uint32_t commit_block = os_page_size / sizeof(Node);

					commit_pages(nodes + commit_ptr, os_page_size);

				#if DBG_MEMSET
					memset(nodes + commit_ptr, DBG_MEMSET_VAL, os_page_size);
				#endif
					commit_ptr += commit_block;

				} else {
					assert(false);
					throw std::runtime_error("Octree allocation overflow"); // crash is preferable to corrupting our octree
				}
			}

		}
		return (uint16_t)alloc_ptr++;
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
		ZoneScoped;

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

			uint16_t child_val = node->children[child_idx];

			bool child_leaf = node->leaf_mask & (1u << child_idx);

			if (!child_leaf) { 
				// recurse into child node
				node = &chunk->nodes[child_val];

			} else if (chunk == root && child_val != B_NULL) {
				// leafs in root svo (ie. svo of chunks) are chunks, so recurse into chunk nodes
				chunk = chunk_allocator[child_val];
				node = chunk->nodes;

			// Could early out on no-op octree writes, but should probably just make sure they don't happen instead of optimizing the performance of a no-op, since additional code could even slow us down
			//} else if (child_val == val) {
			//	// write would be a no-op because the octree already has the desired value at that voxel
			//	return;
			
			} else {
				//// Split node into 8 children of same type
				uint16_t node_ptr = chunk->alloc_node(*this, stack); // invalidates child_node

				// write ptr to children into this nodes slot in parents children array
				stack[cur_scale].node->children[child_idx] = node_ptr;
				stack[cur_scale].node->leaf_mask ^= (1u << child_idx);

				// init and recurse into new node
				node = &chunk->nodes[node_ptr];

				node->leaf_mask = 0xff;
				// alloc and init children for node
				for (int i=0; i<8; ++i) {
					node->children[i] = child_val;
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

	block_id SVO::octree_read (int3 pos) {
		ZoneScoped;

		pos -= root->pos;
		if (any(pos < 0 || pos >= (1 << root->scale)))
			return B_NULL;

		// start with root node
		Chunk* chunk = root;
		int cur_scale = root->scale;
		Node* node = &root->nodes[0];

		for (;;) {
			cur_scale--;
			assert(cur_scale >= 0);

			// get child node that contains target node
			int child_idx = get_child_index(pos, cur_scale);

			uint16_t child_data = node->children[child_idx];
			bool leaf = node->leaf_mask & (1u << child_idx);

			if (!leaf) {
				node = &chunk->nodes[child_data];
			} else if (chunk == root && child_data != B_NULL) {
				// leafs in root svo (ie. svo of chunks) are chunks, so recurse into chunk nodes
				chunk = chunk_allocator[child_data];
				node = chunk->nodes;
			} else {
				return (block_id)child_data;
			}
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
			int3 pos;
			int scale;
			float dist;
		};
		std::vector<ChunkToLoad> chunks_to_load;
		std::vector<Chunk*> chunks_to_unload;

		int iterated_count = 0;

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

		void recurse_chunk_loading (uint32_t node, bool leaf, int3 pos, int scale) {
			for (int i=0; i<8; ++i) {
				int child_scale = scale - 1;
				int3 child_pos = pos + (children_pos[i] << child_scale);

				assert(child_scale >= CHUNK_SCALE);

				uint32_t child_node = !leaf ? svo.root->nodes[node].children[i] : node;
				bool	 child_leaf = leaf || svo.root->nodes[node].leaf_mask & (1u << i);

				float3 posf = (float3)child_pos;
				float sizef = (float)(1 << child_scale);

				float dist = calc_closest_dist(posf, sizef);

				//debug_graphics->push_wire_cube((float3)posf + 0.5f * sizef, sizef * 0.95f, cols[child_scale*2 % ARRLEN(cols)]);

				iterated_count++;

				if (child_leaf) {
					//int lod = max(floori(log2f( (dist - voxels.load_lod_start) / voxels.load_lod_unit )), 0);
					int lod = 0;

					assert(child_scale - CHUNK_SCALE >= lod);

					if (child_node == B_NULL) {
						// no chunk is loaded
						if (dist <= voxels.load_radius) {
							if (child_scale - CHUNK_SCALE == lod) {
								if (svo.pending_chunks.find(child_pos) == svo.pending_chunks.end()) {
									// chunk not queued to be generated yet
									chunks_to_load.push_back({ child_pos, child_scale, dist });
								}
							} else {
								// recurse to the scale we want stuff to be loaded at
								recurse_chunk_loading(B_NULL, true, child_pos, child_scale);
							}
						}
					} else {
						// this chunk is loaded
						Chunk* chunk = svo.chunk_allocator[child_node];
						assert(chunk->scale == child_scale && equal(chunk->pos, child_pos));

						bool want_refine = lod < child_scale - CHUNK_SCALE;

						if (dist > voxels.load_radius + voxels.unload_hyster) {
							chunks_to_unload.push_back(chunk);
						}
					}
				} else {
					// normal recursion into non chunk nodes
					recurse_chunk_loading(child_node, child_leaf, child_pos, child_scale);
				}
			}
		}
	};

	void free_chunk (SVO& svo, Chunk* chunk) {
		assert(svo.pending_chunks.find(chunk->pos) == svo.pending_chunks.end());
		assert(svo.active_chunks.find(chunk->pos) != svo.active_chunks.end());

		svo.active_chunks.erase(chunk->pos);

		assert(chunk->scale == CHUNK_SCALE);
		svo.octree_write(chunk->pos, chunk->scale, B_NULL);

		if (chunk->nodes != chunk->tinydata)
			svo.node_allocator.free((AllocBlock*)chunk->nodes);
		svo.chunk_allocator.free(chunk);
	}

	void recurse_free (SVO& svo, uint32_t node, bool leaf) {
		if (leaf) {
			if (node != B_NULL) {
				Chunk* chunk = svo.chunk_allocator[node];
				free_chunk(svo, chunk);
			}
		} else {
			for (int i=0; i<8; ++i) {
				Node& child_node = svo.root->nodes[node];
				recurse_free(svo, child_node.children[i], child_node.leaf_mask & (1u << i));
			}
		}
	}

	// move octree root along with player through world, so that ideally all desired visible parts of the world are contained
	void update_root (SVO& svo, Player& player) {
		int shift = svo.root->scale - 1;
		float half_root_scalef = (float)(1 << shift);

		int3 center = roundi((player.pos + svo.root_move_hister) / half_root_scalef);

		int3 pos = (center - 1) << shift;

		if (equal(pos, svo.root->pos))
			return;

		ZoneScoped;

		int3 move = (pos - svo.root->pos) >> shift;

		svo.root_move_hister = (float3)move * ROOT_MOVE_HISTER;

		// write moved children into new root node
		Node new_node;
		new_node.leaf_mask = 0;
		for (int i=0; i<8; ++i) {
			int3 child = int3(i & 1, (i >> 1) & 1, (i >> 2) & 1);

			child += move;

			int child_indx = child.x | (child.y << 1) | (child.z << 2);

			if (any(child < 0 || child > 1)) {
				// root moved over this area, no nodes loaded yet
				new_node.children[i] = B_NULL;
				new_node.leaf_mask |= 1u << i;
			} else {
				// copy old children to new slot
				new_node.children[i] = svo.root->nodes[0].children[child_indx];
				if (svo.root->nodes[0].leaf_mask & (1u << child_indx))
					new_node.leaf_mask |= 1u << i;

				svo.root->nodes[0].children[child_indx] = 0;
			}
		}

		// free old subtrees that were dropped (should not really happen all that often because that means our root node is too small)
		// this needs to happen before we do svo.root->pos = pos; or free_chunk will free the wrong chunk 
		for (int i=0; i<8; ++i) {
			if (svo.root->nodes[0].children[i] != 0) {
				recurse_free(svo, svo.root->nodes[0].children[i], svo.root->nodes[0].leaf_mask & (1u << i));
			}
		}

		// replace old node
		memcpy(&svo.root->nodes[0], &new_node, sizeof(Node));

		// actually move root
		svo.root->pos = pos;
	}

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

			Node& children = chunk->nodes[node];
			
			for (int i=0; i<8; ++i) {
				int3 child_pos = pos + (children_pos[i] << child_scale);

				recurse_draw(svo, chunk, children.children[i], children.leaf_mask & (1u << i), child_pos, child_scale);
			}
		}
	}

	void SVO::chunk_loading (Voxels& voxels, Player& player, WorldGenerator& world_gen) {
		ZoneScoped;

		update_root(*this, player);

		{ // finish first, so that pending_chunks becomes smaller before we cap the newly queued ones to limit this size
			ZoneScopedN("finish chunkgen jobs");

			std::vector< std::unique_ptr<ThreadingJob> > jobs (voxels.cap_chunk_load);

			size_t count = background_threadpool.results.pop_multiple(&jobs[0], voxels.cap_chunk_load);

			for (size_t i=0; i<count; ++i) {
				jobs[i]->finalize();
			}
		}

		ChunkLoader cl = { *this, voxels, player.pos } ;
		{
			ZoneScopedN("recurse_chunk_loading");
			cl.recurse_chunk_loading(0, false, root->pos, root->scale);

			ImGui::Text("chunk_loading iterated_count: %d", cl.iterated_count);
		}

		{
			ZoneScopedN("chunk unloading");
			for (auto chunk : cl.chunks_to_unload) {
				free_chunk(*this, chunk);
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
				new (chunk) Chunk (c.pos, CHUNK_SCALE);

				ZoneScopedN("push WorldgenJob");

				assert(pending_chunks.find(c.pos) == pending_chunks.end());
				assert(active_chunks.find(c.pos) == active_chunks.end());

				pending_chunks.emplace(c.pos, chunk);

				return std::make_unique<WorldgenJob>(chunk, this, &world_gen);
			});

			if (debug_draw_chunks) {
				for (; i<(int)cl.chunks_to_load.size(); ++i) {
					auto& c = cl.chunks_to_load[i];

					float size = (float)(1 << c.scale);
					debug_graphics->push_wire_cube((float3)c.pos + 0.5f * size, size * 0.1f, srgba(0xFF, 0x00, 0x37));
				}
			}
		}

		if (debug_draw_svo) {
			ZoneScopedN("SVO::debug_draw");
		
			recurse_draw(*this, root, 0, false, root->pos, root->scale);
		}

		if (debug_draw_chunks) {
			for (auto& it : active_chunks) {
				float size = (float)(1 << it.second->scale);
				debug_graphics->push_wire_cube((float3)it.second->pos + 0.5f * size, size * 0.95f, srgba(0x00, 0x5E, 0xFF, 100));
			}

			for (auto& it : pending_chunks) {
				float size = (float)(1 << it.second->scale);
				debug_graphics->push_wire_cube((float3)it.second->pos + 0.5f * size, size * 0.8f, srgba(0xFF, 0x84, 0x00));
			}
		}
	}
}
