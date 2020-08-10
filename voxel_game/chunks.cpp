#include "stdafx.hpp"
#include "chunks.hpp"
#include "chunk_mesher.hpp"
#include "player.hpp"
#include "world.hpp"
#include "util/collision.hpp"
#include "world_generator.hpp"
#include "voxel_light.hpp"

const int logical_cores = std::thread::hardware_concurrency();

// NOPE: a̶s̶ ̶m̶a̶n̶y̶ ̶b̶a̶c̶k̶g̶r̶o̶u̶n̶d̶ ̶t̶h̶r̶e̶a̶d̶s̶ ̶a̶s̶ ̶t̶h̶e̶r̶e̶ ̶a̶r̶e̶ ̶l̶o̶g̶i̶c̶a̶l̶ ̶c̶o̶r̶e̶s̶ ̶t̶o̶ ̶a̶l̶l̶o̶w̶ ̶b̶a̶c̶k̶g̶r̶o̶u̶n̶d̶ ̶t̶h̶r̶e̶a̶d̶s̶ ̶t̶o̶ ̶u̶s̶e̶ ̶e̶v̶e̶n̶ ̶t̶h̶e̶ ̶m̶a̶i̶n̶ ̶t̶h̶r̶e̶a̶d̶'̶s̶ ̶t̶i̶m̶e̶ ̶w̶h̶e̶n̶ ̶w̶e̶ ̶a̶r̶e̶ ̶g̶p̶u̶ ̶b̶o̶t̶t̶l̶e̶n̶e̶c̶k̶e̶d̶ ̶o̶r̶ ̶a̶t̶ ̶a̶n̶ ̶f̶p̶s̶ ̶c̶a̶p̶
// A threadpool for async background work tasks
// keep a reasonable amount of cores free from background work because lower thread priority is not enough to ensure that these threads get preempted when high prio threads need to run
// this is because of limited frequency of the scheduling interrupt 'timer resolution' on windows at least
// the main thread should be able to run after waiting and there need to be enough additional cores free for the os tasks, else mainthread often gets preemted for ver long (1ms - 10+ ms) causing serious lag
// cores:  1 -> threads: 1
// cores:  2 -> threads: 1
// cores:  4 -> threads: 2
// cores:  6 -> threads: 4
// cores:  8 -> threads: 5
// cores: 12 -> threads: 9
// cores: 16 -> threads: 12
// cores: 24 -> threads: 18
// cores: 32 -> threads: 25
const int background_threads  = clamp(roundi((float)logical_cores * 0.80f) - 1, 1, logical_cores);

// main thread + parallelism_threads = logical cores -1 to allow the main thread to join up with the rest of the cpu to work on parallel work that needs to be done immidiately
// leave one thread for system and background apps
const int parallelism_threads = clamp(logical_cores - 1, 1, logical_cores);

static constexpr bool NORMAL_PRIO = false;
static constexpr bool HIGH_PRIO = true;

Threadpool<BackgroundJob > background_threadpool  = { background_threads , NORMAL_PRIO, ">> background threadpool"  };
Threadpool<ParallelismJob> parallelism_threadpool = { parallelism_threads - 1, HIGH_PRIO,   ">> parallelism threadpool" }; // parallelism_threads - 1 to let main thread contribute work too

FreelistAllocator<MeshingBlock> meshing_allocator;

void shutdown_threadpools () {
	background_threadpool.shutdown();
	parallelism_threadpool.shutdown();
}

//// Chunk

Chunk::Chunk (chunk_coord coord): coord{coord} {

}

void ChunkData::init_border () {

	//for (int i=0; i<COUNT; ++i)
	//	id[i] = B_NO_CHUNK;

	memset(block_light, 0, sizeof(block_light));
	//memset(sky_light, 0, sizeof(sky_light)); // always inited by update sky light after chunk gen
	memset(hp, 255, sizeof(hp));
}

void Chunk::init_blocks () {
	blocks = std::make_unique<ChunkData>();
	blocks->init_border();
}

Block Chunk::get_block (bpos pos) const {
	return blocks->get(pos);
}
void Chunk::set_block_unchecked (Chunks& chunks, bpos pos, Block b) {
	blocks->set(pos, b);

	chunks.svo.update_block(*this, pos, b.id); 
}
void Chunk::_set_block_no_light_update (Chunks& chunks, bpos pos_in_chunk, Block b) {
	Block blk = blocks->get(pos_in_chunk);
	
	blocks->set(pos_in_chunk, b);
	needs_remesh = true;

	chunks.svo.update_block(*this, pos_in_chunk, b.id); 
}
void Chunk::set_block (Chunks& chunks, bpos pos_in_chunk, Block b) {
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

void Chunk::reupload (MeshingResult& result) {
	result.opaque_vertices.upload(mesh.opaque_mesh);
	result.tranparent_vertices.upload(mesh.transparent_mesh);

	face_count = (result.opaque_vertices.vertex_count + result.tranparent_vertices.vertex_count) / 6;
}

//// Chunks
BackgroundJob BackgroundJob::execute () {
	auto timer = Timer::start();

	world_gen->generate_chunk(*chunk, *svo);

	time = timer.end();

	update_sky_light_chunk(chunk);
	return std::move(*this);
}

ParallelismJob ParallelismJob::execute () {
	auto timer = Timer::start();

	mesh_chunk(*chunks, graphics->chunk_graphics, graphics->tile_textures, *wg, chunk, &meshing_result);

	time = timer.end();
	return std::move(*this);
}

Chunk* ChunkHashmap::alloc_chunk (chunk_coord coord) {
	std::unique_ptr<Chunk> ptr;
	Chunk* raw_ptr;

	{
		ptr = std::make_unique<Chunk>(coord);
		raw_ptr = ptr.get();
	}

	{
		hashmap.emplace(chunk_coord_hashmap{ coord }, std::move(ptr));
	}

	return raw_ptr;
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
	svo.remove_chunk(*it);

	// TODO: clear neighbour block copies to _NO_CHUNK here?
	return chunks.erase_chunk(it);
}

void Chunks::remesh_all () {
	for (auto& chunk : chunks) {
		chunk.needs_remesh = true;
	}
}

void Chunks::update_chunk_loading (World& world, WorldGenerator const& world_gen, Player const& player) {
	ZoneScopedN("update_chunk_loading");

	svo.pre_update(player);

	// check their actual distance to determine if they should be generated or not
	auto chunk_dist_to_player = [&] (chunk_coord pos) {
		bpos chunk_origin = pos * CHUNK_DIM;
		return point_box_nearest_dist((float3)chunk_origin, CHUNK_DIM, player.pos);
	};
	auto chunk_lod = [&] (float dist) {
		return clamp(floori(log2f(dist / generation_radius * 16)), 0,3);
	};

	{ // chunk unloading
		ZoneScopedN("chunk unloading");

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
		ZoneScopedN("chunk loading");

		chunk_coord start =	(chunk_coord)floor(	((float3)player.pos - generation_radius) / (float3)CHUNK_DIM );
		chunk_coord end =	(chunk_coord)ceil(	((float3)player.pos + generation_radius) / (float3)CHUNK_DIM );

		// check all chunk positions within a square of chunk_generation_radius
		std::vector<chunk_coord> chunks_to_generate;

		{
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
			ZoneScopedN("sort chunks_to_generate");

			// load chunks nearest to player first
			std::sort(chunks_to_generate.begin(), chunks_to_generate.end(),
				[&] (chunk_coord l, chunk_coord r) { return chunk_dist_to_player(l) < chunk_dist_to_player(r); }
			);
		}

		int count = min((int)chunks_to_generate.size(), max_chunk_gens_processed_per_frame);

		{
			for (int i=0; i<count; ++i) {
				ZoneScopedN("queue chunk_to_generate");

				auto cp = chunks_to_generate[i];

				Chunk* chunk = pending_chunks.alloc_chunk(cp);
				float dist = chunk_dist_to_player(cp);
			
				BackgroundJob job;
				job.chunk = chunk;
				job.world_gen = &world_gen;
				job.svo = &world.chunks.svo;

				{
					background_threadpool.jobs.push(job);
				}
			}
		}

		{
			int count = 0;

			BackgroundJob res;
			while (count++ < max_chunk_gens_processed_per_frame && background_threadpool.results.try_pop(&res)) {
				ZoneScopedN("finialize chunk_to_generate");

				{ // move chunk into real hashmap
					auto it = pending_chunks.hashmap.find(chunk_coord_hashmap{res.chunk->coord});
					chunks.hashmap.emplace(chunk_coord_hashmap{res.chunk->coord}, std::move(it->second));
					pending_chunks.erase_chunk({ it });

					svo.add_chunk(*res.chunk);
				}

				chunk_gen_time.push(res.time);
				clog("Chunk (%3d,%3d,%3d) generated in %7.2f ms", res.chunk->coord.x, res.chunk->coord.y, res.chunk->coord.z, res.time * 1024);
			}
		}

	}

	svo.post_update();
}

void Chunks::update_chunks (Graphics const& graphics, WorldGenerator const& wg, Player const& player) {
	auto chunk_dist_to_player = [&] (chunk_coord pos) {
		bpos chunk_origin = pos * CHUNK_DIM;
		return point_box_nearest_dist((float3)chunk_origin, CHUNK_DIM, player.pos);
	};

	std::vector<Chunk*> chunks_to_remesh;

	{
		for (Chunk& chunk : chunks) {
			if (chunk.needs_remesh)
				chunks_to_remesh.push_back(&chunk);
		}
	}

	{
		ZoneScopedN("sort chunks_to_generate");
		
		// update chunks nearest to player first
		std::sort(chunks_to_remesh.begin(), chunks_to_remesh.end(),
			[&] (Chunk* l, Chunk* r) { return chunk_dist_to_player(l->coord) < chunk_dist_to_player(r->coord); }
		);
	}

	{ // remesh all chunks in parallel
		int count = min((int)chunks_to_remesh.size(), max_chunks_meshed_per_frame);

		for (int i=0; i<count; ++i) {
			ZoneScopedN("queue chunk_to_remesh");

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
				ZoneScopedN("finialize meshing_result");

				ParallelismJob result = parallelism_threadpool.results.pop();

				result.chunk->reupload(result.meshing_result);
				result.chunk->needs_remesh = false;

				meshing_time.push(result.time);
				clog("Chunk (%3d,%3d) meshing update took %7.3f ms", result.chunk->coord.x, result.chunk->coord.y, result.time * 1000);
			}

			auto total = _total.end();

			clog(">>> %d", meshing_allocator.count());

			clog("Meshing update for frame took %7.3f ms", total * 1000);
		}
	}
}
