#include "chunks.hpp"
#include "chunk_mesher.hpp"
#include "player.hpp"
#include "world.hpp"
#include "util/timer.hpp"
#include "util/collision.hpp"
#include "world_generator.hpp"
#include "voxel_light.hpp"
#include <algorithm> // std::sort

const int logical_cores = std::thread::hardware_concurrency();

// as many background threads as there are logical cores to allow background threads to use even the main threats time when we are gpu bottlenecked or at an fps cap
const int background_threads  = max(logical_cores, 1);

// main thread + parallelism_threads = logical cores to allow the main thread to join up with the rest of the cpu to work on parallel work that needs to be done immidiately
const int parallelism_threads = max(logical_cores - 1, 1);

static constexpr bool NORMAL_PRIO = false;
static constexpr bool HIGH_PRIO = true;

Threadpool<BackgroundJob > background_threadpool  = { background_threads , NORMAL_PRIO, ">> background threadpool"  };
Threadpool<ParallelismJob> parallelism_threadpool = { parallelism_threads, HIGH_PRIO,   ">> parallelism threadpool" };

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
	OPTICK_EVENT();

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
	OPTICK_EVENT();

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
	OPTICK_EVENT();

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
	OPTICK_EVENT();

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

void Chunk::reupload (MeshingResult const& result) {
	OPTICK_EVENT();

	mesh.opaque_mesh.upload(result.opaque_vertices.ptr, result.opaque_vertices.size);
	mesh.transparent_mesh.upload(result.tranparent_vertices.ptr, result.tranparent_vertices.size);

	face_count = (result.opaque_vertices.size + result.tranparent_vertices.size) / 6;
}

//// Chunks
BackgroundJob BackgroundJob::execute () {
	OPTICK_EVENT();

	auto timer = Timer::start();

	world_gen->generate_chunk(*chunk);

	time = timer.end();

	update_sky_light_chunk(chunk);
	return std::move(*this);
}

ParallelismJob ParallelismJob::execute () {
	OPTICK_EVENT();

	auto timer = Timer::start();

	mesh_chunk(*chunks, graphics->chunk_graphics, graphics->tile_textures, *wg, chunk, &remesh_result);

	time = timer.end();
	return std::move(*this);
}

Chunk* ChunkHashmap::alloc_chunk (chunk_coord coord) {
	OPTICK_EVENT();
	return hashmap.emplace(chunk_coord_hashmap{ coord }, std::make_unique<Chunk>(coord)).first->second.get();
}
Chunk* ChunkHashmap::_lookup_chunk (chunk_coord coord) {
	auto kv = hashmap.find(chunk_coord_hashmap{ coord });
	if (kv == hashmap.end())
		return nullptr;

	Chunk* chunk = kv->second.get();
	_prev_query_chunk = chunk;
	return chunk;
}
Chunk* ChunkHashmap::query_chunk (chunk_coord coord) {
	if (_prev_query_chunk && equal(_prev_query_chunk->coord, coord)) {
		return _prev_query_chunk;
	} else {
		return _lookup_chunk(coord);
	}
}
ChunkHashmap::Iterator ChunkHashmap::erase_chunk (ChunkHashmap::Iterator it) {
	OPTICK_EVENT();
	// reset this pointer to prevent use after free
	_prev_query_chunk = nullptr;
	// delete chunk
	return ChunkHashmap::Iterator( hashmap.erase(it.it) );
}

Chunk* Chunks::query_chunk (chunk_coord coord) {
	return chunks.query_chunk(coord);
}
Block Chunks::query_block (bpos p, Chunk** out_chunk, bpos* out_block_pos_chunk) {
	if (out_chunk)
		*out_chunk = nullptr;

	bpos block_pos_chunk;
	chunk_coord chunk_pos = get_chunk_from_block_pos(p, &block_pos_chunk);
	if (out_block_pos_chunk)
		*out_block_pos_chunk = block_pos_chunk;

	Chunk* chunk = query_chunk(chunk_pos);
	if (!chunk)
		return _NO_CHUNK;

	if (out_chunk)
		*out_chunk = chunk;
	return chunk->get_block(block_pos_chunk);
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

void Chunks::update_chunk_loading (World const& world, WorldGenerator const& world_gen, Player const& player) {
	OPTICK_EVENT();

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
			OPTICK_EVENT("chunks_to_generate iterate all chunks");
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
			OPTICK_EVENT("std::sort(chunks_to_generate)");
			// load chunks nearest to player first
			std::sort(chunks_to_generate.begin(), chunks_to_generate.end(),
				[&] (chunk_coord l, chunk_coord r) { return chunk_dist_to_player(l) < chunk_dist_to_player(r); }
			);
		}

		{
			OPTICK_EVENT("chunks_to_generate push jobs");
			for (auto& cp : chunks_to_generate) {
				Chunk* chunk = pending_chunks.alloc_chunk(cp);
				float dist = chunk_dist_to_player(cp);
			
				BackgroundJob job;
				job.chunk = chunk;
				job.world_gen = &world_gen;
				background_threadpool.jobs.push(job);
			}
		}

		{
			OPTICK_EVENT("chunks_to_generate finish jobs");
			BackgroundJob res;
			while (background_threadpool.results.try_pop(&res)) {
				{ // move chunk into real hashmap
					auto it = pending_chunks.hashmap.find(chunk_coord_hashmap{res.chunk->coord});
					chunks.hashmap.emplace(chunk_coord_hashmap{res.chunk->coord}, std::move(it->second));
					pending_chunks.erase_chunk({ it });
				}
				res.chunk->update_neighbour_blocks(*this);

				chunk_gen_time.push(res.time);
				logf("Chunk (%3d,%3d,%3d) generated in %7.2f ms", res.chunk->coord.x, res.chunk->coord.y, res.chunk->coord.z, res.time * 1024);
			}
		}

	}

}


void Chunks::update_chunks (Graphics const& graphics, WorldGenerator const& wg, Player const& player) {
	OPTICK_EVENT();

	auto chunk_dist_to_player = [&] (chunk_coord pos) {
		bpos chunk_origin = pos * CHUNK_DIM;
		return point_box_nearest_dist((float3)chunk_origin, CHUNK_DIM, player.pos);
	};

	std::vector<Chunk*> chunks_to_remesh;

	{
		OPTICK_EVENT("chunks_to_remesh iterate all chunks");
		for (Chunk& chunk : chunks) {
			if (chunk.needs_remesh)
				chunks_to_remesh.push_back(&chunk);
		}
	}

	{
		OPTICK_EVENT("std::sort(chunks_to_remesh)");
		// update chunks nearest to player first
		std::sort(chunks_to_remesh.begin(), chunks_to_remesh.end(),
			[&] (Chunk* l, Chunk* r) { return chunk_dist_to_player(l->coord) < chunk_dist_to_player(r->coord); }
		);
	}

	{ // remesh all chunks in parallel
		OPTICK_EVENT("remesh all chunks with threadpool");

		int count = min((int)chunks_to_remesh.size(), max_chunks_meshed_per_frame);

		for (int i=0; i<count; ++i) {
			auto* chunk = chunks_to_remesh[i];

			ParallelismJob job = {
				chunk, this, &graphics, &wg
			};
			parallelism_threadpool.jobs.push(std::move(job));
		}

		parallelism_threadpool.contribute_work();

		if (count > 0) {
			auto _total = Timer::start();

			for (int i=0; i<count; ++i) {

				OPTICK_EVENT("for loop");

				ParallelismJob result = parallelism_threadpool.results.pop();

				result.chunk->reupload(result.remesh_result);
				result.chunk->needs_remesh = false;

				{
					OPTICK_EVENT("timing");

					meshing_time.push(result.time);
					logf("Chunk (%3d,%3d) meshing update took %7.3f ms", result.chunk->coord.x, result.chunk->coord.y, result.time * 1000);
				}
			}

			auto total = _total.end();

			logf("Meshing update for frame took %7.3f ms", total * 1000);
		}
	}
}
