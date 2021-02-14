#include "common.hpp"
#include "chunks.hpp"
#include "chunk_mesher.hpp"
#include "player.hpp"
#include "world.hpp"
#include "world_generator.hpp"
#include "voxel_light.hpp"
#include "chunk_mesher.hpp"

//// Voxel system

void Chunks::init_voxels (Chunk& chunk) {
	chunk.flags |= Chunk::SPARSE_VOXELS;
	chunk.voxel_data = B_NULL;
}
void Chunks::free_voxels (Chunk& c) {
	ZoneScoped;

	if (c.flags & Chunk::SPARSE_VOXELS)
		return; // sparse chunk

	auto& dc = dense_chunks[c.voxel_data];

	for (uint32_t i=0; i<CHUNK_SUBCHUNK_COUNT; ++i) {
		if (!dc.is_subchunk_sparse(i)) {
			auto& subchunk = dense_subchunks[ dc.sparse_data[i] ];
			DBG_MEMSET(&subchunk, DBG_MEMSET_FREED, sizeof(subchunk));
			dense_subchunks.free(dc.sparse_data[i]);
		}
	}

	DBG_MEMSET(&dc, DBG_MEMSET_FREED, sizeof(dc));
	dense_chunks.free(c.voxel_data);
}

block_id Chunks::read_block (int x, int y, int z) {
	int bx, by, bz;
	int cx, cy, cz;
	CHUNK_BLOCK_POS(x,y,z, cx,cy,cz, bx,by,bz);

	auto cid = chunks_arr.checked_get(cx,cy,cz);
	if (cid == U16_NULL)
		return B_NULL;

	return read_block(bx,by,bz, &chunks[cid]);
}

block_id Chunks::read_block (int x, int y, int z, Chunk const* c) {
	assert(c->flags != 0);

	if (c->flags & Chunk::SPARSE_VOXELS)
		return (block_id)c->voxel_data; // sparse chunk

	auto& dc = dense_chunks[c->voxel_data];

	uint32_t subchunk_i = SUBCHUNK_IDX(x,y,z);
	auto subchunk_val = dc.sparse_data[subchunk_i];

	if (dc.is_subchunk_sparse(subchunk_i))
		return CHECK_BLOCK( (block_id)subchunk_val ); // sparse subchunk

	auto& subchunk = dense_subchunks[subchunk_val];

	auto blocki = BLOCK_IDX(x,y,z);
	assert(blocki >= 0 && blocki < SUBCHUNK_VOXEL_COUNT);
	auto block = subchunk.voxels[blocki];

	return CHECK_BLOCK(block); // dense subchunk
}

void Chunks::write_block (int x, int y, int z, block_id data) {
	int bx, by, bz;
	int cx, cy, cz;
	CHUNK_BLOCK_POS(x,y,z, cx,cy,cz, bx,by,bz);

	auto cid = chunks_arr.checked_get(cx,cy,cz);
	//assert(chunk && (chunk->flags & Chunk::LOADED)); // out of bounds writes happen on digging in unloaded chunks
	if (cid == U16_NULL)
		return;

	write_block(bx,by,bz, &chunks[cid], data);
}

void Chunks::write_block (int x, int y, int z, Chunk* c, block_id data) {
	assert(c->flags != 0);

	if (c->flags & Chunk::SPARSE_VOXELS) {
		if (data == (block_id)c->voxel_data)
			return; // chunk stays sparse
		densify_chunk(*c); // sparse chunk, allocate dense chunk
	}

	auto& dc = dense_chunks[c->voxel_data];

	uint32_t subchunk_i = SUBCHUNK_IDX(x,y,z);
	uint32_t& subchunk_val = dc.sparse_data[subchunk_i];

	if (dc.is_subchunk_sparse(subchunk_i)) {
		if (data == (block_id)subchunk_val)
			return; // chunk stays sparse
		densify_subchunk(dc, subchunk_i, subchunk_val); // sparse subchunk, allocate dense subchunk
	}

	auto& subchunk = dense_subchunks[subchunk_val];
	auto blocki = BLOCK_IDX(x,y,z);
	subchunk.voxels[blocki] = data;

	write_block_update_chunk_flags(x,y,z, c);
}

void Chunks::write_block_update_chunk_flags (int x, int y, int z, Chunk* c) {
	c->flags |= Chunk::REMESH|Chunk::VOXELS_DIRTY;

	auto flag_neighbour = [&] (int x, int y, int z) {
		auto nid = chunks_arr.checked_get(x,y,z);
		if (nid != U16_NULL) {
			assert(chunks[nid].flags != 0);
			chunks[nid].flags |= Chunk::REMESH;
		}
	};

	// Set remesh flags for neighbours where needed
	if (x == 0           ) flag_neighbour(x-1, y,   z  );
	if (x == CHUNK_SIZE-1) flag_neighbour(x+1, y,   z  );
	if (y == 0           ) flag_neighbour(x,   y-1, z  );
	if (y == CHUNK_SIZE-1) flag_neighbour(x,   y+1, z  );
	if (z == 0           ) flag_neighbour(x,   y,   z-1);
	if (z == CHUNK_SIZE-1) flag_neighbour(x,   y,   z+1);
}

void Chunks::densify_chunk (Chunk& c) {
	ZoneScoped;

	block_id bid = (block_id)c.voxel_data;

	c.voxel_data = (uint16_t)dense_chunks.alloc();
	c.flags &= ~Chunk::SPARSE_VOXELS;

	auto& dc = dense_chunks[c.voxel_data];

	memset(dc.sparse_bits, -1, sizeof(dc.sparse_bits)); // init to sparse subchunks

	for (uint16_t i=0; i<CHUNK_SUBCHUNK_COUNT; ++i)
		dc.sparse_data[i] = (uint32_t)bid;
}
void Chunks::densify_subchunk (ChunkVoxels& dc, uint32_t subchunk_i, uint32_t& subchunk_val) {
	ZoneScoped;

	dc.set_subchunk_dense(subchunk_i);

	block_id bid = (block_id)subchunk_val;

	subchunk_val = dense_subchunks.alloc();
	auto& subchunk = dense_subchunks[subchunk_val];

	for (uint32_t i=0; i<SUBCHUNK_VOXEL_COUNT; ++i)
		subchunk.voxels[i] = (uint32_t)bid;
}

bool Chunks::checked_sparsify_subchunk (ChunkVoxels& dc, uint32_t subchunk_i) {
	auto& subchunk = dense_subchunks[ dc.sparse_data[subchunk_i] ];
	
	block_id bid = subchunk.voxels[0];
	for (int i=1; i<SUBCHUNK_VOXEL_COUNT; ++i) {
		if (subchunk.voxels[i] != bid)
			return false;
	}
	// Subchunk sparse

	DBG_MEMSET(&subchunk, DBG_MEMSET_FREED, sizeof(subchunk));
	dense_subchunks.free(dc.sparse_data[subchunk_i]);

	dc.sparse_data[subchunk_i] = bid;
	dc.set_subchunk_sparse(subchunk_i);

	return true;
}

void Chunks::checked_sparsify_chunk (Chunk& c) {
	if (c.flags & Chunk::SPARSE_VOXELS)
		return; // chunk already sparse

	ZoneScoped;

	auto& dc = dense_chunks[c.voxel_data];

	bool sparse_chunk = true;
	block_id bid;

	{ // subc_i=0 spacial case to avoid extra branch in loop
		if (!dc.is_subchunk_sparse(0))
			sparse_chunk = checked_sparsify_subchunk(dc, 0);
		if (sparse_chunk)
			bid = (block_id)dc.sparse_data[0];
	}
	for (uint16_t subc_i=1; subc_i<CHUNK_SUBCHUNK_COUNT; ++subc_i) {
		if (!dc.is_subchunk_sparse(subc_i))
			sparse_chunk = checked_sparsify_subchunk(dc, subc_i) && sparse_chunk;
		sparse_chunk = sparse_chunk && (bid == (block_id)dc.sparse_data[subc_i]);
	}

	if (!sparse_chunk)
		return;
	// Chunk completely sparse

	DBG_MEMSET(&dc, DBG_MEMSET_FREED, sizeof(dc));
	dense_chunks.free(c.voxel_data);

	c.flags |= Chunk::SPARSE_VOXELS;
	c.voxel_data = (uint16_t)bid;
}

// check if subchunk region in CHUNK_SIZE^3 array is sparse
bool process_subchunk_region (block_id* ptr, SubchunkVoxels& subc) {
	// block id at first voxel
	block_id bid = *ptr;
	// 8 bytes packed version to check while row in one line
	uint64_t packed = (uint64_t)bid | ((uint64_t)bid << 16) | ((uint64_t)bid << 32) | ((uint64_t)bid << 48);

	block_id* in = ptr;
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
}

bool check_chunk_sparse (ChunkVoxels& dc) {
	uint32_t bid = dc.sparse_data[0];
	uint64_t packed = (uint64_t)bid | ((uint64_t)bid << 32);

	uint64_t* ptr = (uint64_t*)dc.sparse_data;
	for (int i=0; i<CHUNK_SUBCHUNK_COUNT/2; ++i) {
		if (*ptr != packed)
			return false; // chunk not fully sparse
		ptr++;
	}

	return true; // chunk contains only <bid> voxels
}

void Chunks::sparse_chunk_from_worldgen (Chunk& c, WorldgenJob& j) {
	ZoneScoped;
	assert(c.flags & Chunk::SPARSE_VOXELS);

	// Allocate dense chunk data as a buffer (buffer will be freed again if chunks turns out to be sparse)
	c.voxel_data = dense_chunks.alloc();
	c.flags &= ~Chunk::SPARSE_VOXELS;
	auto& dc = dense_chunks[c.voxel_data];
	
	// init all subchunks to be sparse, because most tend to be sparse
	memset(dc.sparse_bits, -1, sizeof(dc.sparse_bits));

	// allocate one temp subchunk to copy data into while scanning (instead of a scanning loop + copy loop)
	auto temp_subc = dense_subchunks.alloc();

	bool chunk_sparse = true;

	block_id* ptr = j.voxel_buffer;

	int subc_i = 0;
	for (int sz=0; sz<SUBCHUNK_COUNT; sz++) {
		for (int sy=0; sy<SUBCHUNK_COUNT; sy++) {
			for (int sx=0; sx<SUBCHUNK_COUNT; sx++) {

				bool subchunk_sparse = process_subchunk_region(ptr, dense_subchunks[temp_subc]);

				if (subchunk_sparse) {
					// store sparse block id into sparse storage
					dc.sparse_data[subc_i] = (uint32_t)*ptr;
					// reuse temp subchunk, ie do nothing
				} else {
					// store the dense temp subchunk into our dense chunk, and allocate a new temp subchunk
					// thus avoiding a second copy
					dc.sparse_data[subc_i] = temp_subc;
					temp_subc = dense_subchunks.alloc();

					dc.set_subchunk_dense(subc_i);

					chunk_sparse = false; // chunk cannot be sparse with non-sparse subchunks
				}

				subc_i++;

				ptr += SUBCHUNK_SIZE; // skip to next subchunk begin on x
			}
			ptr += (SUBCHUNK_SIZE -1) * CHUNK_SIZE; // skip to next subchunk begin on y
		}
		ptr += (SUBCHUNK_SIZE -1) * CHUNK_SIZE*CHUNK_SIZE; // skip to next subchunk begin on z
	}

	// free the single unneeded temp subchunk
	dense_subchunks.free(temp_subc);

	if (chunk_sparse) {
		// no non-sparse subchunks, but subchunks could still be sparse with different block types
		// check if they are all the same type

		if (check_chunk_sparse(dc)) {
			// chunk fully sparse

			block_id bid = dc.sparse_data[0];

			dense_chunks.free(c.voxel_data); // free buffer now that chunk turned out sparse
			
			c.voxel_data = bid;
			c.flags |= Chunk::SPARSE_VOXELS;
		}
	}
}

//// Chunk system

chunk_id Chunks::alloc_chunk (int3 pos) {
	ZoneScoped;

	chunk_id id = chunks.alloc();
	auto& chunk = chunks[id];

	{ // init
		chunk.flags = Chunk::ALLOCATED;
		chunk.pos = pos;
		init_voxels(chunk);
		init_mesh(chunk.opaque_mesh);
		init_mesh(chunk.transparent_mesh);
	}

	return id;
}
void Chunks::free_chunk (chunk_id id) {
	ZoneScoped;
	auto& chunk = chunks[id];

	free_voxels(chunk);

	free_mesh(chunk.opaque_mesh);
	free_mesh(chunk.transparent_mesh);

	memset(&chunk, 0, sizeof(Chunk)); // zero chunk, flags will now indicate that chunk is unallocated
	chunks.free(id);
}

void Chunks::update_chunk_loading (World const& world, Player const& player) {
	ZoneScoped;

	float unload_dist = load_radius + unload_hyster;

	// size chunks_arr to unload radius
	chunks_arr.update(unload_dist / CHUNK_SIZE, player.pos / CHUNK_SIZE);

	{
		ZoneScopedN("chunks_to_generate iterate all chunks");

		float load_dist_sqr = load_radius * load_radius;
		float unload_dist_sqr = unload_dist * unload_dist;

		{
			chunks_to_generate.clear(); // clear all inner vectors
			chunks_to_generate.shrink_to_fit(); // delete all inner vectors to avoid constant memory alloc when loading idle
			chunks_to_generate.resize((int)(load_dist_sqr * BUCKET_FAC) + 1);
		}

		if (visualize_chunks) {
			if (visualize_radius) {
				g_debugdraw.wire_sphere(player.pos, load_radius, DBG_RADIUS_COL);

				auto sz = (float)(chunks_arr.size * CHUNK_SIZE);
				g_debugdraw.wire_cube((float3)chunks_arr.pos * CHUNK_SIZE + sz/2, sz, DBG_CHUNK_ARRAY_COL);
			}

			for (auto chunk_pos : queued_chunks) {
				g_debugdraw.wire_cube(((float3)chunk_pos + 0.5f) * CHUNK_SIZE, (float3)CHUNK_SIZE * 0.6f, DBG_QUEUED_CHUNK_COL);
			}
		}

		for (int z=chunks_arr.pos.z; z < (int)(chunks_arr.pos.z + chunks_arr.size); ++z) {
			for (int y=chunks_arr.pos.y; y < (int)(chunks_arr.pos.y + chunks_arr.size); ++y) {
				for (int x=chunks_arr.pos.x; x < (int)(chunks_arr.pos.x + chunks_arr.size); ++x) {
					chunk_id& cid = chunks_arr.get(x,y,z);
						
					int3 pos = int3(x,y,z);
					float dist_sqr = chunk_dist_sq(pos, player.pos);
						
					if (cid == U16_NULL) {
						// chunk not yet loaded
						bool should_load = dist_sqr <= load_dist_sqr;

						if (should_load && queued_chunks.find(int3(x,y,z)) == queued_chunks.end()) { // chunk not yet queued for worldgen
							int bucketi = (int)(dist_sqr * BUCKET_FAC);
							chunks_to_generate[bucketi].push_back(pos);
						}
					} else {
						// chunk loaded
						chunks[cid]._validate_flags();

						bool should_unload = dist_sqr > unload_dist_sqr;
						if (should_unload) {
							free_chunk(cid);
							cid = U16_NULL;
						}
					}
				}
			}
		}
	}

	{
		ZoneScopedN("chunks_to_generate finalize jobs");

		static constexpr int MAX_REMESH_PER_THREAD_FRAME = 3;
		static const int LOAD_LIMIT = parallelism_threads * MAX_REMESH_PER_THREAD_FRAME;

		std::unique_ptr<WorldgenJob> jobs[64];

		int count = (int)background_threadpool.results.pop_n(jobs, std::min((size_t)LOAD_LIMIT, ARRLEN(jobs)));
		for (int i=0; i<count; ++i) {
			auto job = std::move(jobs[i]);
			queued_chunks.erase(job->chunk_pos);

			auto cid = alloc_chunk(job->chunk_pos);
			auto& chunk = chunks[cid];

			chunk.flags |= Chunk::REMESH;
			chunks_arr.get(chunk.pos.x, chunk.pos.y, chunk.pos.z) = cid;

			sparse_chunk_from_worldgen(chunk, *job);

			for (int i=0; i<6; ++i) {
				int3 npos = job->chunk_pos + NEIGHBOURS[i];
				auto nid = chunks_arr.checked_get(npos.x, npos.y, npos.z);
				if (nid != U16_NULL) {
					assert(chunks[nid].flags != 0);
					chunks[nid].flags |= Chunk::REMESH;
				}
			}
		}
	}

	{
		ZoneScopedN("chunks_to_generate push jobs");

		static constexpr int QUEUE_LIMIT = 64; // 256
		std::unique_ptr<WorldgenJob> jobs[QUEUE_LIMIT];

		// Process bucket-sorted chunks_to_generate in order
		//  and push jobs until threadpool has at max background_queued_count jobs (ignore the remaining chunks, which will get pushed as soon as jobs are completed)
		int bucket=0, j=0; // sort bucket indices

		int max_count = (int)ARRLEN(jobs) - (int)queued_chunks.size();
		int count = 0;
		for (int bucket=0; count < max_count && bucket < (int)chunks_to_generate.size(); ++bucket) {
			for (int i=0; count < max_count && i < (int)chunks_to_generate[bucket].size(); ++i) {
				int3 chunk_pos = chunks_to_generate[bucket][i];
				jobs[count++] = std::make_unique<WorldgenJob>(chunk_pos, &world.world_gen);
				queued_chunks.emplace(chunk_pos);
			}
		}

		background_threadpool.jobs.push_n(jobs, count);

		TracyPlot("background_queued_count", (int64_t)background_queued_count);
	}
}

void Chunks::update_chunk_meshing (World const& world) {
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
				auto job = std::make_unique<RemeshChunkJob>(
					&chunk, this, world.world_gen, mesh_world_border);
				remesh_jobs.emplace_back(std::move(job));
			}
		}
	}

	// remesh all chunks in parallel
	parallelism_threadpool.jobs.push_n(remesh_jobs.data(), remesh_jobs.size());

	{
		ZoneScopedN("remesh process slices");

		// upload remeshed slices and register them in chunk mesh
		auto process_slices = [&] (ChunkMeshData& remeshed, ChunkMesh& mesh) {
			ZoneScopedN("process_slices");

			mesh.vertex_count = remeshed.vertex_count();
			uint32_t remain_vertices = mesh.vertex_count;

			int slice = 0;
			while (remain_vertices > 0) {
				if (mesh.slices[slice] == U16_NULL)
					mesh.slices[slice] = slices_alloc.alloc();

				uint32_t count = std::min(remain_vertices, (uint32_t)CHUNK_SLICE_LENGTH);

				// queue data to be uploaded for sliceid, data stays valid (malloc'd) until it is processed by the renderer
				upload_slices.push_back({ mesh.slices[slice], remeshed.slices[slice] });

				remain_vertices -= count;

				slice++;
			}

			// free potentially remaining slices no longer needed
			for (; slice<MAX_CHUNK_SLICES; ++slice) {
				if (mesh.slices[slice] != U16_NULL)
					slices_alloc.free(mesh.slices[slice]);
				mesh.slices[slice] = U16_NULL;
			}
		};

		for (size_t resi=0; resi < remesh_jobs.size();) {
			std::unique_ptr<RemeshChunkJob> results[64];
			size_t count = parallelism_threadpool.results.pop_n_wait(results, 1, ARRLEN(results));

			for (size_t i=0; i<count; ++i) {
				auto res = std::move(results[i]);

				process_slices(res->mesh.opaque_vertices, res->chunk->opaque_mesh);
				process_slices(res->mesh.tranparent_vertices, res->chunk->transparent_mesh);

				res->chunk->flags &= ~Chunk::REMESH;
			}

			resi += count;
		}
	}
}

void Chunks::imgui (Renderer* renderer) {
	if (!imgui_push("Chunks")) return;

	{
		uint32_t total_pending = 0;
		for (auto& b : chunks_to_generate)
			total_pending += (uint32_t)b.size();

		uint32_t final_chunks = chunks.count + total_pending;
		
		ImGui::Text("chunk loading: %5d / %5d (%3.0f %%)", chunks.count, final_chunks, (float)chunks.count / final_chunks * 100);

		if (total_pending > 0) {
			std::string str = "(";
			for (auto& b : chunks_to_generate) {
				str = prints("%s%3d ", str.c_str(), b.size());
			}

			str += ")";

			ImGui::SameLine();
			ImGui::Text("  %s", str.c_str());
		}
	}

	ImGui::Checkbox("visualize_chunks", &visualize_chunks);
	ImGui::SameLine();
	ImGui::Checkbox("subchunks", &visualize_subchunks);
	ImGui::SameLine();
	ImGui::Checkbox("radius", &visualize_radius);

	if (ImGui::BeginPopupContextWindow("Colors")) {
		imgui_ColorEdit("DBG_CHUNK_COL",			&DBG_CHUNK_COL);
		imgui_ColorEdit("DBG_QUEUED_CHUNK_COL",		&DBG_QUEUED_CHUNK_COL);
		imgui_ColorEdit("DBG_SPARSE_CHUNK_COL",		&DBG_SPARSE_CHUNK_COL);
		imgui_ColorEdit("DBG_CULLED_CHUNK_COL",		&DBG_CULLED_CHUNK_COL);
		imgui_ColorEdit("DBG_DENSE_SUBCHUNK_COL",	&DBG_DENSE_SUBCHUNK_COL);
		imgui_ColorEdit("DBG_RADIUS_COL",			&DBG_RADIUS_COL);
		imgui_ColorEdit("DBG_CHUNK_ARRAY_COL",		&DBG_CHUNK_ARRAY_COL);
		ImGui::EndPopup();
	}

	ImGui::Checkbox("debug_frustrum_culling", &debug_frustrum_culling);

	ImGui::Spacing();
	ImGui::Checkbox("mesh_world_border", &mesh_world_border);

	ImGui::Spacing();
	ImGui::DragFloat("load_radius", &load_radius, 1);
	ImGui::DragFloat("unload_hyster", &unload_hyster, 1);

	uint64_t block_volume = chunks.count * (uint64_t)CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
	uint64_t block_mem = 0;
	int chunks_dense = 0;
	int chunks_loaded = 0;

	int subc_count = 0; // sparse chunks do not count, so essentially the number of either stored sparse subchunk block ids or dense subchunks
	int dense_subc = dense_subchunks.count;

	for (chunk_id id = 0; id < end(); ++id) {
		if ((chunks[id].flags & Chunk::ALLOCATED) == 0) continue;

		if ((chunks[id].flags & Chunk::SPARSE_VOXELS) == 0) {
			chunks_dense++;
			subc_count += CHUNK_SUBCHUNK_COUNT;
			block_mem += sizeof(block_id) * CHUNK_VOXEL_COUNT;
		}

		if (chunks[id].flags != 0)
			chunks_loaded++;
	}

	int sparse_chunks = chunks_loaded - chunks_dense;
	int sparse_subc = subc_count - dense_subc;

	// memory actually used for voxel data
	uint64_t dense_vox_mem = dense_subc * sizeof(SubchunkVoxels);
	dense_vox_mem += (sparse_subc + sparse_chunks) * sizeof(block_id); // include memory used to effectively store sparse voxel data
																	   // memory actually commited in memory
	uint64_t total_vox_mem = chunks.commit_size() + dense_chunks.commit_size() + dense_subchunks.commit_size();
	uint64_t overhead = total_vox_mem - dense_vox_mem;

	uint64_t sparse_voxels = (sparse_chunks * (uint64_t)CHUNK_VOXEL_COUNT) + (sparse_subc * (uint64_t)SUBCHUNK_VOXEL_COUNT);

	// NOTE: using 1024 based units even for non-memory numbers because all our counts are power of two based, so results in simpler numbers
	
	ImGui::Text("Chunks       : %4d chunks  %7s M vox volume %4d KB chunk RAM (%6.2f %% usage)",
		chunks.count, format_thousands(block_volume / MB).c_str(),
		(int)(chunks.commit_size()/KB), chunks.usage() * 100);
	ImGui::Text("Sparse chunks: %5d / %5d dense (%6.2f %%)  %6d KB dense chunk RAM (%6.2f %% usage)",
		chunks_dense, chunks_loaded, (float)chunks_dense / (float)chunks_loaded * 100,
		(int)(dense_chunks.commit_size()/KB), dense_chunks.usage() * 100);
	ImGui::Text("Subchunks    : %4dk / %4dk dense (%6.2f %%)  %6d MB dense subchunk RAM (%6.2f %% usage)",
		dense_subc/KB, subc_count/KB, (float)dense_subc / (float)subc_count * 100,
		(int)(dense_subchunks.commit_size()/MB), dense_subchunks.usage() * 100);
	
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
					chunks[id].opaque_mesh.slices_count(), chunks[id].transparent_mesh.slices_count());
		}
		ImGui::TreePop();
	}

	ImGui::Spacing();

	print_block_allocator(chunks, "chunks alloc");
	print_block_allocator(dense_chunks, "dense_chunks alloc");
	print_block_allocator(dense_subchunks, "dense_subchunks alloc");

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

	imgui_pop();
}

void Chunks::visualize_chunk (Chunk& chunk, bool empty, bool culled) {
	lrgba const* col;
	if (debug_frustrum_culling) {
		if (empty) return;
		col = culled ? &DBG_CULLED_CHUNK_COL : &DBG_CHUNK_COL;
	} else if (visualize_chunks) {
		col = chunk.flags & Chunk::SPARSE_VOXELS ? &DBG_SPARSE_CHUNK_COL : &DBG_CHUNK_COL;

		if (visualize_subchunks && (chunk.flags & Chunk::SPARSE_VOXELS) == 0) {
			auto& dc = dense_chunks[chunk.voxel_data];

			uint32_t subc_i = 0;
			for (int sz=0; sz<CHUNK_SIZE; sz += SUBCHUNK_SIZE) {
				for (int sy=0; sy<CHUNK_SIZE; sy += SUBCHUNK_SIZE) {
					for (int sx=0; sx<CHUNK_SIZE; sx += SUBCHUNK_SIZE) {
						if (!dc.is_subchunk_sparse(subc_i)) {
							float3 pos = (float3)(chunk.pos * CHUNK_SIZE + int3(sx,sy,sz)) + SUBCHUNK_SIZE/2;
							g_debugdraw.wire_cube(pos, (float3)SUBCHUNK_SIZE * 0.997f, DBG_DENSE_SUBCHUNK_COL);
						}
						subc_i++;
					}
				}
			}
		}
	} else {
		return;
	}

	g_debugdraw.wire_cube(((float3)chunk.pos + 0.5f) * CHUNK_SIZE, (float3)CHUNK_SIZE * 0.997f, *col);
}
