#include "common.hpp"
#include "chunks.hpp"
#include "chunk_mesher.hpp"
#include "player.hpp"
#include "world.hpp"
#include "world_generator.hpp"
#include "voxel_light.hpp"

//// Chunk

Chunk::Chunk (chunk_coord coord): coord{coord} {

}

void ChunkData::init_border () {

	for (int i=0; i<COUNT; ++i)
		id[i] = B_NO_CHUNK;

	memset(block_light, 0, sizeof(block_light));
	//memset(sky_light, 0, sizeof(sky_light)); // always inited by update sky light after chunk gen
	memset(hp, 255, sizeof(hp));
}

void Chunk::init_blocks () {
	ZoneScoped;

	blocks = std::make_unique<ChunkData>();
	blocks->init_border();
}

Block Chunk::get_block (bpos pos) const {
	return blocks->get(pos);
}
void Chunk::set_block_unchecked (bpos pos, Block b) {
	blocks->set(pos, b);
}
void Chunk::_set_block_no_light_update (Chunks& chunks, bpos pos_in_chunk, Block b) {
	ZoneScoped;

	Block blk = blocks->get(pos_in_chunk);
	
	blocks->set(pos_in_chunk, b);
	needs_remesh = true;

	bool3 lo = (bpos)pos_in_chunk == 0;
	bool3 hi = (bpos)pos_in_chunk == (bpos)CHUNK_DIM-1;
	if (any(lo || hi)) {
		// block at border

		auto update_neighbour_block_copy = [=, &chunks] (chunk_coord chunk_offset, bpos block) {
			auto chunk = chunks.query_chunk(coord + chunk_offset);
			if (chunk) {
				chunk->blocks->set(block, b);

				chunk->needs_remesh = true;
			}
		};

		if (lo.x) {
			update_neighbour_block_copy(chunk_coord(-1, 0, 0), bpos(CHUNK_DIM, pos_in_chunk.y, pos_in_chunk.z));
		} else if (hi.x) {
			update_neighbour_block_copy(chunk_coord(+1, 0, 0), bpos(       -1, pos_in_chunk.y, pos_in_chunk.z));
		}
		if (lo.y) {
			update_neighbour_block_copy(chunk_coord(0, -1, 0), bpos(pos_in_chunk.x, CHUNK_DIM, pos_in_chunk.z));
		} else if (hi.y) {
			update_neighbour_block_copy(chunk_coord(0, +1, 0), bpos(pos_in_chunk.x,        -1, pos_in_chunk.z));
		}
		if (lo.z) {
			update_neighbour_block_copy(chunk_coord(0, 0, -1), bpos(pos_in_chunk.x, pos_in_chunk.y, CHUNK_DIM));
		} else if (hi.z) {
			update_neighbour_block_copy(chunk_coord(0, 0, +1), bpos(pos_in_chunk.x, pos_in_chunk.y,        -1));
		}
	}
}
void Chunk::set_block (Chunks& chunks, bpos pos_in_chunk, Block b) {
	ZoneScoped;

	Block blk = blocks->get(pos_in_chunk);

	bool only_texture_changed = blk.id == b.id && blk.block_light == b.block_light;
	if (only_texture_changed) {
		blocks->set(pos_in_chunk, b);
		needs_remesh = true;
	} else {
		uint8 old_block_light = blk.block_light;

		uint8 new_block_light = calc_block_light_level(this, pos_in_chunk, b);
		bpos pos = pos_in_chunk + chunk_pos_world();

		b.block_light = new_block_light;

		_set_block_no_light_update(chunks, pos_in_chunk, b);

		update_block_light(chunks, pos, old_block_light, new_block_light);

		update_sky_light_column(this, pos_in_chunk);
	}
}

void set_neighbour_blocks_nx (Chunk const& src, Chunk& dst) {
	for (int z=0; z<CHUNK_DIM; ++z) {
		for (int y=0; y<CHUNK_DIM; ++y) {
			dst.set_block_unchecked(bpos(CHUNK_DIM, y,z), src.get_block(int3(0,y,z)));
		}
	}
}
void set_neighbour_blocks_px (Chunk const& src, Chunk& dst) {
	for (int z=0; z<CHUNK_DIM; ++z) {
		for (int y=0; y<CHUNK_DIM; ++y) {
			dst.set_block_unchecked(bpos(-1, y,z), src.get_block(int3(CHUNK_DIM-1, y,z)));
		}
	}
}
void set_neighbour_blocks_ny (Chunk const& src, Chunk& dst) {
	for (int z=0; z<CHUNK_DIM; ++z) {
		for (int x=0; x<CHUNK_DIM; ++x) {
			dst.set_block_unchecked(bpos(x, CHUNK_DIM, z), src.get_block(int3(x,0,z)));
		}
	}
}
void set_neighbour_blocks_py (Chunk const& src, Chunk& dst) {
	for (int z=0; z<CHUNK_DIM; ++z) {
		for (int x=0; x<CHUNK_DIM; ++x) {
			dst.set_block_unchecked(bpos(x, -1, z), src.get_block(int3(x, CHUNK_DIM-1, z)));
		}
	}
}
void set_neighbour_blocks_nz (Chunk const& src, Chunk& dst) {
	for (int y=0; y<CHUNK_DIM; ++y) {
		for (int x=0; x<CHUNK_DIM; ++x) {
			dst.set_block_unchecked(bpos(x, y, CHUNK_DIM), src.get_block(int3(x,y,0)));
		}
	}
}
void set_neighbour_blocks_pz (Chunk const& src, Chunk& dst) {
	for (int y=0; y<CHUNK_DIM; ++y) {
		for (int x=0; x<CHUNK_DIM; ++x) {
			dst.set_block_unchecked(bpos(x, y, -1), src.get_block(int3(x, y, CHUNK_DIM-1)));
		}
	}
}

void Chunk::update_neighbour_blocks (Chunks& chunks) {
	ZoneScoped;

	Chunk* chunk;
	if ((chunk = chunks.query_chunk(coord + chunk_coord(-1, 0, 0)))) {
		set_neighbour_blocks_nx(*this, *chunk);
		set_neighbour_blocks_px(*chunk, *this);
		chunk->needs_remesh = true;
	}
	if ((chunk = chunks.query_chunk(coord + chunk_coord(+1, 0, 0)))) {
		set_neighbour_blocks_px(*this, *chunk);
		set_neighbour_blocks_nx(*chunk, *this);
		chunk->needs_remesh = true;
	}

	if ((chunk = chunks.query_chunk(coord + chunk_coord( 0,-1, 0)))) {
		set_neighbour_blocks_ny(*this, *chunk);
		set_neighbour_blocks_py(*chunk, *this);
		chunk->needs_remesh = true;
	}
	if ((chunk = chunks.query_chunk(coord + chunk_coord( 0,+1, 0)))) {
		set_neighbour_blocks_py(*this, *chunk);
		set_neighbour_blocks_ny(*chunk, *this);
		chunk->needs_remesh = true;
	}

	if ((chunk = chunks.query_chunk(coord + chunk_coord( 0, 0,-1)))) {
		set_neighbour_blocks_nz(*this, *chunk);
		set_neighbour_blocks_pz(*chunk, *this);
		chunk->needs_remesh = true;
	}
	if ((chunk = chunks.query_chunk(coord + chunk_coord( 0, 0,+1)))) {
		set_neighbour_blocks_pz(*this, *chunk);
		set_neighbour_blocks_nz(*chunk, *this);
		chunk->needs_remesh = true;
	}
}

void Chunk::reupload (MeshingResult& result) {
	ZoneScoped;


	//result.opaque_vertices.upload(mesh.opaque_mesh);
	//result.tranparent_vertices.upload(mesh.transparent_mesh);
	//
	face_count = (result.opaque_vertices.vertex_count + result.tranparent_vertices.vertex_count) / 6;
}

//// Chunks

Chunk* ChunkHashmap::alloc_chunk (chunk_coord coord) {
	ZoneScoped;
	return hashmap.emplace(coord, std::make_unique<Chunk>(coord)).first->second.get();
}
Chunk* ChunkHashmap::_lookup_chunk (chunk_coord coord) {
	auto kv = hashmap.find(coord);
	if (kv == hashmap.end())
		return nullptr;

	Chunk* chunk = kv->second.get();
	_prev_query_chunk = chunk;
	return chunk;
}
Chunk* ChunkHashmap::query_chunk (chunk_coord coord) {
	if (_prev_query_chunk && _prev_query_chunk->coord == coord) {
		return _prev_query_chunk;
	} else {
		return _lookup_chunk(coord);
	}
}
ChunkHashmap::Iterator ChunkHashmap::erase_chunk (ChunkHashmap::Iterator it) {
	ZoneScoped;
	// reset this pointer to prevent use after free
	_prev_query_chunk = nullptr;
	// delete chunk
	return ChunkHashmap::Iterator( hashmap.erase(it.it) );
}

Chunk* Chunks::query_chunk (chunk_coord coord) {
	return chunks.query_chunk(coord);
}
Block Chunks::query_block (bpos pos, Chunk** out_chunk, bpos* out_block_pos_chunk) {
	if (out_chunk)
		*out_chunk = nullptr;

	bpos block_pos_chunk;
	chunk_coord chunk_pos = get_chunk_from_block_pos(pos, &block_pos_chunk);
	if (out_block_pos_chunk)
		*out_block_pos_chunk = block_pos_chunk;

	Chunk* chunk = query_chunk(chunk_pos);
	if (!chunk)
		return _NO_CHUNK;

	if (out_chunk)
		*out_chunk = chunk;
	return chunk->get_block(block_pos_chunk);
}

void Chunks::set_block (bpos pos, Block& b) {
	bpos block_pos_chunk;
	chunk_coord chunk_pos = get_chunk_from_block_pos(pos, &block_pos_chunk);
	
	Chunk* chunk = query_chunk(chunk_pos);
	assert(chunk);
	if (!chunk)
		return;

	chunk->set_block(*this, block_pos_chunk, b);
}

ChunkHashmap::Iterator Chunks::unload_chunk (ChunkHashmap::Iterator it) {
	// TODO: clear neighbour block copies to _NO_CHUNK here?
	return chunks.erase_chunk(it);
}

void Chunks::remesh_all () {
	for (auto& chunk : chunks) {
		chunk.needs_remesh = true;
	}
}

void Chunks::update_chunk_loading (World const& world, WorldGenerator const& wg, Player const& player) {
	ZoneScoped;

	// check their actual distance to determine if they should be generated or not
	auto chunk_dist_to_player = [&] (chunk_coord pos) {
		bpos chunk_origin = pos * CHUNK_DIM;
		return point_box_nearest_dist((float3)chunk_origin, CHUNK_DIM, player.pos);
	};
	auto chunk_lod = [&] (float dist) {
		return clamp(floori(log2f(dist / generation_radius * 16)), 0,3);
	};

	{ // chunk unloading
		for (auto it=chunks.begin(); it!=chunks.end();) {
			float dist = chunk_dist_to_player((*it).coord);

			if (dist > (generation_radius + deletion_hysteresis)) {
				it = unload_chunk(it);
			} else {
				auto& chunk = *it;

				chunk.active = dist <= active_radius;
				
				++it;
			}
		}
	}

	{ // chunk loading
		chunk_coord start =	(chunk_coord)floor(	((float3)player.pos - generation_radius) / (float3)CHUNK_DIM );
		chunk_coord end =	(chunk_coord)ceil(	((float3)player.pos + generation_radius) / (float3)CHUNK_DIM );

		// check all chunk positions within a square of chunk_generation_radius
		std::vector<chunk_coord> chunks_to_generate;

		{
			ZoneScopedN("chunks_to_generate iterate all chunks");
			chunk_coord cp;
			for (cp.z = start.z; cp.z<end.z; ++cp.z) {
				for (cp.y = start.y; cp.y<end.y; ++cp.y) {
					for (cp.x = start.x; cp.x<end.x; ++cp.x) {
						auto* chunk = query_chunk(cp);
						float dist = chunk_dist_to_player(cp);

						if (!chunk) {
							if (dist <= generation_radius && !pending_chunks.query_chunk(cp)) {
								// chunk is within chunk_generation_radius and not yet generated
								chunks_to_generate.push_back(cp);
							}
						}
					}
				}
			}
		}

		{
			ZoneScopedN("std::sort(chunks_to_generate)");
			// load chunks nearest to player first
			std::sort(chunks_to_generate.begin(), chunks_to_generate.end(),
				[&] (chunk_coord l, chunk_coord r) { return chunk_dist_to_player(l) < chunk_dist_to_player(r); }
			);
		}

		{
			ZoneScopedN("chunks_to_generate push jobs");
			
			std::unique_ptr<ThreadingJob> jobs[64];

			size_t count = std::min(chunks_to_generate.size(), ARRLEN(jobs));
			for (int i=0; i<count; ++i) {
				auto cp = chunks_to_generate[i];

				Chunk* chunk = pending_chunks.alloc_chunk(cp);
				float dist = chunk_dist_to_player(cp);
				
				jobs[i] = std::make_unique<WorldgenJob>(chunk, this, &wg);
			}

			background_threadpool.jobs.push_n(jobs, count);
		}

		{
			ZoneScopedN("chunks_to_generate finalize jobs");
			
			std::unique_ptr<ThreadingJob> jobs[64];

			size_t count = background_threadpool.results.pop_n(jobs, ARRLEN(jobs));
			for (size_t i=0; i<count; ++i)
				jobs[i]->finalize();
		}

	}

}

void WorldgenJob::finalize () {
	ZoneScoped;

	{ // move chunk into real hashmap
		auto it = chunks->pending_chunks.hashmap.find(chunk->coord);
		chunks->chunks.hashmap.emplace(chunk->coord, std::move(it->second));
		chunks->pending_chunks.erase_chunk({ it });
	}
	chunk->update_neighbour_blocks(*chunks);

	clog("Chunk (%3d,%3d,%3d) generated", chunk->coord.x, chunk->coord.y, chunk->coord.z);
}

void Chunks::update_chunks (Graphics const& graphics, WorldGenerator const& wg, Player const& player) {
	ZoneScoped;

	auto chunk_dist_to_player = [&] (chunk_coord pos) {
		bpos chunk_origin = pos * CHUNK_DIM;
		return point_box_nearest_dist((float3)chunk_origin, CHUNK_DIM, player.pos);
	};

	std::vector< std::unique_ptr<ThreadingJob> > remesh_jobs;

	{
		ZoneScopedN("chunks_to_remesh iterate all chunks");
		for (Chunk& chunk : chunks) {
			if (chunk.needs_remesh)
				remesh_jobs.push_back( std::make_unique<RemeshChunkJob>(&chunk, this, &graphics, &wg) );
		}
	}

	{ // remesh all chunks in parallel
		ZoneScopedN("Multithreaded remesh");

		parallelism_threadpool.jobs.push_n(remesh_jobs.data(), remesh_jobs.size());

		parallelism_threadpool.contribute_work();

		for (size_t result_count=0; result_count<remesh_jobs.size(); ) {
			ZoneScopedN("dequeue results");

			std::unique_ptr<ThreadingJob> results[64];
			size_t count = parallelism_threadpool.results.pop_n_wait(results, 1, ARRLEN(results));

			for (size_t i=0; i<count; ++i)
				results[i]->finalize();

			result_count += count;
		}
	}
}

void RemeshChunkJob::finalize () {
	chunk->reupload(meshing_result);
	chunk->needs_remesh = false;

	clog("Chunk (%3d,%3d) meshing update", chunk->coord.x, chunk->coord.y);
}
