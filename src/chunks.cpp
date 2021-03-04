#include "common.hpp"
#include "chunks.hpp"
#include "chunk_mesher.hpp"
#include "game.hpp"
#include "world_generator.hpp"
#include "voxel_light.hpp"
#include "chunk_mesher.hpp"

//#pragma optimize("", off)

//// Voxel system

void Chunks::destroy () {
	// wait for all jobs to be completed to be able to safely recreate a new chunks with the same positions again later
	background_threadpool.flush();

	for (chunk_id cid=0; cid < chunks.slots.alloc_end; ++cid) {
		if (chunks[cid].flags != 0)
			free_chunk(cid);
	}
	assert(chunks        .count == 0);
	assert(subchunk_nodes.count == 0);
	assert(subchunks     .count == 0);

	chunks_map.clear();
	queued_chunks.clear();
}

void Chunks::init_voxels (Chunk& chunk) {
	chunk.flags |= Chunk::SPARSE_VOXELS;
	chunk.voxel_data = B_NULL;
}
void Chunks::free_voxels (Chunk& c) {
	ZoneScoped;

	if (c.flags & Chunk::SPARSE_VOXELS)
		return; // sparse chunk

	auto& sc64 = subchunk_nodes[c.voxel_data];

	for (uint32_t i=0; i<64; ++i) {
		if (sc64.children_mask & (1ull << i)) {
			auto& sc16 = subchunk_nodes[ sc64.children[i] ];

			for (uint32_t j=0; j<64; ++j) {
				if (sc16.children_mask & (1ull << j)) {
					auto& sc4 = subchunks[ sc16.children[j] ];
					DBG_MEMSET(&sc4, DBG_MEMSET_FREED, sizeof(sc4));
					subchunks.free(sc16.children[j]);
				}
			}

			DBG_MEMSET(&sc16, DBG_MEMSET_FREED, sizeof(sc16));
			subchunk_nodes.free(sc64.children[i]);
		}
	}

	DBG_MEMSET(&sc64, DBG_MEMSET_FREED, sizeof(sc64));
	subchunk_nodes.free(c.voxel_data);
}

void create_subchunk_node (Chunks& chunks, uint32_t* pchild, block_id data) {
	*pchild = chunks.subchunk_nodes.alloc();
	auto& node = chunks.subchunk_nodes[*pchild];

	node.children_mask = 0;
	for (int i=0; i<64; ++i)
		node.children[i] = data;
}
void create_subchunk (Chunks& chunks, uint32_t* pchild, block_id data) {
	*pchild = chunks.subchunks.alloc();
	auto& node = chunks.subchunks[*pchild];

	for (int i=0; i<64; ++i)
		node.voxels[i] = data;
}

block_id Chunks::read_block (int x, int y, int z) {
	//ZoneScoped;

	int bx, by, bz;
	int3 cpos;
	CHUNK_BLOCK_POS(x,y,z, cpos.x,cpos.y,cpos.z, bx,by,bz);

	chunk_id cid = query_chunk(cpos);
	if (cid == U16_NULL)
		return B_NULL;

	return read_block(bx,by,bz, &chunks[cid]);
}
void Chunks::write_block (int x, int y, int z, block_id data) {
	//ZoneScoped;

	int bx, by, bz;
	int3 cpos;
	CHUNK_BLOCK_POS(x,y,z, cpos.x,cpos.y,cpos.z, bx,by,bz);

	chunk_id cid = query_chunk(cpos);

	//assert(chunk && (chunk->flags & Chunk::LOADED)); // out of bounds writes happen on digging in unloaded chunks
	if (cid == U16_NULL)
		return;

	write_block(bx,by,bz, &chunks[cid], data);
}

block_id Chunks::read_block (int x, int y, int z, Chunk const* c) {
	assert(c->flags != 0);

	if (c->flags & Chunk::SPARSE_VOXELS)
		return (block_id)c->voxel_data; // sparse chunk

	//
	auto& sc64 = subchunk_nodes[c->voxel_data];

	uint32_t sc64i = SUBCHUNK_IDX((x >> 4) & 3, (y >> 4) & 3, (z >> 4) & 3);
	if ((sc64.children_mask & (1ull << sc64i)) == 0)
		return (block_id)sc64.children[sc64i];

	//
	auto& sc16 = subchunk_nodes[ sc64.children[sc64i] ];

	uint32_t sc16i = SUBCHUNK_IDX((x >> 2) & 3, (y >> 2) & 3, (z >> 2) & 3);
	if ((sc16.children_mask & (1ull << sc16i)) == 0)
		return (block_id)sc16.children[sc16i];

	//
	auto& sc4 = subchunks[ sc16.children[sc16i] ];

	uint32_t sc4i = SUBCHUNK_IDX(x & 3, y & 3, z & 3);
	return sc4.voxels[sc4i];
}
void Chunks::write_block (int x, int y, int z, Chunk* c, block_id data) {
	assert(c->flags != 0);

	if (c->flags & Chunk::SPARSE_VOXELS) {
		if ((block_id)c->voxel_data == data) return; // chunk stays sparse
		// densify chunk
		create_subchunk_node(*this, &c->voxel_data, c->voxel_data);
		c->flags &= ~Chunk::SPARSE_VOXELS;
	}
	
	//
	auto& sc64 = subchunk_nodes[c->voxel_data];

	uint32_t sc64i = SUBCHUNK_IDX((x >> 4) & 3, (y >> 4) & 3, (z >> 4) & 3);
	if ((sc64.children_mask & (1ull << sc64i)) == 0) {
		if ((block_id)sc64.children[sc64i] == data) return; // node stays sparse
		create_subchunk_node(*this, &sc64.children[sc64i], (block_id)sc64.children[sc64i]);
		sc64.children_mask |= (1ull << sc64i);
	}

	//
	auto& sc16 = subchunk_nodes[ sc64.children[sc64i] ];

	uint32_t sc16i = SUBCHUNK_IDX((x >> 2) & 3, (y >> 2) & 3, (z >> 2) & 3);
	if ((sc16.children_mask & (1ull << sc16i)) == 0) {
		if ((block_id)sc16.children[sc16i] == data) return; // node stays sparse
		create_subchunk(*this, &sc16.children[sc16i], (block_id)sc16.children[sc16i]);
		sc16.children_mask |= (1ull << sc16i);
	}

	//
	auto& sc4 = subchunks[ sc16.children[sc16i] ];

	uint32_t sc4i = SUBCHUNK_IDX(x & 3, y & 3, z & 3);
	sc4.voxels[sc4i] = data;
	
	write_block_update_chunk_flags(x,y,z, c);
}

void Chunks::write_block_update_chunk_flags (int x, int y, int z, Chunk* c) {
	c->flags |= Chunk::REMESH|Chunk::VOXELS_DIRTY;

	auto flag_neighbour = [&] (int x, int y, int z) {
		//auto nid = chunks_arr.checked_get(x,y,z);
		auto nid = query_chunk(int3(x,y,z));
		if (nid != U16_NULL) {
			assert(chunks[nid].flags != 0);
			chunks[nid].flags |= Chunk::REMESH;
		}
	};

	// Set remesh flags for neighbours where needed
	if (x == 0           ) flag_neighbour(c->pos.x-1, c->pos.y,   c->pos.z  );
	if (x == CHUNK_SIZE-1) flag_neighbour(c->pos.x+1, c->pos.y,   c->pos.z  );
	if (y == 0           ) flag_neighbour(c->pos.x,   c->pos.y-1, c->pos.z  );
	if (y == CHUNK_SIZE-1) flag_neighbour(c->pos.x,   c->pos.y+1, c->pos.z  );
	if (z == 0           ) flag_neighbour(c->pos.x,   c->pos.y,   c->pos.z-1);
	if (z == CHUNK_SIZE-1) flag_neighbour(c->pos.x,   c->pos.y,   c->pos.z+1);
}

/*
bool checked_sparsify_subtree (Chunks& chunks, int scale, uint32_t* pdata) {
	if (scale == 0) {
		// leaf subchunk
		auto& sc = chunks.subchunks[*pdata];

		bool sparse = true;
		block_id sparse_val = sc.voxels[0];
		for (int i=1; i<64; ++i) {
			if (sc.voxels[i] != sparse_val) {
				sparse = false;
				break;
			}
		}

		if (sparse) *pdata = sparse_val;
		return sparse;
	} else {
		// internal subchunk nodes
		*pdata = node_count;
		auto& n = nodes[node_count++];

		n.children_mask = 0;

		for (int i=0; i<64; ++i) {
			int cz = (i >> 4);
			int cy = (i >> 2) & 3;
			int cx = (i     ) & 3;

			bool sparse = sparsify_subtree(x*4+cx, y*4+cy, z*4+cz, scale-1, &n.children[i]);
			if (!sparse) n.children_mask |= 1ull << i;
		}

		if (n.children_mask != 0)
			return false; // any child not sparse -> we can't be sparse

						  // all children sparse; check they are all sparse with the same value
		bool sparse = true;
		uint32_t sparse_val = n.children[0];

		for (int i=0; i<64; ++i) {
			if (n.children[i] != sparse_val) {
				sparse = false;
				break;
			}
		}

		if (sparse) *pdata = sparse_val;
		return sparse;
	}
}
*/
void Chunks::checked_sparsify_chunk (Chunk& c) {
	if (c.flags & Chunk::SPARSE_VOXELS)
		return; // chunk already sparse
	ZoneScoped;

	//if (checked_sparsify_subtree(*this, 2, &root_data))
	//	c.flags |= Chunk::SPARSE_VOXELS;
}

//// Chunk system

chunk_id Chunks::alloc_chunk (int3 pos) {
	ZoneScoped;

	chunk_id id = chunks.alloc();
	auto& chunk = chunks[id];

	{ // init
		chunk.flags = Chunk::ALLOCATED;
		chunk.pos = pos;
		//chunk.refcount = 0;
		init_voxels(chunk);
		chunk.init_meshes();
	}

	return id;
}
void Chunks::free_chunk (chunk_id id) {
	ZoneScoped;
	auto& chunk = chunks[id];

	free_voxels(chunk);

	free_slices(chunk.opaque_mesh_slices);
	free_slices(chunk.transp_mesh_slices);

	{ // link neigbour ptrs
		for (int i=0; i<6; ++i) {
			auto nid = chunk.neighbours[i];
			if (nid != U16_NULL) {
				chunks[nid].neighbours[i^1] = U16_NULL;
				chunks[nid].flags |= (Chunk::Flags)(Chunk::NEIGHBOUR0_NULL << (i^1));
			}
		}
	}

	memset(&chunk, 0, sizeof(Chunk)); // zero chunk, flags will now indicate that chunk is unallocated
	chunks.free(id);
}

#include "immintrin.h"

void Chunks::update_chunk_loading (Game& game) {
	ZoneScoped;
	
	// clamp for sanity
	load_radius = clamp(load_radius, 0.0f, 20000.0f);
	unload_hyster = clamp(unload_hyster, 0.0f, 20000.0f);

	float unload_dist = load_radius + unload_hyster;
	float load_dist_sqr = load_radius * load_radius;
	float unload_dist_sqr = unload_dist * unload_dist;

	static constexpr float BUCKET_FAC = (0.5f) / (CHUNK_SIZE*CHUNK_SIZE);
	{
		float radius = load_radius;

		chunks_to_generate.clear(); // clear all inner vectors
		chunks_to_generate.shrink_to_fit(); // delete all inner vectors to avoid constant memory alloc when loading idle
		chunks_to_generate.resize((int)(radius*radius * BUCKET_FAC) + 1);
	}
	auto add_chunk_to_generate = [&] (int3 chunk_pos, float dist_sqr, int phase) {
		int bucketi = (int)(dist_sqr * BUCKET_FAC);
		//assert(bucketi >= 0 && bucketi < chunks_to_generate.size());
		bucketi = clamp(bucketi, 0, (int)chunks_to_generate.size() -1);
		chunks_to_generate[bucketi].push_back({ chunk_pos });
	};

	{
		ZoneScopedN("iterate chunk loading");
		
		if (visualize_chunks) {
			if (visualize_radius) {
				g_debugdraw.wire_sphere(game.player.pos, load_radius, DBG_RADIUS_COL);

				//auto sz = (float)(chunks_arr.size * CHUNK_SIZE);
				//g_debugdraw.wire_cube((float3)chunks_arr.pos * CHUNK_SIZE + sz/2, sz, DBG_CHUNK_ARRAY_COL);
			}

			for (auto& chunk_pos : queued_chunks) {
				g_debugdraw.wire_cube(((float3)chunk_pos + 0.5f) * CHUNK_SIZE, (float3)CHUNK_SIZE * 0.6f, DBG_STAGE1_COL);
			}
		}

	#if 0
		// chunk distance based on dist to box, ie closest point in box is used as distance

		float3 const& player_pos = game.player.pos;

		auto chunk_dist_sqr = [&] (int3 const& pos) {
			float pos_relx = player_pos.x - pos.x * (float)CHUNK_SIZE;
			float pos_rely = player_pos.y - pos.y * (float)CHUNK_SIZE;
			float pos_relz = player_pos.z - pos.z * (float)CHUNK_SIZE;

			float nearestx = clamp(pos_relx, 0.0f, (float)CHUNK_SIZE);
			float nearesty = clamp(pos_rely, 0.0f, (float)CHUNK_SIZE);
			float nearestz = clamp(pos_relz, 0.0f, (float)CHUNK_SIZE);

			float offsx = nearestx - pos_relx;
			float offsy = nearesty - pos_rely;
			float offsz = nearestz - pos_relz;

			return offsx*offsx + offsy*offsy + offsz*offsz;
		};
	#else
		// simplified distance only based on center
	#if 0
		float sz = (float)CHUNK_SIZE;

		float3 const& player_pos = game.player.pos;
		float3 dist_base = sz/2 - game.player.pos;

		auto chunk_dist_sqr = [&] (int3 const& pos) {

			// combine half size add and point sub because these could be optimized as loop-invariants
			float offsx = pos.x * sz + dist_base.x;
			float offsy = pos.y * sz + dist_base.y;
			float offsz = pos.z * sz + dist_base.z;

			return offsx*offsx + offsy*offsy + offsz*offsz;
		};
	#else
		auto player_pos = _mm_load_ps(&game.player.pos.x);

		auto sz = _mm_set1_ps((float)CHUNK_SIZE);
		auto szh = _mm_set1_ps((float)CHUNK_SIZE/2);

		auto dist_base = _mm_sub_ps(szh, player_pos);

		auto chunk_dist_sqr = [&] (int3 const& pos) {

			auto ipos = _mm_load_si128((__m128i*)&pos.x);
			auto fpos = _mm_cvtepi32_ps(ipos);

			auto offs = _mm_fmadd_ps(fpos, sz, dist_base);
			auto dp = _mm_dp_ps(offs, offs, 0x71);

			return _mm_cvtss_f32(dp);
		};
	#endif

	#endif

		{
			int3 pos = floori(game.player.pos / CHUNK_SIZE);
			if (chunks_map.find(pos) == chunks_map.end()) { // chunk not yet loaded
				if (queued_chunks.find(pos) == queued_chunks.end()) // chunk not yet queued for worldgen
					add_chunk_to_generate(pos, 0, 1);
			}
		}

		for (chunk_id cid=0; cid < end(); ++cid) {
			auto& chunk = chunks[cid];
			if (chunk.flags == 0) continue;
			chunk._validate_flags();

			float dist_sqr = chunk_dist_sqr(chunk.pos);

			if (dist_sqr <= unload_dist_sqr) {
				// check flags to see if any of the neighbours are still null, which is faster than checking the array
				if (chunk.flags & Chunk::NEIGHBOUR_NULL_MASK) {
					for (int i=0; i<6; ++i) {
						auto nid = chunk.neighbours[i];
						if (nid == U16_NULL) {
							// neighbour chunk not yet loaded
							auto npos = chunk.pos + NEIGHBOURS[i];
							float ndist_sqr = chunk_dist_sqr(npos);
							
							if (ndist_sqr <= load_dist_sqr && queued_chunks.find(npos) == queued_chunks.end()) // chunk not yet queued for worldgen
								add_chunk_to_generate(npos, ndist_sqr, 1); // note: this creates duplicates because we arrive at the same chunk through two ways
						}
					}
				}
			} else {
				// chunk outside unload radius
				chunks_map.erase(chunk.pos);
				free_chunk(cid);
			}
		}
	}

	{
		ZoneScopedN("process jobs");

		auto update_chunk_phase2_generation = [&] (Chunk& chunk) {
			if (chunk.flags & Chunk::LOADED_PHASE2) return;

			worldgen::Neighbours n;

			// check if neighbours are ready yet and build 3x3x3 LUT for faster lookups in worldgen::object_pass() at the same time
			for (int z=-1; z<=1; ++z)
			for (int y=-1; y<=1; ++y)
			for (int x=-1; x<=1; ++x) {
				chunk_id nid = query_chunk(chunk.pos + int3(x,y,z));

				if (nid == U16_NULL)
					return; // neighbours not read yet

				n.neighbours[z+1][y+1][x+1] = &chunks[nid];
			}

			worldgen::object_pass(*this, chunk, n, &game._threads_world_gen);

			chunk.flags |= Chunk::LOADED_PHASE2 | Chunk::REMESH;
		};

		{
			ZoneScopedN("pop jobs");

			static constexpr int MAX_REMESH_PER_THREAD_FRAME = 3;
			static const int LOAD_LIMIT = parallelism_threads * MAX_REMESH_PER_THREAD_FRAME;

			std::unique_ptr<WorldgenJob> jobs[64];

			int count = (int)background_threadpool.results.pop_n(jobs, std::min((size_t)LOAD_LIMIT, ARRLEN(jobs)));
			for (int jobi=0; jobi<count; ++jobi) {
				auto job = std::move(jobs[jobi]);
				auto& chunk_pos = job->noise_pass.chunk_pos;

				queued_chunks.erase(job->noise_pass.chunk_pos);

				auto cid = alloc_chunk(chunk_pos);
				auto& chunk = chunks[cid];
				chunks_map.emplace(chunk.pos, cid);

				worldgen::store_voxels_from_worldgen(*this, chunk, job->noise_pass);

				chunk.flags |= Chunk::REMESH;

				// link neighbour ptrs and flag neighbours to be remeshed
				for (int ni=0; ni<6; ++ni) {
					auto nid = query_chunk(chunk_pos + NEIGHBOURS[ni]);

					chunk.neighbours[ni] = nid;
					if (nid == U16_NULL) {
						chunk.flags |= (Chunk::Flags)(Chunk::NEIGHBOUR0_NULL << ni);
					} else {
						assert(chunks[nid].flags != 0);
						chunks[nid].flags |= Chunk::REMESH;
						chunks[nid].flags &= ~(Chunk::Flags)(Chunk::NEIGHBOUR0_NULL << (ni^1));
						chunks[nid].neighbours[ni^1] = cid;
					}
				}

				// run phase2 generation where required
				// TODO: this could possibly be accelerated by keeping a counter of how many neighbours have are ready,
				// but don't bother because this should be fast enough and will be less bugprone (and not require storing a counter)
				for (auto& offs : FULL_NEIGHBOURS) {
					chunk_id nid = query_chunk(chunk.pos + offs);
					if (nid != U16_NULL)
						update_chunk_phase2_generation(chunks[nid]);
				}
			}
		}

		{
			ZoneScopedN("push jobs");

			static constexpr int QUEUE_LIMIT = 64; // 256
			std::unique_ptr<WorldgenJob> jobs[QUEUE_LIMIT];

			// Process bucket-sorted chunks_to_generate in order
			//  and push jobs until threadpool has at max background_queued_count jobs (ignore the remaining chunks, which will get pushed as soon as jobs are completed)
			int bucket=0, j=0; // sort bucket indices

			int max_count = (int)ARRLEN(jobs) - (int)queued_chunks.size();
			int count = 0;
			for (int bucket=0; count < max_count && bucket < (int)chunks_to_generate.size(); ++bucket) {
				for (int i=0; count < max_count && i < (int)chunks_to_generate[bucket].size(); ++i) {
					auto genchunk = chunks_to_generate[bucket][i];

					if (queued_chunks.find(genchunk.pos) != queued_chunks.end()) continue; // remove duplicates generated by code above

					ZoneScopedN("phase 1 job");

					auto job = std::make_unique<WorldgenJob>(genchunk.pos, &game._threads_world_gen);
						
					jobs[count++] = std::move(job);

					queued_chunks.emplace(genchunk.pos);
				}
			}

			background_threadpool.jobs.push_n(jobs, count);

			{ // for imgui
				pending_chunks = 0; // chunks waiting to be queued
				for (auto& b : chunks_to_generate)
					pending_chunks += (uint32_t)b.size();
				pending_chunks -= count; // exclude chunks we have already queued
			}

			//TracyPlot("background_queued_count", (int64_t)background_queued_count);
		}
	}
}

void Chunks::update_chunk_meshing (Game& game) {
	ZoneScoped;

	std::vector<std::unique_ptr<RemeshChunkJob>> remesh_jobs;

	{
		ZoneScopedN("remesh iterate chunks");

		for (chunk_id id = 0; id<end(); ++id) {
			auto& chunk = chunks[id];
			chunk._validate_flags();
			
			if (chunk.flags & Chunk::VOXELS_DIRTY) {
				checked_sparsify_chunk(chunk);
				chunk.flags &= ~Chunk::VOXELS_DIRTY;
			}

			if (chunk.flags & Chunk::REMESH) {
				auto job = std::make_unique<RemeshChunkJob>(*this, &chunk, game.world_gen, mesh_world_border);
				remesh_jobs.emplace_back(std::move(job));
			}
		}
	}

	// remesh all chunks in parallel
	parallelism_threadpool.jobs.push_n(remesh_jobs.data(), remesh_jobs.size());

	{
		ZoneScopedN("remesh process slices");

		// upload remeshed slices and register them in chunk mesh
		auto process_slices = [&] (ChunkMeshData& remeshed, uint32_t* pvertex_count, slice_id* pslices) {
			ZoneScopedN("process_slices");

			*pvertex_count = remeshed.vertex_count();
			uint32_t remain_vertices = *pvertex_count;

			slice_id* prev_next = pslices;
			slice_id sliceid = *prev_next;

			int i = 0;
			while (remain_vertices > 0) {
				if (sliceid == U16_NULL) {
					sliceid = (slice_id)slices.alloc();
					slices[sliceid].next = U16_NULL;
					*prev_next = sliceid;
				}

				uint32_t count = std::min(remain_vertices, (uint32_t)CHUNK_SLICE_LENGTH);

				// queue data to be uploaded for sliceid, data stays valid (malloc'd) until it is processed by the renderer
				upload_slices.push_back({ sliceid, remeshed.slices[i++] });

				remain_vertices -= count;

				prev_next = &slices[sliceid].next;
				sliceid = *prev_next;
			}

			// free potentially remaining slices no longer needed
			free_slices(sliceid);

			// end linked list before the part that we freed
			*prev_next = U16_NULL;
		};

		for (size_t resi=0; resi < remesh_jobs.size();) {
			std::unique_ptr<RemeshChunkJob> results[64];
			size_t count = parallelism_threadpool.results.pop_n_wait(results, 1, ARRLEN(results));

			for (size_t i=0; i<count; ++i) {
				auto res = std::move(results[i]);

				process_slices(res->opaque_vertices, &res->chunk->opaque_mesh_vertex_count, &res->chunk->opaque_mesh_slices);
				process_slices(res->transp_vertices, &res->chunk->transp_mesh_vertex_count, &res->chunk->transp_mesh_slices);
				
				res->chunk->flags &= ~Chunk::REMESH;
			}

			resi += count;
		}
	}
}

void _count_subchunks (Chunks& chunks, int scale, int* counts, uint32_t node) {
	counts[scale]++;

	if (scale > 0) {
		auto& n = chunks.subchunk_nodes[node];

		for (int i=0; i<64; ++i) {
			if (n.children_mask & (1ull << i)) {
				_count_subchunks(chunks, scale-1, counts, n.children[i]);
			}
		}
	}
}

void Chunks::imgui (Renderer* renderer) {
	////

	ImGui::Checkbox("visualize_chunks", &visualize_chunks);
	ImGui::SameLine();
	ImGui::Checkbox("subchunks", &visualize_subchunks);
	ImGui::SameLine();
	ImGui::Checkbox("radius", &visualize_radius);

	//if (ImGui::BeginPopupContextWindow("Colors")) {
	//	imgui_ColorEdit("DBG_CHUNK_COL",			&DBG_CHUNK_COL);
	//	imgui_ColorEdit("DBG_STAGE1_COL",			&DBG_STAGE1_COL);
	//	imgui_ColorEdit("DBG_SPARSE_CHUNK_COL",		&DBG_SPARSE_CHUNK_COL);
	//	imgui_ColorEdit("DBG_CULLED_CHUNK_COL",		&DBG_CULLED_CHUNK_COL);
	//	imgui_ColorEdit("DBG_DENSE_SUBCHUNK_COL",	&DBG_DENSE_SUBCHUNK_COL);
	//	imgui_ColorEdit("DBG_RADIUS_COL",			&DBG_RADIUS_COL);
	//	imgui_ColorEdit("DBG_CHUNK_ARRAY_COL",		&DBG_CHUNK_ARRAY_COL);
	//	ImGui::EndPopup();
	//}

	ImGui::Checkbox("debug_frustrum_culling", &debug_frustrum_culling);

	ImGui::Spacing();
	ImGui::Checkbox("mesh_world_border", &mesh_world_border);

	ImGui::Spacing();
	ImGui::DragFloat("load_radius", &load_radius, 1, 0);
	ImGui::DragFloat("unload_hyster", &unload_hyster, 1, 0);

	////
	ImGui::Separator();

	{
		uint32_t final_chunks = chunks.count + (uint32_t)queued_chunks.size() + pending_chunks;
		
		ImGui::Text("chunk loading: %5d / %5d (%3.0f %%)", chunks.count, final_chunks, (float)chunks.count / final_chunks * 100);

		if (pending_chunks > 0) {
			std::string str = "(";
			for (auto& b : chunks_to_generate) {
				str = prints("%s%3d ", str.c_str(), b.size());
			}

			str += ")";

			ImGui::SameLine();
			ImGui::Text("  %s", str.c_str());
		}
	}
	
	////
	ImGui::Separator();

	uint64_t block_volume = chunks.count * (uint64_t)CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
	
	int subc_counts[3] = {};

	for (chunk_id id = 0; id < end(); ++id) {
		if ((chunks[id].flags & Chunk::ALLOCATED) == 0) continue;

		if ((chunks[id].flags & Chunk::SPARSE_VOXELS) == 0)
			_count_subchunks(*this, 2, subc_counts, chunks[id].voxel_data);
	}

	struct SubchunkLayer {
		int volume; // number of subchunks(_nodes) that could exist based on total loaded voxel volume
		int parent; // number of subchunks(_nodes) that could exist based on in the dense volume of the parent layer
		int dense;  // number of subchunks(_nodes) that are actually dense and exist in memory
	};
	SubchunkLayer layers[3];

	for (int i=2; i>=0; --i) {
		int per_chunk = CHUNK_SIZE / (4 << (i*2));
		layers[i].volume = chunks.count * per_chunk*per_chunk*per_chunk;
		layers[i].parent = i == 2 ? layers[i].volume : layers[i+1].dense * 64;
		layers[i].dense = subc_counts[i];
	}

	uint64_t total_vox_mem = chunks.commit_size() + subchunk_nodes.commit_size() + subchunks.commit_size();

	ImGui::Text("Chunks : %4d chunks  %7s M vox volume %4d KB chunk RAM (%6.2f %% usage)",
		chunks.count, format_thousands(block_volume / 1000000).c_str(),
		(int)(chunks.commit_size()/KB), chunks.usage() * 100);

	ImGui::Spacing();

	ImGui::Text("Subchunk tree: dense     volume     parent (   %% vol  %% parent)");

	for (int i=2; i>=0; --i) {
		int size = 4 << (i*2);
		
		ImGui::Text("layer %3d: %9s  %9s  %9s (%6.2f %%  %6.2f %%)", size,
			format_thousands(layers[i].dense).c_str(),
			format_thousands(layers[i].volume).c_str(),
			format_thousands(layers[i].parent).c_str(),
			(float)layers[i].dense / (float)layers[i].volume * 100,
			(float)layers[i].dense / (float)layers[i].parent * 100);
	}

	ImGui::Spacing();
	ImGui::Text("Memory  : %3d MB total RAM, %3d KB subchunk_nodes (%6.2f %% usage)  %3d MB subchunks (%6.2f %% usage)",
		total_vox_mem / MB,
		subchunk_nodes.commit_size() / KB, subchunk_nodes.usage() * 100,
		subchunks.commit_size() / MB, subchunks.usage() * 100);

	ImGui::Spacing();

	if (ImGui::TreeNode("chunks")) {
		for (chunk_id id=0; id<end(); ++id) {
			if ((chunks[id].flags & Chunk::ALLOCATED) == 0)
				ImGui::Text("[%5d] <not allocated>", id);
			else
				ImGui::Text("[%5d] %+4d,%+4d,%+4d - %2d, %2d slices", id, chunks[id].pos.x,chunks[id].pos.y,chunks[id].pos.z,
					_slices_count(chunks[id].opaque_mesh_vertex_count), _slices_count(chunks[id].transp_mesh_vertex_count));
		}
		ImGui::TreePop();
	}
	
	ImGui::Spacing();

	print_block_allocator(chunks, "chunks alloc");
	print_block_allocator(subchunk_nodes, "subchunk_nodes alloc");
	print_block_allocator(subchunks, "subchunks alloc");

	ImGui::Spacing();
	
#if 0
	if (ImGui::TreeNode("count_hash_collisions")) {
		size_t collisions = 0, max_bucket_size = 0, empty_buckets = 0;
		for (size_t i=0; i<pos_to_id.bucket_count(); ++i) {
			size_t c = pos_to_id.bucket_size(i);
			if (c > 1) collisions += c - 1;
			if (c == 0) empty_buckets++;
			max_bucket_size = std::max(max_bucket_size, c);
		}

		ImGui::Text("chunks: %5d  collisions: %d (buckets: %5d, max_bucket_size: %5d, empty_buckets: %5d)",
			chunks.count, collisions, pos_to_id.bucket_count(), max_bucket_size, empty_buckets);

		if (ImGui::TreeNode("bucket counts")) {
			for (size_t i=0; i<pos_to_id.bucket_count(); ++i)
				ImGui::Text("[%5d] %5d", i, pos_to_id.bucket_size(i));
			ImGui::TreePop();
		}

		ImGui::TreePop();
	}
#endif

	if (renderer)
		renderer->chunk_renderer_imgui(*this);
}

void _visualize_subchunks (Chunks& chunks, int scale, uint32_t node, int3 const pos) {
	float3 posf = (float3)pos;
	float size = (float)(4 << (scale*2));
	g_debugdraw.wire_cube(posf + size/2, (float3)size * 0.997f, DBG_SUBCHUNK_COLS[scale]);

	if (scale > 0) {
		auto& n = chunks.subchunk_nodes[node];

		for (int i=0; i<64; ++i) {
			if (n.children_mask & (1ull << i)) {
				int z = (i >> 4);
				int y = (i >> 2) & 3;
				int x = (i     ) & 3;
				_visualize_subchunks(chunks, scale-1, n.children[i],
					int3(pos.x + (x << (scale*2)), pos.y + (y << (scale*2)), pos.z + (z << (scale*2))));
			}
		}
	}
}

void Chunks::visualize_chunk (Chunk& chunk, bool empty, bool culled) {
	lrgba const* col = &DBG_CHUNK_COL;
	float size = 1.0f;

	if (debug_frustrum_culling) {
		if (empty) return;

		if (culled)
			col = &DBG_CULLED_CHUNK_COL;

		g_debugdraw.wire_cube(((float3)chunk.pos + 0.5f) * CHUNK_SIZE, (float3)CHUNK_SIZE * size * 0.997f, *col);

	} else if (visualize_chunks) {

		if (!visualize_subchunks) {
			if (chunk.flags & Chunk::LOADED_PHASE2) {
				col = &DBG_CHUNK_COL;
				if (chunk.flags & Chunk::SPARSE_VOXELS) {
					col = &DBG_SPARSE_CHUNK_COL;
				}
			} else {
				col = &DBG_STAGE1_COL;
			}

			g_debugdraw.wire_cube(((float3)chunk.pos + 0.5f) * CHUNK_SIZE, (float3)CHUNK_SIZE * size * 0.997f, *col);
		} else {
			if ((chunk.flags & Chunk::SPARSE_VOXELS) == 0)
				_visualize_subchunks(*this, 2, chunk.voxel_data, chunk.pos * CHUNK_SIZE);
		}
	}
}

#if 0
struct VoxelRaytrace {
	Chunks* chunks;

	float3 ray_pos;
	float3 ray_dir;
	float max_dist = 128;

	static constexpr block_id B_AIR = 1;

	int min_component (float3 v) {
		if (		v.x < v.y && v.x < v.z )	return 0;
		else if (	v.y < v.z )					return 1;
		else									return 2;
	}

	int get_step_face (int axis) {
		return axis*2 +(ray_dir[axis] >= 0.0 ? 0 : 1);
	}

	int3 coord;
	int step_size = 1;

	Chunk* chunk;
	uint32_t voxel_data;
	uint32_t subchunk_data;

	block_id read_voxel (int step_mask) {

		if (step_mask & ~CHUNK_SIZE_MASK) {
			ImGui::Text(">>> Chunk read");

			voxel_data = chunk->voxel_data;

			if (chunk->flags & Chunk::SPARSE_VOXELS) {
				step_size = CHUNK_SIZE;

				coord.x &= ~CHUNK_SIZE_MASK;
				coord.y &= ~CHUNK_SIZE_MASK;
				coord.z &= ~CHUNK_SIZE_MASK;

				return (block_id)voxel_data; // sparse chunk
			}
		}

		if (step_mask & ~SUBCHUNK_MASK) {
			ImGui::Text(">> Subchunk read");

			auto& dc = chunks->dense_chunks[voxel_data];

			uint32_t subchunk_i = SUBCHUNK_IDX(coord.x & CHUNK_SIZE_MASK, coord.y & CHUNK_SIZE_MASK, coord.z & CHUNK_SIZE_MASK);
			subchunk_data = dc.sparse_data[subchunk_i];

			if (dc.is_subchunk_sparse(subchunk_i)) {
				step_size = SUBCHUNK_SIZE;

				coord.x &= ~SUBCHUNK_MASK;
				coord.y &= ~SUBCHUNK_MASK;
				coord.z &= ~SUBCHUNK_MASK;

				return (block_id)subchunk_data; // sparse subchunk
			}
		}
		ImGui::Text("> Voxel read");

		auto& subchunk = chunks->dense_subchunks[subchunk_data];

		step_size = 1;
		uint32_t block_i = BLOCK_IDX(coord.x,coord.y,coord.z);
		return subchunk.voxels[block_i];
	}

	void run () {
		ImGui::DragFloat3("ray_pos", &ray_pos.x, 0.1f);
		ImGui::DragFloat3("ray_dir", &ray_dir.x, 0.1f);
		ImGui::DragFloat("max_dist", &max_dist, 0.1f);

		g_debugdraw.vector(ray_pos, ray_dir * max_dist, lrgba(1,0,0,1));

		coord = floori(ray_pos);

		{
			chunk_id cid = chunks->query_chunk(coord / CHUNK_SIZE);
			if (cid == U16_NULL)
				return;
			chunk = &chunks->chunks[cid];
		}

		float3 rdir; // reciprocal of ray dir
		rdir.x = ray_dir.x != 0.0f ? 1.0f / abs(ray_dir.x) : INF;
		rdir.y = ray_dir.y != 0.0f ? 1.0f / abs(ray_dir.y) : INF;
		rdir.z = ray_dir.z != 0.0f ? 1.0f / abs(ray_dir.z) : INF;

		int step_mask = -1;

		int iter = 0;
		while (iter < 100) {
			iter++;

			block_id bid = read_voxel(step_mask);

			g_debugdraw.wire_cube((float3)coord + ((float)step_size / 2), (float)step_size * 0.99f, lrgba(0,0,1,1));
			
			//ImGui::Text("%+3d,%+3d,%+3d: %s %d", coord.x, coord.y, coord.z,
			//	g_assets.block_types[bid].name.c_str(), step_size);
			
			if (bid != B_AIR)
				return;

			float3 rel = ray_pos - (float3)coord;

			float3 plane_offs;
			plane_offs.x = ray_dir.x > 0 ? step_size - rel.x : rel.x;
			plane_offs.y = ray_dir.y > 0 ? step_size - rel.y : rel.y;
			plane_offs.z = ray_dir.z > 0 ? step_size - rel.z : rel.z;

			float3 next = rdir * plane_offs;
			int axis = min_component(next);

			if (next[axis] > max_dist)
				break;

			float3 proj = ray_pos + ray_dir * next[axis];

			//lrgba col = lrgba(0,0,0,1);
			//col[axis] = 1;
			//g_debugdraw.pointx(proj, 0.1f, col);
			//
			//ImGui::Text("%c proj: %7.3f, %7.3f, %7.3f", "XYZ"[axis], proj.x, proj.y, proj.z);

			int3 old_coord = coord;

			proj[axis] += ray_dir[axis] > 0 ? 0.5f : -0.5f;
			coord = floori(proj);

			//ImGui::Text("---------------------");

			int3 step_maskv = coord ^ old_coord;
			step_mask = step_maskv.x | step_maskv.y | step_maskv.z;

			// handle step out of chunk by checking bits
			if (step_mask & ~CHUNK_SIZE_MASK) {
				chunk_id cid = chunk->neighbours[get_step_face(axis) ^ 1]; // ^1 flip dir
				if (cid == U16_NULL)
					return;
				chunk = &chunks->chunks[cid];
			}
		}
	}
};
#endif

void test_rayracy_voxels (Chunks& chunks, float3 ray_pos, float3 ray_dir) {
	//static VoxelRaytrace vr;
	//vr.chunks = &chunks;
	//vr.ray_pos = ray_pos;
	//vr.ray_dir = ray_dir;
	//vr.run();
}
