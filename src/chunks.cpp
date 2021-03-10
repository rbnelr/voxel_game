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
	assert(chunks.count == 0);
	assert(chunk_voxels.count == 0);
	assert(subchunks.count == 0);
	//chunks_arr = ScrollingArray<chunk_id>();

	chunks_map.clear();
	queued_chunks.clear();
}

void Chunks::free_voxels (chunk_id cid, Chunk& chunk) {
	ZoneScoped;

	auto& vox = chunk_voxels[cid];

	for (uint32_t i=0; i<CHUNK_SUBCHUNK_COUNT; ++i) {
		if (!vox.is_subchunk_sparse(i)) {
			auto& subchunk = subchunks[ vox.sparse_data[i] ];
			DBG_MEMSET(&subchunk, DBG_MEMSET_FREED, sizeof(subchunk));
			subchunks.free(vox.sparse_data[i]);
		}
	}

	DBG_MEMSET(&vox, DBG_MEMSET_FREED, sizeof(vox));
	chunk_voxels.free(cid);
}

block_id Chunks::read_block (int x, int y, int z) {
	//ZoneScoped;

	int bx, by, bz;
	int3 cpos;
	CHUNK_BLOCK_POS(x,y,z, cpos.x,cpos.y,cpos.z, bx,by,bz);

	//chunk_id cid = chunks_arr.checked_get(cpos.x,cpos.y,cpos.z);
	chunk_id cid = query_chunk(cpos);
	if (cid == U16_NULL)
		return B_NULL;

	return read_block(bx,by,bz, cid);
}

block_id Chunks::read_block (int x, int y, int z, chunk_id cid) {
	auto& vox = chunk_voxels[cid];

	uint32_t subchunk_i = SUBCHUNK_IDX(x,y,z);
	auto subchunk_val = vox.sparse_data[subchunk_i];

	if (vox.is_subchunk_sparse(subchunk_i))
		return CHECK_BLOCK( (block_id)subchunk_val ); // sparse subchunk

	auto& subchunk = subchunks[subchunk_val];

	auto blocki = BLOCK_IDX(x,y,z);
	assert(blocki >= 0 && blocki < SUBCHUNK_VOXEL_COUNT);
	auto block = subchunk.voxels[blocki];

	return CHECK_BLOCK(block); // dense subchunk
}

void Chunks::write_block (int x, int y, int z, block_id data) {
	//ZoneScoped;

	int bx, by, bz;
	int3 cpos;
	CHUNK_BLOCK_POS(x,y,z, cpos.x,cpos.y,cpos.z, bx,by,bz);

	//chunk_id cid = chunks_arr.checked_get(cx,cy,cz);
	chunk_id cid = query_chunk(cpos);
	
	//assert(chunk && (chunk->flags & Chunk::LOADED)); // out of bounds writes happen on digging in unloaded chunks
	if (cid == U16_NULL)
		return;

	write_block(bx,by,bz, cid, data);
}

void Chunks::write_block (int x, int y, int z, chunk_id cid, block_id data) {
	auto& vox = chunk_voxels[cid];

	uint32_t subchunk_i = SUBCHUNK_IDX(x,y,z);
	uint32_t& subchunk_val = vox.sparse_data[subchunk_i];

	if (vox.is_subchunk_sparse(subchunk_i)) {
		if (data == (block_id)subchunk_val)
			return; // chunk stays sparse
		densify_subchunk(vox, subchunk_i, subchunk_val); // sparse subchunk, allocate dense subchunk
	}

	auto& subchunk = subchunks[subchunk_val];
	auto blocki = BLOCK_IDX(x,y,z);
	subchunk.voxels[blocki] = data;

	write_block_update_chunk_flags(x,y,z, &chunks[cid]);
}

void Chunks::write_block_update_chunk_flags (int x, int y, int z, Chunk* c) {
	c->flags |= Chunk::REMESH | Chunk::VOXELS_DIRTY;

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

void Chunks::densify_subchunk (ChunkVoxels& vox, uint32_t subchunk_i, uint32_t& subchunk_val) {
	ZoneScoped;
	vox.set_subchunk_dense(subchunk_i);

	block_id bid = (block_id)subchunk_val;

	subchunk_val = subchunks.alloc();
	auto& subchunk = subchunks[subchunk_val];

	for (uint32_t i=0; i<SUBCHUNK_VOXEL_COUNT; ++i)
		subchunk.voxels[i] = (uint32_t)bid;
}

bool Chunks::checked_sparsify_subchunk (ChunkVoxels& vox, uint32_t subchunk_i) {
	auto& subchunk = subchunks[ vox.sparse_data[subchunk_i] ];
	
	block_id bid = subchunk.voxels[0];
	for (int i=1; i<SUBCHUNK_VOXEL_COUNT; ++i) {
		if (subchunk.voxels[i] != bid)
			return false;
	}
	// Subchunk sparse

	DBG_MEMSET(&subchunk, DBG_MEMSET_FREED, sizeof(subchunk));
	subchunks.free(vox.sparse_data[subchunk_i]);

	vox.sparse_data[subchunk_i] = bid;
	vox.set_subchunk_sparse(subchunk_i);

	return true;
}

void Chunks::checked_sparsify_chunk (chunk_id cid) {
	ZoneScoped;
	auto& vox = chunk_voxels[cid];

	for (uint16_t subc_i=0; subc_i<CHUNK_SUBCHUNK_COUNT; ++subc_i) {
		if (!vox.is_subchunk_sparse(subc_i))
			checked_sparsify_subchunk(vox, subc_i);
	}
}

// check if subchunk region in CHUNK_SIZE^3 array is sparse
bool process_subchunk_region (block_id* ptr, SubchunkVoxels& subc) {
#if 1
	// block id at first voxel
	block_id bid = *ptr;
	// 8 bytes packed version to check while row in one line
	uint64_t packed = (uint64_t)bid | ((uint64_t)bid << 16) | ((uint64_t)bid << 32) | ((uint64_t)bid << 48);

	uint64_t* copy = (uint64_t*)subc.voxels;

	int is_sparse = 1;

	for (int z=0; z<SUBCHUNK_SIZE; ++z) {
		for (int y=0; y<SUBCHUNK_SIZE; ++y) {
			for (int x=0; x<SUBCHUNK_SIZE/4; ++x) {
				uint64_t read = *(uint64_t*)ptr;
				
				is_sparse &= (int)(read == packed);

				ptr += 4;
				*copy++ = read; // copy into subchunk
			}
			ptr += CHUNK_SIZE - SUBCHUNK_SIZE; // skip ptr ahead to next row of subchunk
		}
		ptr += CHUNK_SIZE * (CHUNK_SIZE - SUBCHUNK_SIZE); // skip ptr ahead to next layer of subchunk
	}
	return is_sparse;
#else
	block_id bid = *ptr;
	block_id* copy = subc.voxels;

	int is_sparse = 1;

	for (int z=0; z<SUBCHUNK_SIZE; ++z) {
		for (int y=0; y<SUBCHUNK_SIZE; ++y) {
			for (int x=0; x<SUBCHUNK_SIZE; ++x) {
				is_sparse &= (int)(*ptr == bid);

				*copy++ = *ptr++; // copy into subchunk
			}
			ptr += CHUNK_SIZE - SUBCHUNK_SIZE; // skip ptr ahead to next row of subchunk
		}
		ptr += CHUNK_SIZE * (CHUNK_SIZE - SUBCHUNK_SIZE); // skip ptr ahead to next layer of subchunk
	}
	return is_sparse;
#endif
}

void Chunks::sparse_chunk_from_worldgen (chunk_id cid, Chunk& chunk, block_id* raw_voxels) {
	ZoneScoped;

	auto& vox = chunk_voxels[cid];
	
	// init all subchunks to be sparse, because most tend to be sparse
	memset(vox.sparse_bits, -1, sizeof(vox.sparse_bits));

	// allocate one temp subchunk to copy data into while scanning (instead of a scanning loop + copy loop)
	auto temp_subc = subchunks.alloc();

	block_id* ptr = raw_voxels;

	int subc_i = 0;
	for (int sz=0; sz<SUBCHUNK_COUNT; sz++) {
		for (int sy=0; sy<SUBCHUNK_COUNT; sy++) {
			for (int sx=0; sx<SUBCHUNK_COUNT; sx++) {

				bool subchunk_sparse = process_subchunk_region(ptr, subchunks[temp_subc]);

				if (subchunk_sparse) {
					// store sparse block id into sparse storage
					vox.sparse_data[subc_i] = (uint32_t)*ptr;
					// reuse temp subchunk, ie do nothing
				} else {
					// store the dense temp subchunk into our dense chunk, and allocate a new temp subchunk
					// thus avoiding a second copy
					vox.sparse_data[subc_i] = temp_subc;
					temp_subc = subchunks.alloc();

					vox.set_subchunk_dense(subc_i);
				}

				subc_i++;

				ptr += SUBCHUNK_SIZE; // skip to next subchunk begin on x
			}
			ptr += (SUBCHUNK_SIZE -1) * CHUNK_SIZE; // skip to next subchunk begin on y
		}
		ptr += (SUBCHUNK_SIZE -1) * CHUNK_SIZE*CHUNK_SIZE; // skip to next subchunk begin on z
	}

	// free the single unneeded temp subchunk
	subchunks.free(temp_subc);
}

//// Chunk system

chunk_id Chunks::alloc_chunk (int3 pos) {
	ZoneScoped;

	chunk_id cid = chunks.alloc();
	auto& chunk = chunks[cid];

	auto vid = chunk_voxels.alloc();
	assert(vid == cid);

	{ // init
		chunk.flags = Chunk::ALLOCATED;
		chunk.pos = pos;
		//chunk.refcount = 0;
		chunk.init_meshes();
	}

	return cid;
}
void Chunks::free_chunk (chunk_id cid) {
	ZoneScoped;
	auto& chunk = chunks[cid];

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

	free_voxels(cid, chunk);

	memset(&chunk, 0, sizeof(Chunk)); // zero chunk, flags will now indicate that chunk is unallocated
	chunks.free(cid);
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

		auto update_chunk_phase2_generation = [&] (chunk_id cid) {
			auto& chunk = chunks[cid];
			if (chunk.flags & Chunk::LOADED_PHASE2) return;

			worldgen::Neighbours n;

			// check if neighbours are ready yet and build 3x3x3 LUT for faster lookups in worldgen::object_pass() at the same time
			for (int z=-1; z<=1; ++z)
			for (int y=-1; y<=1; ++y)
			for (int x=-1; x<=1; ++x) {
				chunk_id nid = query_chunk(chunk.pos + int3(x,y,z));

				if (nid == U16_NULL)
					return; // neighbours not read yet

				n.neighbours[z+1][y+1][x+1] = nid;
			}

			worldgen::object_pass(*this, cid, n, &game._threads_world_gen);

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

				sparse_chunk_from_worldgen(cid, chunk, &job->noise_pass.voxels[0][0][0]);

				chunk.flags |= Chunk::REMESH | Chunk::VOXELS_DIRTY;

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
						update_chunk_phase2_generation(nid);
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

	upload_slices.clear();
	upload_slices.shrink_to_fit();

	upload_voxels.clear();
	upload_voxels.shrink_to_fit();

	std::vector<std::unique_ptr<RemeshChunkJob>> remesh_jobs;

	{
		ZoneScopedN("remesh iterate chunks");

		for (chunk_id cid = 0; cid<end(); ++cid) {
			auto& chunk = chunks[cid];
			chunk._validate_flags();
			
			if (chunk.flags & Chunk::VOXELS_DIRTY) {
				upload_voxels.push_back(cid);

				checked_sparsify_chunk(cid);
				chunk.flags &= ~Chunk::VOXELS_DIRTY;
			}

			if (chunk.flags & Chunk::REMESH) {
				auto job = std::make_unique<RemeshChunkJob>(*this, cid, game.world_gen, mesh_world_border);
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
				auto& chunk = chunks[res->chunk];

				process_slices(res->opaque_vertices, &chunk.opaque_mesh_vertex_count, &chunk.opaque_mesh_slices);
				process_slices(res->transp_vertices, &chunk.transp_mesh_vertex_count, &chunk.transp_mesh_slices);
				
				chunk.flags &= ~Chunk::REMESH;
			}

			resi += count;
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
	uint64_t block_mem = 0;
	int chunks_loaded = 0;

	for (chunk_id id = 0; id < end(); ++id) {
		if ((chunks[id].flags & Chunk::ALLOCATED) == 0) continue;

		if (chunks[id].flags != 0)
			chunks_loaded++;
	}

	int subc_count = chunks_loaded * CHUNK_SUBCHUNK_COUNT;
	int dense_subc = subchunks.count;
	int sparse_subc = subc_count - dense_subc;

	// memory actually used for voxel data
	uint64_t dense_vox_mem = dense_subc * sizeof(SubchunkVoxels);
	dense_vox_mem += sparse_subc * sizeof(block_id); // include memory used to effectively store sparse voxel data
	
	// memory actually commited in memory
	uint64_t total_vox_mem = chunks.commit_size() + chunk_voxels.commit_size() + subchunks.commit_size();
	uint64_t overhead = total_vox_mem - dense_vox_mem;

	uint64_t sparse_voxels = sparse_subc * (uint64_t)SUBCHUNK_VOXEL_COUNT;

	// NOTE: using 1024 based units even for non-memory numbers because all our counts are power of two based, so results in simpler numbers

	//ImGui::Text("3D Array     : %4d^3 chunks %4d KB",
	//	chunks_arr.size, sizeof(chunk_id) * chunks_arr.size*chunks_arr.size*chunks_arr.size / KB);

	ImGui::Text("Chunks       : %4d chunks  %5s MVox volume %4d KB chunk RAM (%6.2f %% usage)",
		chunks.count, format_thousands(block_volume / MB).c_str(),
		(int)(chunks.commit_size()/KB), chunks.usage() * 100);
	ImGui::Text("Chunk Voxels :                        %6d KB chunk voxel RAM (%6.2f %% usage)",
		(int)(chunk_voxels.commit_size()/KB), chunk_voxels.usage() * 100);
	ImGui::Text("Subchunks    : %4dk / %4dk dense (%6.2f %%)  %6d MB dense subchunk RAM (%6.2f %% usage)",
		dense_subc/KB, subc_count/KB, (float)dense_subc / (float)subc_count * 100,
		(int)(subchunks.commit_size()/MB), subchunks.usage() * 100);
	
	ImGui::Spacing();
	ImGui::Text("Sparseness   : %6d M / %6d M vox sparse (%6.2f %%)  %3d MB total RAM  %4d KB overhead (%6.2f %%)",
		sparse_voxels/MB, block_volume/MB, (float)sparse_voxels / block_volume * 100, total_vox_mem/MB, overhead/KB, (float)overhead / total_vox_mem * 100);

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
	print_block_allocator(chunk_voxels, "chunk_voxels alloc");
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

void Chunks::visualize_chunk (chunk_id cid, Chunk& chunk, bool empty, bool culled) {
	lrgba const* col = &DBG_CHUNK_COL;
	float size = 1.0f;

	if (debug_frustrum_culling) {
		if (empty) return;

		if (culled)
			col = &DBG_CULLED_CHUNK_COL;

	} else if (visualize_chunks) {

		if (chunk.flags & Chunk::LOADED_PHASE2) {
			col = &DBG_CHUNK_COL;
		} else {
			col = &DBG_STAGE1_COL;
		}

	} else {
		return;
	}

	g_debugdraw.wire_cube(((float3)chunk.pos + 0.5f) * CHUNK_SIZE, (float3)CHUNK_SIZE * size * 0.997f, *col);

	if (visualize_chunks && visualize_subchunks) {
		auto& vox = chunk_voxels[cid];

		uint32_t subc_i = 0;
		for (int sz=0; sz<CHUNK_SIZE; sz += SUBCHUNK_SIZE) {
			for (int sy=0; sy<CHUNK_SIZE; sy += SUBCHUNK_SIZE) {
				for (int sx=0; sx<CHUNK_SIZE; sx += SUBCHUNK_SIZE) {
					if (!vox.is_subchunk_sparse(subc_i)) {
						float3 pos = (float3)(chunk.pos * CHUNK_SIZE + int3(sx,sy,sz)) + SUBCHUNK_SIZE/2;
						g_debugdraw.wire_cube(pos, (float3)SUBCHUNK_SIZE * 0.997f, DBG_DENSE_SUBCHUNK_COL);
					}
					subc_i++;
				}
			}
		}
	}
}
