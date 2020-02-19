#include "chunks.hpp"
#include "chunk_mesher.hpp"
#include "player.hpp"
#include "world.hpp"
#include "util/timer.hpp"
#include "util/collision.hpp"
#include "world_generator.hpp"
#include <algorithm> // std::sort


//// Chunk

Chunk::Chunk (chunk_coord coord): coord{coord} {

}

void Chunk::init_blocks () {
#if 1
	// bottom
	for (int y=0; y<CHUNK_DIM_Y+2; ++y) {
		for (int x=0; x<CHUNK_DIM_X+2; ++x) {
			blocks[0][y][x] = _OUT_OF_BOUNDS;
		}
	}
	// top
	for (int y=0; y<CHUNK_DIM_Y+2; ++y) {
		for (int x=0; x<CHUNK_DIM_X+2; ++x) {
			blocks[CHUNK_DIM_Z+1][y][x] = _OUT_OF_BOUNDS;
		}
	}

	for (int z=1; z<CHUNK_DIM_Z+1; ++z) {
		// backward
		for (int x=0; x<CHUNK_DIM_X+2; ++x) {
			blocks[z][0][x] = _NO_CHUNK;
		}
		// forward
		for (int x=0; x<CHUNK_DIM_X+2; ++x) {
			blocks[z][CHUNK_DIM_Y+1][x] = _NO_CHUNK;
		}
	}

	for (int z=1; z<CHUNK_DIM_Z+1; ++z) {
		for (int y=1; y<CHUNK_DIM_Y+1; ++y) {
			blocks[z][y][0] = _NO_CHUNK; // left
			blocks[z][y][CHUNK_DIM_X+1] = _NO_CHUNK; // right
		}
	}
#else
	// bottom
	for (int y=0; y<CHUNK_DIM_Y+2; ++y) {
		for (int x=0; x<CHUNK_DIM_X+2; ++x) {
			blocks[0][y][x] = _OUT_OF_BOUNDS;
		}
	}
	for (int z=1; z<CHUNK_DIM_Z+1; ++z) {
		for (int y=0; y<CHUNK_DIM_Y+2; ++y) {
			for (int x=0; x<CHUNK_DIM_X+2; ++x) {
				blocks[z][y][x] = _NO_CHUNK;
			}
		}
	}
	// top
	for (int y=0; y<CHUNK_DIM_Y+2; ++y) {
		for (int x=0; x<CHUNK_DIM_X+2; ++x) {
			blocks[CHUNK_DIM_Z+1][y][x] = _OUT_OF_BOUNDS;
		}
	}
#endif
}

Block* Chunk::get_block_unchecked (bpos pos) {
	return &blocks[pos.z + 1][pos.y + 1][pos.x + 1];
}
Block const& Chunk::get_block (bpos pos) const {
	return blocks[pos.z + 1][pos.y + 1][pos.x + 1];
}
Block const& Chunk::get_block (bpos_t x, bpos_t y, bpos_t z) const {
	return blocks[z + 1][y + 1][x + 1];
}
void Chunk::set_block_unchecked (bpos pos, Block b) {
	blocks[pos.z + 1][pos.y + 1][pos.x + 1] = b;
}
void Chunk::set_block (Chunks& chunks, bpos pos, Block b) {
	Block* blk = &blocks[pos.z + 1][pos.y + 1][pos.x + 1];
	
	bool only_texture_changed = blk->id == b.id && blk->light_level == b.light_level;

	*blk = b;
	needs_remesh = true;

	if (!only_texture_changed) {
		needs_block_light_update = true;

		bool2 lo = (bpos2)pos == 0;
		bool2 hi = (bpos2)pos == (bpos2)CHUNK_DIM-1;
		if (any(lo || hi)) {
			// block at border

			auto update_neighbour_block_copy = [=, &chunks] (chunk_coord chunk_offset, bpos block) {
				auto chunk = chunks.query_chunk(coord + chunk_offset);
				if (chunk) {
					chunk->set_block_unchecked(block, b);

					chunk->needs_remesh = true;
					chunk->needs_block_light_update = true;
				}
			};

			if (lo.x) {
				update_neighbour_block_copy(chunk_coord(-1, 0), bpos(CHUNK_DIM_X, pos.y, pos.z));
			} else {
				update_neighbour_block_copy(chunk_coord(+1, 0), bpos(         -1, pos.y, pos.z));
			}
			if (lo.y) {
				update_neighbour_block_copy(chunk_coord(0, -1), bpos(pos.x, CHUNK_DIM_Y, pos.z));
			} else {
				update_neighbour_block_copy(chunk_coord(0, +1), bpos(pos.x,          -1, pos.z));
			}
		}
	}
}

void set_neighbour_blocks_nx (Chunk const& src, Chunk& dst) {
	for (int z=0; z<CHUNK_DIM_Z; ++z) {
		for (int y=0; y<CHUNK_DIM_Y; ++y) {
			dst.set_block_unchecked(bpos(CHUNK_DIM_X, y,z), src.get_block(0,y,z));
		}
	}
}
void set_neighbour_blocks_px (Chunk const& src, Chunk& dst) {
	for (int z=0; z<CHUNK_DIM_Z; ++z) {
		for (int y=0; y<CHUNK_DIM_Y; ++y) {
			dst.set_block_unchecked(bpos(-1, y,z), src.get_block(CHUNK_DIM_X-1, y,z));
		}
	}
}
void set_neighbour_blocks_ny (Chunk const& src, Chunk& dst) {
	for (int z=0; z<CHUNK_DIM_Z; ++z) {
		for (int x=0; x<CHUNK_DIM_X; ++x) {
			dst.set_block_unchecked(bpos(x, CHUNK_DIM_Y, z), src.get_block(x,0,z));
		}
	}
}
void set_neighbour_blocks_py (Chunk const& src, Chunk& dst) {
	for (int z=0; z<CHUNK_DIM_Z; ++z) {
		for (int x=0; x<CHUNK_DIM_X; ++x) {
			dst.set_block_unchecked(bpos(x, -1, z), src.get_block(x, CHUNK_DIM_Y-1, z));
		}
	}
}

void Chunk::update_neighbour_blocks (Chunks& chunks) {
	Chunk* chunk;
	if ((chunk = chunks.query_chunk(coord + chunk_coord(-1, 0)))) {
		set_neighbour_blocks_nx(*this, *chunk);
		set_neighbour_blocks_px(*chunk, *this);
		chunk->needs_remesh = true;
	}
	if ((chunk = chunks.query_chunk(coord + chunk_coord(+1, 0)))) {
		set_neighbour_blocks_px(*this, *chunk);
		set_neighbour_blocks_nx(*chunk, *this);
		chunk->needs_remesh = true;
	}

	if ((chunk = chunks.query_chunk(coord + chunk_coord( 0,-1)))) {
		set_neighbour_blocks_ny(*this, *chunk);
		set_neighbour_blocks_py(*chunk, *this);
		chunk->needs_remesh = true;
	}
	if ((chunk = chunks.query_chunk(coord + chunk_coord( 0,+1)))) {
		set_neighbour_blocks_py(*this, *chunk);
		set_neighbour_blocks_ny(*chunk, *this);
		chunk->needs_remesh = true;
	}
}

void Chunk::update_block_light (Chunks& chunks) {
	needs_block_light_update = false;
	return;

	// get light transported from this block to neighbouring blocks
	// light gets 'emitted' by glowing blocks or transmitted by non-opaque blocks (air, water, glass, leaves etc.)
	auto get_light = [=] (bpos bp) -> int8_t {
		//if (!all(bp >= 0 && bp < CHUNK_DIM))
		//	return 0;
		auto b = get_block(bp);
		return (int8_t)b.light_level;
	};

	bpos p; // position in chunk

	for (p.z=0; p.z<CHUNK_DIM_Z; ++p.z) {
		for (p.y=0; p.y<CHUNK_DIM_Y; ++p.y) {
			for (p.x=0; p.x<CHUNK_DIM_X; ++p.x) {
				auto blk = get_block(p);
				blk.light_level = BLOCK_PROPS[blk.id].glow_level;
			}
		}
	}

	for (int i=0; i<15; ++i) {
		for (p.z=0; p.z<CHUNK_DIM_Z; ++p.z) {
			for (p.y=0; p.y<CHUNK_DIM_Y; ++p.y) {
				for (p.x=0; p.x<CHUNK_DIM_X; ++p.x) {
					if (any(p == 0 || p == CHUNK_DIM-1))
						continue;
					
					auto blk = get_block(p);

					auto A = get_light(p - bpos(1,0,0));
					auto B = get_light(p + bpos(1,0,0));
					auto C = get_light(p - bpos(0,1,0));
					auto D = get_light(p + bpos(0,1,0));
					auto E = get_light(p - bpos(0,0,1));
					auto F = get_light(p + bpos(0,0,1));

					int8_t li = max(
						max(max(A, B), max(C, D)),
						max(E, F)
					);

					li -= 1;
					li = max(li, (int8_t)blk.light_level);

					blk.light_level = (uint8_t)li;
					set_block(chunks, p, blk);
				}
			}
		}
	}

	needs_block_light_update = false;
}

void Chunk::reupload (MeshingResult const& result) {
	mesh.opaque_mesh.upload(result.opaque_vertices);
	mesh.transparent_mesh.upload(result.tranparent_vertices);

	face_count = (result.opaque_vertices.size() + result.tranparent_vertices.size()) / 6;
}

//// Chunks
BackgroundJob BackgroundJob::execute () {
	auto timer = Timer::start();

	world_gen->generate_chunk(*chunk);

	time = timer.end();
	return std::move(*this);
}

ParallelismJob ParallelismJob::execute () {
	auto timer = Timer::start();

	remesh_result = mesh_chunk(*chunks, graphics->chunk_graphics, graphics->tile_textures, chunk);

	time = timer.end();
	return std::move(*this);
}

Chunk* ChunkHashmap::alloc_chunk (chunk_coord coord) {
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
	// reset this pointer to prevent use after free
	_prev_query_chunk = nullptr;
	// delete chunk
	return ChunkHashmap::Iterator( hashmap.erase(it.it) );
}

Chunk* Chunks::query_chunk (chunk_coord coord) {
	return chunks.query_chunk(coord);
}
Block const& Chunks::query_block (bpos p, Chunk** out_chunk, bpos* out_block_pos_chunk) {
	if (out_chunk)
		*out_chunk = nullptr;

	bpos block_pos_chunk;
	chunk_coord chunk_pos = get_chunk_from_block_pos(p, &block_pos_chunk);
	if (out_block_pos_chunk)
		*out_block_pos_chunk = block_pos_chunk;

	if (p.z < 0 || p.z >= CHUNK_DIM_Z)
		return _OUT_OF_BOUNDS;

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

	// check their actual distance to determine if they should be generated or not
	auto chunk_dist_to_player = [&] (chunk_coord pos) {
		bpos2 chunk_origin = pos * CHUNK_DIM_2D;
		return point_square_nearest_dist((float2)chunk_origin, (float2)CHUNK_DIM_2D, (float2)player.pos);
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
		chunk_coord start =	(chunk_coord)floor(	((float2)player.pos - generation_radius) / (float2)CHUNK_DIM_2D );
		chunk_coord end =	(chunk_coord)ceil(	((float2)player.pos + generation_radius) / (float2)CHUNK_DIM_2D );

		// check all chunk positions within a square of chunk_generation_radius
		std::vector<chunk_coord> chunks_to_generate;

		chunk_coord cp;
		for (cp.x = start.x; cp.x<end.x; ++cp.x) {
			for (cp.y = start.y; cp.y<end.y; ++cp.y) {
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

		// load chunks nearest to player first
		std::sort(chunks_to_generate.begin(), chunks_to_generate.end(),
			[&] (chunk_coord l, chunk_coord r) { return chunk_dist_to_player(l) < chunk_dist_to_player(r); }
		);

		for (auto& cp : chunks_to_generate) {
			Chunk* chunk = pending_chunks.alloc_chunk(cp);
			float dist = chunk_dist_to_player(cp);
			
			BackgroundJob job;
			job.chunk = chunk;
			job.world_gen = &world_gen;
			background_threadpool.jobs.push(job);
		}

		BackgroundJob res;
		while (background_threadpool.results.try_pop(&res)) {
			{ // move chunk into real hashmap
				auto it = pending_chunks.hashmap.find(chunk_coord_hashmap{res.chunk->coord});
				chunks.hashmap.emplace(chunk_coord_hashmap{res.chunk->coord}, std::move(it->second));
				pending_chunks.erase_chunk({ it });
			}
			res.chunk->update_neighbour_blocks(*this);

			chunk_gen_time.push(res.time);
			logf("Chunk (%3d,%3d) generated in %7.2f ms  frame %d", res.chunk->coord.x, res.chunk->coord.y, res.time * 1024);
		}

	}

}


void Chunks::update_chunks (Graphics const& graphics, Player const& player) {
	auto chunk_dist_to_player = [&] (chunk_coord pos) {
		bpos2 chunk_origin = pos * CHUNK_DIM_2D;
		return point_square_nearest_dist((float2)chunk_origin, (float2)CHUNK_DIM_2D, (float2)player.pos);
	};

	std::vector<Chunk*> chunks_to_remesh;

	for (Chunk& chunk : chunks) {
		if (chunk.needs_remesh)
			chunks_to_remesh.push_back(&chunk);
	}

	// update chunks nearest to player first
	std::sort(chunks_to_remesh.begin(), chunks_to_remesh.end(),
		[&] (Chunk* l, Chunk* r) { return chunk_dist_to_player(l->coord) < chunk_dist_to_player(r->coord); }
	);

	// update _all chunks_ data required for remesh (remesh accesses neighbours)
	for (int i=0; i<(int)chunks_to_remesh.size(); ++i) {
		auto* chunk = chunks_to_remesh[i];

		if (chunk->needs_block_light_update) {
			auto timer = Timer::start();

			chunk->update_block_light(*this);

			auto time = timer.end();
			light_time.push(time);
			logf("Chunk (%3d,%3d) light update took %7.3f ms", chunk->coord.x, chunk->coord.y, time * 1000);
		}
	}

	{ // remesh all chunks in parallel
		int count = min((int)chunks_to_remesh.size(), max_chunks_meshed_per_frame);

		for (int i=0; i<count; ++i) {
			auto* chunk = chunks_to_remesh[i];

			ParallelismJob job = {
				chunk, this, &graphics
			};
			parallelism_threadpool.jobs.push(job);
		}

		parallelism_threadpool.contribute_work();

		for (int i=0; i<count; ++i) {
			auto result = parallelism_threadpool.results.pop();

			result.chunk->reupload(result.remesh_result);
			result.chunk->needs_remesh = false;

			meshing_time.push(result.time);
			logf("Chunk (%3d,%3d) meshing update took %7.3f ms", result.chunk->coord.x, result.chunk->coord.y, result.time * 1000);
		}
	}
}
