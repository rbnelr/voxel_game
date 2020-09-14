#include "stdafx.hpp"
#include "svo.hpp"
#include "voxel_system.hpp"
#include "worldgen.hpp"
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
	bool can_collapse (Node* node) {
		if (node->children_types != ONLY_BLOCK_IDS)
			return false; // can only collapse block_id voxels

		//for (int i=1; i<8; ++i) {
		//	if (node->children[i] != node->children[0])
		//		return false; // not all leafs equal, cant collapse
		//}

		//return // manual unroll because compiler does not do it, faster
		//	node->children[1] == node->children[0] && 
		//	node->children[2] == node->children[0] && 
		//	node->children[3] == node->children[0] && 
		//	node->children[4] == node->children[0] && 
		//	node->children[5] == node->children[0] && 
		//	node->children[6] == node->children[0] && 
		//	node->children[7] == node->children[0];

		// simd version of the check
		auto cmp = _mm256_xor_si256(_mm256_loadu_si256((__m256i*)node->children), _mm256_set1_epi32(node->children[0]));
		return _mm256_testz_si256(cmp, cmp); // true if zero, ie. all children == child0
	}

	void recurse_neighbour (SVO& svo, TypedVoxel vox, int mask, int side) { // mask selects axis that neighbour touches, side says which side (1=right)
		if (vox.type == NODE_PTR) {
			for (int i=0; i<8; ++i) {
				if ((i & mask) == side)
					recurse_neighbour(svo, svo.root->nodes[vox.value].get_child(i), mask, side);
			}
		} else {
			if (vox.type == CHUNK_PTR) {
				assert(vox.value != 0);
				Chunk* chunk = &svo.allocator.chunks[vox.value];
				chunk->flags |= MESH_DIRTY;
			}
		}
	}
	void flag_neighbour_meshing (SVO& svo, Chunk* chunk, int scale, int size, int x, int y, int z) {
		int chunk_size = 1 << chunk->scale;
		int relx = x - chunk->pos.x;
		int rely = y - chunk->pos.y;
		int relz = z - chunk->pos.z;

		if (relx == 0)					recurse_neighbour(svo, svo.octree_read(x -size, y, z, scale, true).vox, 1, 1);
		if (relx +size == chunk_size)	recurse_neighbour(svo, svo.octree_read(x +size, y, z, scale, true).vox, 1, 0);
		if (rely == 0)					recurse_neighbour(svo, svo.octree_read(x, y -size, z, scale, true).vox, 2, 2);
		if (rely +size == chunk_size)	recurse_neighbour(svo, svo.octree_read(x, y +size, z, scale, true).vox, 2, 0);
		if (relz == 0)					recurse_neighbour(svo, svo.octree_read(x, y, z -size, scale, true).vox, 4, 4);
		if (relz +size == chunk_size)	recurse_neighbour(svo, svo.octree_read(x, y, z +size, scale, true).vox, 4, 0);
	}

	void __vectorcall SVO::octree_write (int x, int y, int z, int scale, TypedVoxel vox) {
		int cur_scale = root->scale;
		int size = 1 << root->scale;
		int relx = x - root->pos.x;
		int rely = y - root->pos.y;
		int relz = z - root->pos.z;
		if (relx < 0 || rely < 0 || relz < 0 || relx >= size || rely >= size || relz >= size) {
			// writes outside the root node are not possible, should never be attempted by caller
			assert(false);
		}

		// start with root node
		Node* node = &root->nodes[0];

		Chunk* chunk = root;

		StackNode stack[MAX_DEPTH];
	#if DBG_MEMSET
		memset(stack, DBG_MEMSET_VAL, sizeof(stack));
	#endif

		assert(scale < root->scale);

		for (;;) {
			cur_scale--;
			size >>= 1;
			assert(size > 0);

			// get child node that contains target node
			int child_idx = get_child_index(relx,rely,relz, size);

			// update stack
			stack[cur_scale].node = node;
			stack[cur_scale].child_idx = child_idx;

			if (cur_scale == scale) {
				// reached target octree depth
				break;
			}

			auto child_vox = node->get_child(child_idx);

			// Could early out on no-op octree writes, but should probably just make sure they don't happen instead of optimizing the performance of a no-op, since additional code could even slow us down
			//} else if (child_val == val) {
			//	// write would be a no-op because the octree already has the desired value at that voxel
			//	return;

			if (child_vox.type != BLOCK_ID) {
				if (child_vox.type == CHUNK_PTR) {
					assert(child_vox.value != 0);
					// leafs in root svo (ie. svo of chunks) are chunks, so recurse into chunk nodes
					chunk = &allocator.chunks[child_vox.value];
					child_vox.value = 0;
				}

				// recurse into child node
				node = &chunk->nodes[child_vox.value];

			} else {
				//// Split node into 8 children of same type and recurse into the correct child
				assert(child_vox.type == BLOCK_ID);

				uint32_t node_ptr;
				Node* new_node = allocator.alloc_node(chunk, &node_ptr);
				assert(node_ptr > 0);

				// write ptr to children into this nodes slot in parents children array
				node->set_child(child_idx, { NODE_PTR, node_ptr });

				// recurse into new node
				node = new_node;

				// alloc and init children for node
				node->children_types = ONLY_BLOCK_IDS;
				for (int i=0; i<8; ++i) {
					node->children[i] = child_vox.value;
				}

				chunk->flags |= SVO_DIRTY;
			}
		}

		assert(vox.type == CHUNK_PTR || vox.type == BLOCK_ID);
		if (vox.type == CHUNK_PTR)
			assert(chunk == root);

		// do the write
		stack[cur_scale].node->set_child(stack[cur_scale].child_idx, vox);

		chunk->flags |= SVO_DIRTY | MESH_DIRTY;

		// collapse octree nodes with same leaf values by walking up the stack, stop at chunk root or svo root or if can't collapse
		for (	auto i = cur_scale;
				i < chunk->scale-1 && can_collapse(stack[i].node);
				++i) {

			stack[i+1].node->set_child(stack[i+1].child_idx, { BLOCK_ID, stack[i].node->children[0] });
		}

		flag_neighbour_meshing(*this, vox.type == CHUNK_PTR ? &allocator.chunks[vox.value] : chunk, scale, size, x,y,z);
	}

	OctreeReadResult __vectorcall SVO::octree_read (int x, int y, int z, int target_scale, bool read_chunk) {
		int scale = root->scale;
		int size = 1 << root->scale;
		
		x -= root->pos.x;
		y -= root->pos.y;
		z -= root->pos.z;
		if (x < 0 || y < 0 || z < 0 || x >= size || y >= size || z >= size) {
			return { { BLOCK_ID, B_NULL }, size, INT_MAX };
		}

		// start with root node
		Chunk* chunk = root;
		Node* node = &root->nodes[0];

		TypedVoxel vox;

		for (;;) {
			scale--;
			size >>= 1;
			assert(size > 0);

			// get child node that contains target node
			int child_idx = get_child_index(x,y,z, size);

			vox = node->get_child(child_idx);

			if (vox.type == BLOCK_ID) {
				break;
			} else {
				assert(vox.type == NODE_PTR || vox.type == CHUNK_PTR);
				assert(vox.value != 0);
				
				if (scale == target_scale) {
					// return size=-1 to signal that the target node is made up of further subdivided voxels
					size = -1;
					break;
				}

				uint32_t node_ptr = vox.value;

				if (vox.type == CHUNK_PTR) {
					if (read_chunk)
						break;

					// leafs in root svo (ie. svo of chunks) are chunks, so recurse into chunk nodes
					chunk = &allocator.chunks[node_ptr];
					node_ptr = 0;
				}

				// recurse into child node
				node = &chunk->nodes[node_ptr];
			}
		}
		
		return { vox, size, chunk->scale - CHUNK_SCALE };
	}

	void SVO::chunk_to_octree (Chunk* chunk, block_id* blocks) {
		ZoneScopedN("svo_chunk_to_octree");

		StackNode stack[MAX_DEPTH];
		
		Node dummy;
		stack[CHUNK_SCALE] = { &dummy, 0, 0 };

		uint16_t scale = CHUNK_SCALE;

		for (;;) {
			assert(scale > 0);
			int3& pos			= stack[scale].pos;
			int& child_indx		= stack[scale].child_idx;
			Node* node			= stack[scale].node;

			if (child_indx >= 8) {
				// Pop
				scale++;

				if (scale == CHUNK_SCALE)
					break;

				if (can_collapse(node)) {
					// collapse node by writing leaf into parent
					stack[scale].node->set_child(stack[scale].child_idx, { BLOCK_ID, node->children[0] });

					// free node
				#if DBG_MEMSET
					memset(node, DBG_MEMSET_VAL, sizeof(Node));
				#endif

					uint32_t node_idx = allocator.indexof(chunk, node);

					assert(node_idx == chunk->alloc_ptr -1); // this algo should only ever free in a stack based order, this allows us to never generate dead nodes
					chunk->alloc_ptr--;
				}

			} else {

				int x = pos.x + (children_pos[child_indx].x << scale);
				int y = pos.y + (children_pos[child_indx].y << scale);
				int z = pos.z + (children_pos[child_indx].z << scale);

				if (scale == 1) {
					// generate 8 leaf nodes in a more optimized way than pushing and poping
					
					// use seperate CHUNK_3D_INDEX for parent xyz and child offset to make compiler understand how to do this indexing efficently
					uintptr_t blocks_idx = CHUNK_3D_INDEX(x,y,z);
					
					// manual unroll because compiler was not able to do this efficently
					bool can_collapse =
						blocks[blocks_idx + CHUNK_3D_CHILD_OFFSET(1)] == blocks[blocks_idx] && 
						blocks[blocks_idx + CHUNK_3D_CHILD_OFFSET(2)] == blocks[blocks_idx] && 
						blocks[blocks_idx + CHUNK_3D_CHILD_OFFSET(3)] == blocks[blocks_idx] && 
						blocks[blocks_idx + CHUNK_3D_CHILD_OFFSET(4)] == blocks[blocks_idx] && 
						blocks[blocks_idx + CHUNK_3D_CHILD_OFFSET(5)] == blocks[blocks_idx] && 
						blocks[blocks_idx + CHUNK_3D_CHILD_OFFSET(6)] == blocks[blocks_idx] && 
						blocks[blocks_idx + CHUNK_3D_CHILD_OFFSET(7)] == blocks[blocks_idx];

					if (can_collapse) {

						node->set_child(child_indx, { BLOCK_ID, blocks[blocks_idx] });

					} else {
						uint32_t node_ptr;
						Node* new_node = allocator.alloc_node(chunk, &node_ptr);

						new_node->children_types = ONLY_BLOCK_IDS;
						for (int i=0; i<8; i++) {
							new_node->children[i] = blocks[blocks_idx + CHUNK_3D_CHILD_OFFSET(i)];
						}

						node->set_child(child_indx, { NODE_PTR, node_ptr });
					}

				} else {
					// Push
					uint32_t node_ptr;
					Node* new_node = allocator.alloc_node(chunk, &node_ptr);

					node->set_child(child_indx, { NODE_PTR, node_ptr });

					scale--;
					stack[scale] = { new_node, int3(x, y, z), 0 };
					continue;
				}
			}

			stack[scale].child_idx++;
		}

	}

	void free_chunk (SVO& svo, Chunk* chunk) {
		svo.octree_write(chunk->pos.x, chunk->pos.y, chunk->pos.z, chunk->scale, { BLOCK_ID, B_NULL });

		svo.allocator.free_chunk(chunk);
	}

	void recurse_free (SVO& svo, TypedVoxel vox) {
		switch (vox.type) {
			case CHUNK_PTR: {
				assert(vox.value != 0);

				Chunk* chunk = &svo.allocator.chunks[vox.value];
				free_chunk(svo, chunk);
			} break;
			case NODE_PTR: {
				for (int i=0; i<8; ++i) {
					auto child = svo.root->nodes[vox.value].get_child(i);
					recurse_free(svo, child);
				}
			} break;
		}
	}

	// move octree root along with player through world, so that ideally all desired visible parts of the world are contained
	void update_root_move (SVO& svo, Player& player) {
		return; // TODO: current lod system is not compatible with root move, unless root move could somehow discard all in-process async chunk generation
	//
	//	int shift = svo.root->scale - 1;
	//	float half_root_scalef = (float)(1 << shift);
	//
	//	int3 center = roundi((player.pos + svo.root_move_hister) / half_root_scalef);
	//
	//	int3 pos = (center - 1) << shift;
	//
	//	if (equal(pos, svo.root->pos))
	//		return;
	//
	//	ZoneScoped;
	//
	//	int3 move = (pos - svo.root->pos) >> shift;
	//
	//	svo.root_move_hister = (float3)move * ROOT_MOVE_HISTER;
	//
	//	// write moved children into new root node
	//	Node new_node;
	//	new_node.leaf_mask = 0;
	//	for (int i=0; i<8; ++i) {
	//		int3 child = int3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
	//
	//		child += move;
	//
	//		int child_indx = child.x | (child.y << 1) | (child.z << 2);
	//
	//		if (any(child < 0 || child > 1)) {
	//			// root moved over this area, no nodes loaded yet
	//			new_node.children[i] = B_NULL;
	//			new_node.leaf_mask |= 1u << i;
	//		} else {
	//			// copy old children to new slot
	//			new_node.children[i] = svo.root->nodes[0].children[child_indx];
	//			if (svo.root->nodes[0].leaf_mask & (1u << child_indx))
	//				new_node.leaf_mask |= 1u << i;
	//
	//			svo.root->nodes[0].children[child_indx] = 0;
	//		}
	//	}
	//
	//	// free old subtrees that were dropped (should not really happen all that often because that means our root node is too small)
	//	// this needs to happen before we do svo.root->pos = pos; or free_chunk will free the wrong chunk 
	//	for (int i=0; i<8; ++i) {
	//		if (svo.root->nodes[0].children[i] != 0) {
	//			recurse_free(svo, svo.root->nodes[0].children[i], svo.root->nodes[0].leaf_mask & (1u << i));
	//		}
	//	}
	//
	//	// replace old node
	//	memcpy(&svo.root->nodes[0], &new_node, sizeof(Node));
	//
	//	// actually move root
	//	svo.root->pos = pos;
	}

	void recurse_draw (SVO& svo, Chunk* chunk, TypedVoxel vox, int3 pos, int scale, float3 player_pos) {
		float size = (float)(1 << scale);
		
		float3 pos_rel = player_pos - (float3)pos;
		float dist = max_component(abs(clamp(pos_rel, 0, size) - pos_rel));
		if (dist > svo.debug_draw_octree_range) return;

		auto col = cols[scale % ARRLEN(cols)];
		if ((vox.type != BLOCK_ID || vox.value > (svo.debug_draw_air ? B_NULL : B_AIR)) && scale <= svo.debug_draw_octree_max) {
			debug_graphics->push_wire_cube((float3)pos + 0.5f*size, size - 2 * svo.debug_draw_inset, col);
		}

		int child_scale = scale - 1;
		if (vox.type != BLOCK_ID && child_scale >= svo.debug_draw_octree_min) {

			uint32_t child_idx = vox.value;
			if (vox.type == CHUNK_PTR) {
				assert(vox.value != 0);
				chunk = &svo.allocator.chunks[vox.value];
				child_idx = 0;
			}

			Node* node = &chunk->nodes[child_idx];

			for (int i=0; i<8; ++i) {
				int3 child_pos = pos + (children_pos[i] << child_scale);

				auto child = node->get_child(i);

				recurse_draw(svo, chunk, child, child_pos, child_scale, player_pos);
			}
		}
	}

	void ChunkLoadJob::execute () {
		wg.generate_chunk(chunk, svo);
	}
	void ChunkLoadJob::finalize () {
		ZoneScoped;

		int3 pos = chunk->pos;
		int scale = chunk->scale;
		
		switch (load_type) {
			case LoadOp::CREATE: {
				//clog("apply CREATE to %2d : %+7d,%+7d,%+7d", scale, pos.x,pos.y,pos.z);

				assert(scale == svo.root->scale-1);

				svo.octree_write(pos.x, pos.y, pos.z, scale, { CHUNK_PTR, svo.allocator.indexof(chunk) });
				chunk->flags |= SVO_DIRTY | MESH_DIRTY;

				svo.pending_chunks.remove(pos, scale);
				svo.chunks.insert(pos, scale, chunk);

			} break;

			case LoadOp::SPLIT: {
				//clog("apply SPLIT to %2d : %+7d,%+7d,%+7d", scale, pos.x,pos.y,pos.z);

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
					auto p = siblings[i]->pos;
					svo.octree_write(p.x, p.y, p.z, siblings[i]->scale, { CHUNK_PTR, svo.allocator.indexof(siblings[i]) });
					siblings[i]->flags |= SVO_DIRTY | MESH_DIRTY;
				}
			} break;

			case LoadOp::MERGE: {
				//clog("apply MERGE to %2d : %+7d,%+7d,%+7d", scale, pos.x,pos.y,pos.z);
				
				// delete children
				for (int i=0; i<8; ++i) {
					int3 child_pos = pos + (children_pos[i] << scale-1);
					auto child = svo.chunks.remove(child_pos, scale-1);
					free_chunk(svo, child);
				}
			
				svo.octree_write(pos.x, pos.y, pos.z, scale, { CHUNK_PTR, svo.allocator.indexof(chunk) });
				chunk->flags |= SVO_DIRTY | MESH_DIRTY;

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

	void RemeshChunkJob::finalize () {
		ZoneScoped;

		static constexpr uintptr_t MESH_PAGING = 4096; // round up sizes so that not every single vertex addition triggers a realloc

		uintptr_t old_count = (uintptr_t)chunk->opaque_vertex_count   + chunk->transparent_vertex_count;
		uintptr_t new_count = opaque_mesh.size() + transparent_mesh.size();

		uintptr_t old_size = round_up_pot((uintptr_t)old_count * sizeof(VoxelVertex), MESH_PAGING);
		uintptr_t new_size = round_up_pot((uintptr_t)new_count * sizeof(VoxelVertex), MESH_PAGING);

		//// reallocate buffer if size changed
		//if (old_size != new_size) {
		//	if (chunk->gl_mesh) {
		//		glDeleteBuffers(1, &chunk->gl_mesh);
		//		chunk->gl_mesh = 0;
		//	}
		//
		//	if (new_size > 0) {
		//		glGenBuffers(1, &chunk->gl_mesh);
		//
		//		glBindBuffer(GL_ARRAY_BUFFER, chunk->gl_mesh);
		//		glBufferStorage(GL_ARRAY_BUFFER, new_size, nullptr, GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
		//	}
		//
		//	chunk->opaque_vertex_count = (uint32_t)opaque_mesh.size();
		//	chunk->transparent_vertex_count = (uint32_t)transparent_mesh.size();
		//} else {
		//	glBindBuffer(GL_ARRAY_BUFFER, chunk->gl_mesh);
		//}
		//
		//if (new_size > 0) {
		//	// copy data
		//	void* data = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
		//
		//	memcpy((VoxelVertex*)data,						opaque_mesh.data(),			opaque_mesh.size() * sizeof(VoxelVertex));
		//	memcpy((VoxelVertex*)data + opaque_mesh.size(),	transparent_mesh.data(),	transparent_mesh.size() * sizeof(VoxelVertex));
		//
		//	glUnmapBuffer(GL_ARRAY_BUFFER);
		//}

		// reallocate buffer if size changed
		if (old_size != new_size) {
			if (new_size > 0) {
				if (!chunk->gl_mesh)
					glGenBuffers(1, &chunk->gl_mesh);
		
				glBindBuffer(GL_ARRAY_BUFFER, chunk->gl_mesh);
				glBufferData(GL_ARRAY_BUFFER, new_size, nullptr, GL_DYNAMIC_DRAW);
			}
		
			chunk->opaque_vertex_count = (uint32_t)opaque_mesh.size();
			chunk->transparent_vertex_count = (uint32_t)transparent_mesh.size();
		} else {
			glBindBuffer(GL_ARRAY_BUFFER, chunk->gl_mesh);
		}
		
		if (opaque_mesh.size() > 0)
			glBufferSubData(GL_ARRAY_BUFFER, 0										 , opaque_mesh.size()      * sizeof(VoxelVertex), opaque_mesh.data());
		if (transparent_mesh.size() > 0)
			glBufferSubData(GL_ARRAY_BUFFER, opaque_mesh.size() * sizeof(VoxelVertex), transparent_mesh.size() * sizeof(VoxelVertex), transparent_mesh.data());
		
		//glBindBuffer(GL_VERTEX_ARRAY, 0);
	}

	void SVO::update_chunk_loading (Player& player, WorldGenerator& world_gen) {
		ZoneScoped;

		std::vector<LoadOp> ops_to_queue;
		{
			ZoneScopedN("chunk iteration");

			for (int i=0; i<8; ++i) {
				int scale = root->scale -1;
				int3 pos = root->pos + (children_pos[i] << scale);

				auto vox = root->nodes[0].get_child(i);
				if (vox.type == BLOCK_ID && vox.value == B_NULL) {
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

				assert(dist >= 0 && load_lod_start >= 0 && load_lod_unit > 0);

				float lg = log2f( (dist - load_lod_start) / load_lod_unit );
				int lod = max(floori(lg), 0);

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

		{ // queue chunk loads
			ZoneScopedN("queue chunk loads");

			int count = clamp(cap_chunk_load - (int)pending_chunks.count(), 0, (int)ops_to_queue.size());
			std::vector<std::unique_ptr<ThreadingJob>> jobs;

			for (auto& op : ops_to_queue) {
				if ((int)jobs.size() >= count)
					break;

				ZoneScopedN("queue chunk load");

				switch (op.type) {
					case LoadOp::CREATE: {
						auto* chunk = allocator.alloc_chunk(op.pos, op.scale);

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

							auto* chunk = allocator.alloc_chunk(child_pos, child_scale);

							pending_chunks.insert(child_pos, child_scale, chunk);

							//clog("queue SPLIT for %2d : %+7d,%+7d,%+7d", child_scale, child_pos.x,child_pos.y,child_pos.z);
							jobs.emplace_back( std::make_unique<ChunkLoadJob>(chunk, *this, world_gen, op.type) );
						}
					} break;

					case LoadOp::MERGE: {
						// merge
						auto* chunk = allocator.alloc_chunk(op.pos, op.scale);

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

			background_threadpool.jobs.push_n(jobs.data(), jobs.size());
		}
	}

	void SVO::update_chunk_gpu_data (Graphics& graphics, WorldGenerator& world_gen) {
		ZoneScoped;

		std::vector<std::unique_ptr<ThreadingJob>> remeshing_jobs;
		{
			ZoneScopedN("Upload svo data & collect chunks to remesh");

			//glBindBuffer(GL_SHADER_STORAGE_BUFFER, allocator.node_ptrs_ssbo);

			uint32_t commited_chunks = allocator.comitted_chunk_count();

			//bool realloc_node_ptrs = allocator.node_ptrs_ssbo_length != commited_chunks;
			//if (realloc_node_ptrs) {
			//	glBufferData(GL_SHADER_STORAGE_BUFFER, commited_chunks * sizeof(GLuint64EXT), nullptr, GL_DYNAMIC_DRAW);
			//	allocator.node_ptrs_ssbo_length = commited_chunks;
			//}
			//
			//GLuint64EXT* node_ptrs = (GLuint64EXT*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
			//glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

			// realloc chunk svo ssbo if needed, get new ptr and update ptr in node_ptrs
			for (uint32_t i=0; i<commited_chunks; ++i) {
				Chunk* chunk = &allocator.chunks[i];
				if (!allocator.chunk_is_allocated(i)) continue;

				if (i > 0 && (chunk->flags & SVO_DIRTY))
					assert(chunk->flags & MESH_DIRTY);

				//if (chunk->flags & SVO_DIRTY) {
				//	ZoneScopedN("chunk svo upload");
				//
				//	assert((chunk->svo_data_size == 0) == (chunk->gl_svo_data == 0));
				//
				//	// reallocate buffer if size changed (only on page boundries, not on every node alloc)
				//	if (chunk->svo_data_size != chunk->commit_ptr) {
				//		if (chunk->gl_svo_data) {
				//			glDeleteBuffers(1, &chunk->gl_svo_data);
				//			chunk->gl_svo_data = 0;
				//		}
				//
				//		if (chunk->commit_ptr != 0) {
				//			glGenBuffers(1, &chunk->gl_svo_data);
				//
				//			glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk->gl_svo_data);
				//			glBufferStorage(GL_SHADER_STORAGE_BUFFER, chunk->commit_ptr, nullptr, GL_MAP_WRITE_BIT | GL_DYNAMIC_STORAGE_BIT);
				//		}
				//
				//		chunk->svo_data_size = chunk->commit_ptr;
				//	} else {
				//		glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunk->gl_svo_data);
				//	}
				//
				//	// copy data
				//	// TODO: DOn't do this, this blocks! buffer orphan might be the correct way to do this, but I don't know if that works with glGetBufferParameterui64vNV
				//	void* data = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
				//	memcpy(data, chunk->nodes, chunk->commit_ptr);
				//	glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
				//
				//	// get gpu ptr
				//	if (!glIsBufferResidentNV(GL_SHADER_STORAGE_BUFFER))
				//		glMakeBufferResidentNV(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
				//
				//	glGetBufferParameterui64vNV(GL_SHADER_STORAGE_BUFFER, GL_BUFFER_GPU_ADDRESS_NV, &chunk->gl_svo_data_ptr);
				//
				//	//glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
				//}
				//
				//if (realloc_node_ptrs || (chunk->flags & SVO_DIRTY)) {
				//	node_ptrs[ allocator.indexof(chunk) ] = chunk->gl_svo_data_ptr;
				//}

				if (i > 0 && (chunk->flags & MESH_DIRTY)) { // never mesh root
					// queue remeshing
					remeshing_jobs.emplace_back( std::make_unique<RemeshChunkJob>(chunk, *this, graphics, world_gen.seed) );

					float size = (float)(1 << chunk->scale);
					debug_graphics->push_wire_cube((float3)chunk->pos + 0.5f * size, size * 0.995f, lrgba(1,0,0,1));
				}

				chunk->flags &= ~(SVO_DIRTY|MESH_DIRTY);
			}

			//glBindBuffer(GL_SHADER_STORAGE_BUFFER, allocator.node_ptrs_ssbo);
			//glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
			//glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
		
		{ //
			ZoneScopedN("Multithreaded remesh");

			parallelism_threadpool.jobs.push_n(remeshing_jobs.data(), remeshing_jobs.size());

			parallelism_threadpool.contribute_work();

			size_t results_count = 0;
			while (results_count < remeshing_jobs.size()) {
				ZoneScopedN("dequeue results");

				std::unique_ptr<ThreadingJob> results[64];
				size_t count = parallelism_threadpool.results.pop_n_wait(results, 1, ARRLEN(results));

				for (size_t i=0; i<count; ++i) {
					results[i]->finalize();
				}

				results_count += count;
			}
		}
	}

	void SVO::update_chunk_loading_and_meshing (Player& player, WorldGenerator& world_gen, Graphics& graphics) {
		ZoneScoped;

		update_root_move(*this, player);

		{ // finish first, so that pending_chunks becomes smaller before we cap the newly queued ones to limit this size
			ZoneScopedN("finish chunkgen jobs");

			static constexpr int finalize_cap = 64;

			std::unique_ptr<ThreadingJob> results [finalize_cap];
			size_t count = background_threadpool.results.pop_n(results, ARRLEN(results));

			for (size_t i=0; i<count; ++i) {
				results[i]->finalize();
			}
		}

		update_chunk_loading(player, world_gen);
		update_chunk_gpu_data(graphics, world_gen);

		if (debug_draw_svo) {
			ZoneScopedN("SVO::debug_draw");
		
			recurse_draw(*this, root, { NODE_PTR, 0 }, root->pos, root->scale, player.pos);
		}

		if (debug_draw_chunks) {
			ZoneScopedN("SVO::debug_draw_chunks");

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

		if (debug_spam_place_block) {
			for (int i=0; i<debug_spam_place_block_per_frame; ++i) {
				int3 pos = roundi(random.normal3(10, float3(0,0,20)));
				octree_write(pos.x, pos.y, pos.z, 0, { BLOCK_ID, B_LEAVES });
			}
		}
	}
}
