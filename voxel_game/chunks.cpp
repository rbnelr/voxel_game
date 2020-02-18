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

void Chunk::remesh (Chunks& chunks, Graphics const& graphics) {
	mesh_chunk(chunks, graphics.chunk_graphics, graphics.tile_textures, this);

	needs_remesh = false;
}

void Chunk::update_block_brighness () {
	bpos i; // position in chunk
	for (i.y=0; i.y<CHUNK_DIM_Y; ++i.y) {
		for (i.x=0; i.x<CHUNK_DIM_X; ++i.x) {

			i.z = CHUNK_DIM_Z -1;

			int light_level = 15;

			for (; i.z > -1; --i.z) {
				auto* b = get_block(i);
				auto& props = BLOCK_PROPS[b->id];

				if (b->id == B_AIR) {
					
				} else if (props.collision != CM_SOLID || props.transparency != TM_OPAQUE) {
					light_level = max(light_level - 4, 0);
				} else {
					light_level = 0;
				}

				b->light_level = light_level;
			}
		}
	}

	needs_block_brighness_update = false;
}

void Chunk::block_only_texture_changed (bpos block_pos_world) { // only breaking animation of block changed -> do not need to update block brightness -> do not need to remesh surrounding chunks (only this chunk needs remesh)
	needs_remesh = true;
}
void Chunk::block_changed (Chunks& chunks, bpos block_pos_world) { // block was placed or broken -> need to update our block brightness -> need to remesh ourselves and surrounding chunks
	needs_remesh = true;
	needs_block_brighness_update = true;

	Chunk* chunk;

	bpos pos = block_pos_world - chunk_pos_world();

	bpos2 chunk_edge = select((bpos2)pos == bpos2(0), bpos2(-1), select((bpos2)pos == CHUNK_DIM_2D -1, bpos2(+1), bpos2(0))); // -1 or +1 for each component if it is on negative or positive chunk edge

																															  // update surrounding chunks if the changed block is touching them to update lighting properly
	if (chunk_edge.x != 0						&& (chunk = chunks.query_chunk(coord +chunk_coord(chunk_edge.x,0))))
		chunk->needs_remesh = true;
	if (chunk_edge.y != 0						&& (chunk = chunks.query_chunk(coord +chunk_coord(0,chunk_edge.y))))
		chunk->needs_remesh = true;
	if (chunk_edge.x != 0 && chunk_edge.y != 0	&& (chunk = chunks.query_chunk(coord +chunk_edge)))
		chunk->needs_remesh = true;
}

void Chunk::whole_chunk_changed (Chunks& chunks) { // whole chunk could have changed -> update our block brightness -> remesh all surrounding chunks
	needs_remesh = true;
	needs_block_brighness_update = true;

	// update surrounding chunks to update lighting properly
	chunks.remesh_neighbours(coord);
}

void Chunks::remesh_neighbours (chunk_coord coord) {
	Chunk* chunk;

	if ((chunk = query_chunk(coord + chunk_coord(-1,-1)))) chunk->needs_remesh = true;
	if ((chunk = query_chunk(coord + chunk_coord( 0,-1)))) chunk->needs_remesh = true;
	if ((chunk = query_chunk(coord + chunk_coord(+1,-1)))) chunk->needs_remesh = true;
	if ((chunk = query_chunk(coord + chunk_coord(+1, 0)))) chunk->needs_remesh = true;
	if ((chunk = query_chunk(coord + chunk_coord(+1,+1)))) chunk->needs_remesh = true;
	if ((chunk = query_chunk(coord + chunk_coord( 0,+1)))) chunk->needs_remesh = true;
	if ((chunk = query_chunk(coord + chunk_coord(-1,+1)))) chunk->needs_remesh = true;
	if ((chunk = query_chunk(coord + chunk_coord(-1, 0)))) chunk->needs_remesh = true;
}

//// Chunks
BackgroundJob BackgroundJob::execute () {
	world_gen->generate_chunk(*chunk, &timer);

	return std::move(*this);
}

ParallelismJob ParallelismJob::execute () {
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
Block* Chunks::query_block (bpos p, Chunk** out_chunk) {
	if (out_chunk)
		*out_chunk = nullptr;

	if (p.z < 0 || p.z >= CHUNK_DIM_Z)
		return (Block*)&_OUT_OF_BOUNDS; // cast const away, check before you write into the returned block for this special case

	bpos block_pos_chunk;
	chunk_coord chunk_pos = get_chunk_from_block_pos(p, &block_pos_chunk);

	Chunk* chunk = query_chunk(chunk_pos);
	if (!chunk)
		return (Block*)&_NO_CHUNK; // cast const away, check before you write into the returned block for this special case

	if (out_chunk)
		*out_chunk = chunk;
	return chunk->get_block(block_pos_chunk);
}

ChunkHashmap::Iterator Chunks::unload_chunk (ChunkHashmap::Iterator it) {
	remesh_neighbours(it.it->second->coord);
	return chunks.erase_chunk(it);
}

void Chunks::remesh_all () {
	for (auto& chunk : chunks) {
		chunk.needs_remesh = true;
	}
}

void Chunks::update_chunks_load (World const& world, WorldGenerator const& world_gen, Player const& player) {

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
			
			chunk_gen_time.push(res.timer);
			logf("Chunk (%3d,%3d) generated in %7.2f ms  frame %d", res.chunk->coord.x, res.chunk->coord.y, res.timer * 1024);

			res.chunk->whole_chunk_changed(*this);
		}

	}

}

template <typename FUNC>
Block clac_block_lod (FUNC get_block) {
	int dark_count = 0;
	//for (int i=0; i<8; ++i) {
	//	if (get_block(i)->dark)
	//		dark_count++; 
	//}
	
	int grass_count = 0;
	for (int i=0; i<8; ++i) {
		if (get_block(i)->id == B_GRASS)
			if (++grass_count == 4)
				return { B_GRASS, dark_count > 4, 255 }; 
	}

	for (int j=0; j<8; ++j) {
		auto* b = get_block(j);

		int count = 0;
		for (int i=0; i<8; ++i) {
			if (get_block(i)->id == b->id)
				if (++count == 4) // Dominant block
					return { b->id, dark_count > 4, 255 }; 
		}
	}
	return { get_block(0)->id, dark_count > 4, 255 }; 
}

void Chunks::update_chunks_brightness () {
	int count = 0;
	
	for (Chunk& chunk : chunks) {
		if (chunk.needs_block_brighness_update) {
			if (count++ >= max_chunks_brightness_per_frame)
				break;

			auto timer = Timer::start();

			chunk.update_block_brighness();

			auto time = timer.end();
			brightness_time.push(time);
			logf("Chunk (%3d,%3d) brightness update took %7.3f ms", chunk.coord.x,chunk.coord.y, time * 1000);
		}
	}
}

void Chunks::update_chunk_graphics (Graphics const& graphics) {
	int count = 0;

	for (Chunk& chunk : chunks) {
		if (chunk.needs_remesh) {
			if (count++ >= max_chunks_meshed_per_frame)
				break;

			auto timer = Timer::start();

			chunk.remesh(*this, graphics);

			auto time = timer.end();
			meshing_time.push(time);
			logf("Chunk (%3d,%3d) meshing update took %7.3f ms", chunk.coord.x,chunk.coord.y, time * 1000);
		}
	}
}
