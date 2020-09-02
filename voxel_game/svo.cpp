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
				chunk = allocator[child_val];
				node = chunk->nodes;

			// Could early out on no-op octree writes, but should probably just make sure they don't happen instead of optimizing the performance of a no-op, since additional code could even slow us down
			//} else if (child_val == val) {
			//	// write would be a no-op because the octree already has the desired value at that voxel
			//	return;
			
			} else {
				//// Split node into 8 children of same type
				uint16_t node_ptr = chunk->alloc_node(allocator, stack); // invalidates child_node

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

				chunk->flags |= GPU_DIRTY;
			}
		}

		// do the write
		stack[cur_scale].node->children[ stack[cur_scale].child_indx ] = val;
		stack[cur_scale].node->leaf_mask |= 1u << stack[cur_scale].child_indx;

		chunk->flags |= GPU_DIRTY;

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
				chunk = allocator[child_data];
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
					uint16_t nodei = chunk->alloc_node(allocator, stack); // invalidates node
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

	void free_chunk (SVO& svo, Chunk* chunk) {
		svo.octree_write(chunk->pos, chunk->scale, B_NULL);

		svo.allocator.free_chunk(chunk);
	}

	void recurse_free (SVO& svo, uint32_t node, bool leaf) {
		if (leaf) {
			if (node != B_NULL) {
				Chunk* chunk = svo.allocator[node];
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
	void update_root_move (SVO& svo, Player& player) {
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

			chunk = svo.allocator[node];
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

	void ChunkLoadJob::finalize () {
		ZoneScoped;

		int3 pos = chunk->pos;
		int scale = chunk->scale;
		
		switch (load_type) {
			case LoadOp::CREATE: {
				clog("apply CREATE to %2d : %+7d,%+7d,%+7d", scale, pos.x,pos.y,pos.z);

				assert(scale == svo.root->scale-1);

				svo.octree_write(pos, scale, svo.allocator.indexof(chunk));

				svo.pending_chunks.remove(pos, scale);
				svo.chunks.insert(pos, scale, chunk);

			} break;

			case LoadOp::SPLIT: {
				clog("apply SPLIT to %2d : %+7d,%+7d,%+7d", scale, pos.x,pos.y,pos.z);

				// this chunk is done
				chunk->flags |= LOCKED;

				svo.pending_chunks.remove(pos, scale);
				svo.chunks.insert(pos, scale, chunk);

				int3 parent_pos = ((pos - svo.root->pos) & ~((1 << (scale+1)) - 1)) + svo.root->pos;

				// check if all siblings are done
				Chunk* siblings[8] = {};
				for (int i=0; i<8; ++i) {
					int3 pos = parent_pos + (children_pos[i] << scale);
					svo.chunks.get(pos, scale, &siblings[i]);

					if (!siblings[i])
						return; // not all siblings are done, wait until they are
				}

				// free parent chunk
				auto parent = svo.chunks.remove(parent_pos, scale+1);
				if (parent) {
					free_chunk(svo, parent);

					// remove child count from parent parent
					int3 grandparent_pos = ((parent_pos - svo.root->pos) & ~((1 << (scale+2)) - 1)) + svo.root->pos;
					
					auto it = svo.parent_chunks.chunks.find(int4(grandparent_pos, scale+2));
					if (it != svo.parent_chunks.chunks.end()) {
						assert(it->second > 0);
						it->second--;
						if (it->second == 0) {
							svo.parent_chunks.chunks.erase(it);
						}
					}

					// add parent entry with count of direct children to trigger merge when appropriate
					svo.parent_chunks.insert(parent_pos, scale+1, 8);
				}

				// insert us and all siblings into octree
				for (int i=0; i<8; ++i) {
					assert(siblings[i]->flags & LOCKED);
					siblings[i]->flags &= ~LOCKED;
					svo.octree_write(siblings[i]->pos, siblings[i]->scale, svo.allocator.indexof(siblings[i]));
				}
			} break;

			case LoadOp::MERGE: {
				clog("apply MERGE to %2d : %+7d,%+7d,%+7d", scale, pos.x,pos.y,pos.z);
			
				// delete children
				for (int i=0; i<8; ++i) {
					int3 child_pos = pos + (children_pos[i] << scale-1);
					auto child = svo.chunks.remove(child_pos, scale-1);
					free_chunk(svo, child);
				}
			
				svo.octree_write(pos, scale, svo.allocator.indexof(chunk));

				svo.pending_chunks.remove(pos, scale);
				svo.chunks.insert(pos, scale, chunk);
			
				// add parent to be able to track merges again
				if (scale < svo.root->scale-1) {
					int3 parent_pos = ((pos - svo.root->pos) & ~((1 << (scale+1)) - 1)) + svo.root->pos;
					
					svo.parent_chunks.chunks[int4(parent_pos, scale+1)]++;
				}
			} break;
		}
	}
	void SVO::chunk_loading (Voxels& voxels, Player& player, WorldGenerator& world_gen) {
		ZoneScoped;

		update_root_move(*this, player);

		{ // finish first, so that pending_chunks becomes smaller before we cap the newly queued ones to limit this size
			ZoneScopedN("finish chunkgen jobs");

			static constexpr int finalize_cap = 64;
			std::vector< std::unique_ptr<ThreadingJob> > jobs (finalize_cap);

			size_t count = background_threadpool.results.pop_multiple(&jobs[0], (int)jobs.size());

			for (size_t i=0; i<count; ++i) {
				jobs[i]->finalize();
			}
		}

		std::vector<LoadOp> ops_to_queue;
		{
			ZoneScopedN("chunk iteration");

			for (int i=0; i<8; ++i) {
				int scale = root->scale -1;
				int3 pos = root->pos + (children_pos[i] << scale);

				if (root->nodes[0].leaf_mask & (1u << i) && root->nodes[0].children[i] == B_NULL) {
					// unloaded chunk in root due to root move or initial world creation
					if (!chunks.contains(pos, scale) && !pending_chunks.contains(pos, scale))
						ops_to_queue.push_back({ nullptr, pos, root->scale-1, -9999, LoadOp::CREATE });
				}
			}

			auto calc_lod = [&] (int3 pos, int scale, float* out_dist) {
				float size = (float)(1 << scale);

				float3 pos_rel = player.pos - (float3)pos;

				float3 clamped = clamp(pos_rel, 0, size);
				float dist = length(clamped - pos_rel);

				int lod = max(floori(log2f( (dist - voxels.load_lod_start) / voxels.load_lod_unit )), 0);

				*out_dist = dist;
				return lod;
			};

			for (auto& it : chunks.chunks) {
				if (it.second->flags & LOCKED) continue;

				int3 pos = (int3)it.first.v;
				int scale = it.first.v.w;

				float dist;
				int lod = calc_lod(pos, scale, &dist);

				if (scale > CHUNK_SCALE + lod) {
					// split chunk into children
					ops_to_queue.push_back({ it.second, pos, scale, dist, LoadOp::SPLIT });
				}
			}

			for (auto& it : parent_chunks.chunks) {
				assert(it.second >= 0 && it.second <= 8);
				if (it.second != 8) continue;
			
				int3 pos = (int3)it.first.v;
				int scale = it.first.v.w;
			
				float dist;
				int lod = calc_lod(pos, scale, &dist);
			
				if (scale <= CHUNK_SCALE + lod) {
					// split chunk into children
					ops_to_queue.push_back({ nullptr, pos, scale, dist, LoadOp::MERGE });
				}
			}
		}

		{
			ZoneScopedN("std::sort ops_to_queue");

			// TODO: why is sorting in wrong order, often large scale updates happen first even though they are clearly futher away?
			std::sort(ops_to_queue.begin(), ops_to_queue.end(), [] (LoadOp& l, LoadOp& r) {
				//if (l.scale != r.scale)
				//	return std::less<int>()(l.scale, r.scale);
				return std::less<float>()(l.dist, r.dist);
			});
		}

		{
			ZoneScopedN("queue chunk loads");

			int count = clamp(voxels.cap_chunk_load - (int)pending_chunks.count(), 0, (int)ops_to_queue.size());
			std::vector<std::unique_ptr<ThreadingJob>> jobs;

			for (auto& op : ops_to_queue) {
				if ((int)jobs.size() >= count)
					break;

				switch (op.type) {
					case LoadOp::CREATE: {
						auto* chunk = allocator.alloc_chunk();
						new (chunk) Chunk (op.pos, op.scale);

						pending_chunks.insert(op.pos, op.scale, chunk);

						//clog("queue CREATE for %2d : %+7d,%+7d,%+7d", op.scale, op.pos.x,op.pos.y,op.pos.z);
						jobs.emplace_back( std::make_unique<ChunkLoadJob>(chunk, *this, world_gen, op.type) );
					} break;

					case LoadOp::SPLIT: {
						// split
						op.chunk->flags |= LOCKED;

						for (int i=0; i<8; ++i) {
							int child_scale = op.scale -1;
							int3 child_pos = op.pos + (children_pos[i] << child_scale);

							auto* chunk = allocator.alloc_chunk();
							new (chunk) Chunk (child_pos, child_scale);

							pending_chunks.insert(child_pos, child_scale, chunk);

							//clog("queue SPLIT for %2d : %+7d,%+7d,%+7d", child_scale, child_pos.x,child_pos.y,child_pos.z);
							jobs.emplace_back( std::make_unique<ChunkLoadJob>(chunk, *this, world_gen, op.type) );
						}
					} break;

					case LoadOp::MERGE: {
						// merge
						auto* chunk = allocator.alloc_chunk();
						new (chunk) Chunk (op.pos, op.scale);
					
						parent_chunks.remove(op.pos, op.scale);
						pending_chunks.insert(op.pos, op.scale, chunk);
					
						// lock children
						for (int i=0; i<8; ++i) {
							int3 child_pos = op.pos + (children_pos[i] << (op.scale-1));
					
							Chunk* c = *chunks.get(child_pos, op.scale-1);
							c->flags |= LOCKED;
						}
					
						//clog("queue MERGE for %2d : %+7d,%+7d,%+7d", op.scale, op.pos.x,op.pos.y,op.pos.z);
						jobs.emplace_back( std::make_unique<ChunkLoadJob>(chunk, *this, world_gen, op.type) );
					} break;
				}
			}

			background_threadpool.jobs.push_multiple(jobs.data(), (int)jobs.size());
		}

		if (debug_draw_svo) {
			ZoneScopedN("SVO::debug_draw");
		
			recurse_draw(*this, root, 0, false, root->pos, root->scale);
		}

		if (debug_draw_chunks) {
			for (auto* chunk : chunks) {
				float size = (float)(1 << chunk->scale);
				if (!debug_draw_chunks_onlyz0 || chunk->pos.z == 0)
					debug_graphics->push_wire_cube((float3)chunk->pos + 0.5f * size, size * 0.995f, cols[chunk->scale % ARRLEN(cols)] * lrgba(1,1,1,0.5f));
			}
			//for (auto* chunk : pending_chunks) {
			//	float size = (float)(1 << chunk->scale);
			//	if (!debug_draw_chunks_onlyz0 || chunk->pos.z == 0)
			//		debug_graphics->push_wire_cube((float3)chunk->pos + 0.5f * size, size * 0.8f, srgba(255, 75, 0));
			//}
			//for (auto it : parent_chunks.chunks) {
			//	int3 pos = (int3)it.first.v;
			//	float size = (float)(1 << it.first.v.w);
			//	if (!debug_draw_chunks_onlyz0 || pos.z == 0)
			//		debug_graphics->push_wire_cube((float3)pos + 0.5f * size, size * 0.995f, srgba(20, 255, 0));
			//}
		}
	}
}
