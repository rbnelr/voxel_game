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
	chunk.voxel_data = (uint16_t)dense_chunks.alloc();

	auto& dc = dense_chunks[chunk.voxel_data];

	memset(dc.sparse_bits, 0, sizeof(dc.sparse_bits)); // init to dense subchunks

	for (uint16_t i=0; i<CHUNK_SUBCHUNK_COUNT; ++i)
		dc.sparse_data[i] = dense_subchunks.alloc();
}
void Chunks::free_voxels (Chunk& c) {

}

block_id Chunks::read_block (int x, int y, int z) {
	int bx, by, bz;
	int3 chunk_pos;
	CHUNK_BLOCK_POS(x,y,z, chunk_pos, bx,by,bz);

	Chunk* chunk = query_chunk(chunk_pos);
	if (!chunk || (chunk->flags & Chunk::LOADED) == 0)
		return B_NULL;

	return read_block(bx,by,bz, chunk);
}

block_id Chunks::read_block (int x, int y, int z, Chunk const* c) {

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
	int3 chunk_pos;
	CHUNK_BLOCK_POS(x,y,z, chunk_pos, bx,by,bz);

	Chunk* chunk = query_chunk(chunk_pos);
	assert(chunk && (chunk->flags & Chunk::LOADED)); // invalid call
	if (!chunk || (chunk->flags & Chunk::LOADED) == 0)
		return;

	write_block(bx,by,bz, chunk, data);
}

void Chunks::write_block (int x, int y, int z, Chunk* c, block_id data) {
	assert(c->flags & Chunk::LOADED);

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
}

void Chunks::densify_chunk (Chunk& c) {
	block_id bid = (block_id)c.voxel_data;

	c.voxel_data = (uint16_t)dense_chunks.alloc();
	c.flags &= ~Chunk::SPARSE_VOXELS;

	auto& dc = dense_chunks[c.voxel_data];

	memset(dc.sparse_bits, (uint32_t)-1, sizeof(dc.sparse_bits)); // init to sparse subchunks

	for (uint16_t i=0; i<CHUNK_SUBCHUNK_COUNT; ++i)
		dc.sparse_data[i] = (uint32_t)bid;
}
void Chunks::densify_subchunk (ChunkVoxels& dc, uint32_t subchunk_i, uint32_t& subchunk_val) {
	dc.set_subchunk_dense(subchunk_i);

	block_id bid = (block_id)subchunk_val;

	subchunk_val = dense_subchunks.alloc();
	auto& subchunk = dense_subchunks[subchunk_val];

	for (uint32_t i=0; i<SUBCHUNK_VOXEL_COUNT; ++i)
		subchunk.voxels[i] = bid;
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
		memset(chunk.neighbours, -1, sizeof(chunk.neighbours));
		init_mesh(chunk.opaque_mesh);
		init_mesh(chunk.transparent_mesh);
	}

	assert(pos_to_id.find(pos) == pos_to_id.end());
	{
		ZoneScopedN("pos_to_id.emplace");
		pos_to_id.emplace(pos, id);
	}

	for (int i=0; i<6; ++i) {
		ZoneScopedN("alloc_chunk::get neighbour");

		chunks[id].neighbours[i] = query_chunk_id(pos + OFFSETS[i]); // add neighbour to our list
		if (chunks[id].neighbours[i] != U16_NULL) {
			assert(chunks[ chunks[id].neighbours[i] ].neighbours[i^1] == U16_NULL);
			chunks[ chunks[id].neighbours[i] ].neighbours[i^1] = id; // add ourself from neighbours neighbour list
		}
	}

	return id;
}
void Chunks::free_chunk (chunk_id id) {
	ZoneScoped;
	auto& chunk = chunks[id];

	free_voxels(chunk);

	for (int i=0; i<6; ++i) {
		auto n = chunk.neighbours[i];
		if (n != U16_NULL) {
			assert(chunks[n].neighbours[i^1] == id); // i^1 flips direction
			chunks[n].neighbours[i^1] = U16_NULL; // delete ourself from neighbours neighbour list
		}
	}

	free_mesh(chunk.opaque_mesh);
	free_mesh(chunk.transparent_mesh);

	pos_to_id.erase(chunks[id].pos);

	chunks.free(id);
	memset(&chunk, 0, sizeof(Chunk)); // zero chunk, flags will now indicate that chunk is unallocated
}

void Chunks::update_chunk_loading (World const& world, Player const& player) {
	ZoneScoped;

	{ // chunk loading/unloading
		constexpr float BUCKET_FAC = 1.0f / (CHUNK_SIZE*CHUNK_SIZE * 4);

		// check all chunk positions within a square of chunk_generation_radius
		std_vector< std_vector<int3> > chunks_to_generate;
		chunks_to_generate.resize((int)(load_radius*load_radius * BUCKET_FAC) + 1);
		
		{
			ZoneScopedN("chunks_to_generate iterate all chunks");

			{
				// queue first chunk explicitly, so that we can queue the rest via the neighbour refs
				int3 player_pos = floori(player.pos / CHUNK_SIZE);
				if (pos_to_id.find(player_pos) == pos_to_id.end()) {
					chunks_to_generate[0].push_back(player_pos);
				}
			}

			float load_dist_sqr = load_radius * load_radius;
			float unload_dist = load_radius + unload_hyster;
			float unload_dist_sqr = unload_dist * unload_dist;

			for (chunk_id id=0; id<end(); ++id) {
				chunks[id]._validate_flags();
				if ((chunks[id].flags & Chunk::LOADED) == 0) continue;

				float dist_sqr = chunk_dist_sq(chunks[id].pos, player.pos);

				if (dist_sqr > unload_dist_sqr) {
					// unload
					free_chunk(id);
				} else {
					// check neighbour references in chunk for nulls
					// if null check if chunks is supposed to be loaded, if yes then queue
					// note that duplicates can be queued here since the neighbours can be reached via 6 directions
					// instead of doing a potentially expensive hashmap lookup or linear search
					// simply deal with the duplicates later, when we have decided which chunks we even consider to generate (these are more limited in number)
					for (int i=0; i<6; ++i) {
						if (chunks[id].neighbours[i] == U16_NULL) {
							// neighbour chunk not yet loaded
							int3 pos = chunks[id].pos + OFFSETS[i];
							float ndist_sqr = chunk_dist_sq(pos, player.pos);

							if (ndist_sqr <= load_radius*load_radius) {
								int bucketi = (int)(ndist_sqr * BUCKET_FAC);
								chunks_to_generate[bucketi].push_back(pos);
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
				auto& chunk = *job->chunk;

				for (int i=0; i<6; ++i) {
					if (chunk.neighbours[i] != U16_NULL) {
						auto& n = chunks[chunk.neighbours[i]];
						if (n.flags & Chunk::LOADED) n.flags |= Chunk::DIRTY;
					}
				}

				chunk.flags |= Chunk::LOADED|Chunk::DIRTY;
			}

			background_queued_count -= (int)count;
		}

		{
			ZoneScopedN("chunks_to_generate push jobs");

			static constexpr int QUEUE_LIMIT = 256;
			std::unique_ptr<WorldgenJob> jobs[QUEUE_LIMIT];

			// Process bucket-sorted chunks_to_generate in order, remove duplicates
			//  and push jobs until threadpool has at max background_queued_count jobs (ignore the remaining chunks, which will get pushed as soon as jobs are completed)
			int bucket=0, j=0; // sort bucket indices

			int max_count = (int)ARRLEN(jobs) - background_queued_count;
			int count = 0;
			while (count < max_count) {

				while (bucket < chunks_to_generate.size() && j == chunks_to_generate[bucket].size()) {
					bucket++;
					j = 0;
				}
				if (bucket >= chunks_to_generate.size())
					break; // all chunks_to_generate processed

				auto pos = chunks_to_generate[bucket][j++];
				if (pos_to_id.find(pos) != pos_to_id.end())
					continue; // duplicate (chunk with pos already alloc_chunk'd), ignore

				auto id = alloc_chunk(pos);

				auto job = std::make_unique<WorldgenJob>();
				job->chunk = &chunks[id];
				job->chunks = this;
				job->wg = &world.world_gen;
				jobs[count++] = std::move(job);
			}

			background_threadpool.jobs.push_n(jobs, count);
			background_queued_count += (int)count;

			TracyPlot("background_queued_count", (int64_t)background_queued_count);
		}
	}
}

void Chunks::update_chunk_meshing (World const& world) {
	ZoneScoped;

	std::vector<std::unique_ptr<RemeshChunkJob>> remesh_jobs;

	{
		ZoneScopedN("remesh iterate chunks");

		auto should_remesh = Chunk::DIRTY|Chunk::LOADED|Chunk::ALLOCATED;
		for (chunk_id id = 0; id<end(); ++id) {
			auto& chunk = chunks[id];
			chunk._validate_flags();
			if ((chunk.flags & should_remesh) != should_remesh) continue;

			//checked_sparsify(chunk);

			if (chunk.flags & Chunk::SPARSE_VOXELS) {
				chunk.flags &= ~Chunk::DIRTY;
			} else {

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

				res->chunk->flags &= ~Chunk::DIRTY;
			}

			resi += count;
		}
	}
}

void Chunks::imgui (Renderer* renderer) {
	if (!imgui_push("Chunks")) return;

	ImGui::Checkbox("mesh_world_border", &mesh_world_border);
	ImGui::Checkbox("show_chunks", &visualize_chunks);
	ImGui::Checkbox("debug_frustrum_culling", &debug_frustrum_culling);

	ImGui::DragFloat("load_radius", &load_radius, 1);
	ImGui::DragFloat("unload_hyster", &unload_hyster, 1);

	uint64_t block_volume = chunks.count * (uint64_t)CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
	uint64_t block_mem = 0;
	int sparse_chunks = 0;
	int loaded_chunks = 0;

	for (chunk_id id = 0; id < end(); ++id) {
		if ((chunks[id].flags & Chunk::LOADED) == 0) continue;

		//if (chunks[id].voxels.is_sparse()) {
		//	sparse_chunks++;
		//} else {
			block_mem += sizeof(block_id) * CHUNK_VOXEL_COUNT;
		//}
		loaded_chunks++;
	}

	ImGui::Text("Voxels: %4d chunks  %11s volume %6d KB chunk storage",
		chunks.count, format_thousands(block_volume).c_str(), (int)((chunks.commit_end - (char*)chunks.arr) / 1000));
	ImGui::Text("Voxel data: %5.0f MB RAM",
		(float)block_mem/1024/1024);//, (float)sparse_chunks / (float)loaded_chunks * 100);

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

	if (ImGui::TreeNode("chunks alloc")) {
		print_bitset_allocator(chunks.slots, sizeof(Chunk), os_page_size);
		ImGui::TreePop();
	}

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

		//if (!chunk.voxels.is_sparse()) {
		//	assert(chunk.voxels.dense);
		//
		//	size_t subc_i = 0;
		//
		//	for (int sz=0; sz<CHUNK_SIZE; sz += SUBCHUNK_SIZE) {
		//		for (int sy=0; sy<CHUNK_SIZE; sy += SUBCHUNK_SIZE) {
		//			for (int sx=0; sx<CHUNK_SIZE; sx += SUBCHUNK_SIZE) {
		//				auto sparse_bit = chunk.voxels.dense->is_subchunk_sparse(subc_i);
		//
		//				if (!sparse_bit) {
		//					float3 pos = (float3)(chunk.pos * CHUNK_SIZE + int3(sx,sy,sz)) + SUBCHUNK_SIZE/2;
		//					g_debugdraw.wire_cube(pos, (float3)SUBCHUNK_SIZE * 0.997f, DBG_DENSE_SUBCHUNK_COL);
		//				}
		//
		//				subc_i++;
		//			}
		//		}
		//	}
		//}
	} else {
		return;
	}

	g_debugdraw.wire_cube(((float3)chunk.pos + 0.5f) * CHUNK_SIZE, (float3)CHUNK_SIZE * 0.997f, *col);
}
