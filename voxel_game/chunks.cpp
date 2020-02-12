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

void Chunk::remesh (Chunks& chunks, ChunkGraphics const& graphics) {
	mesh_chunk(chunks, graphics, this);

	needs_remesh = false;
}

void Chunk::update_block_brighness () {
	bpos i; // position in chunk
	for (i.y=0; i.y<CHUNK_DIM_Y; ++i.y) {
		for (i.x=0; i.x<CHUNK_DIM_X; ++i.x) {

			i.z = CHUNK_DIM_Z -1;

			for (; i.z > -1; --i.z) {
				auto* b = get_block(i);

				if (b->type != BT_AIR) break;

				b->dark = false;
			}

			for (; i.z > -1; --i.z) {
				auto* b = get_block(i);

				b->dark = true;
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

Chunk* Chunks::_lookup_chunk (chunk_coord coord) {
	auto kv = chunks.find(chunk_coord_hashmap{ coord });
	if (kv == chunks.end())
		return nullptr;

	Chunk* chunk = &kv->second;
	_prev_query_chunk = chunk;
	return chunk;
}

Chunk* Chunks::query_chunk (chunk_coord coord) {
	if (_prev_query_chunk && equal(_prev_query_chunk->coord, coord)) {
		return _prev_query_chunk;
	} else {
		return _lookup_chunk(coord);
	}
}
Block* Chunks::query_block (bpos p, Chunk** out_chunk) {
	if (out_chunk)
		*out_chunk = nullptr;

	if (p.z < 0 || p.z >= CHUNK_DIM_Z)
		return (Block*)&B_OUT_OF_BOUNDS; // cast const away, check before you write into the returned block for this special case

	bpos block_pos_chunk;
	chunk_coord chunk_pos = get_chunk_from_block_pos(p, &block_pos_chunk);

	Chunk* chunk = query_chunk(chunk_pos);
	if (!chunk)
		return (Block*)&B_NO_CHUNK; // cast const away, check before you write into the returned block for this special case

	if (out_chunk)
		*out_chunk = chunk;
	return chunk->get_block(block_pos_chunk);
}

Chunk* Chunks::load_chunk (World const& world, WorldGenerator& world_gen, chunk_coord chunk_pos) {
	assert(!query_chunk(chunk_pos));
	
	Chunk* chunk = &chunks.emplace(chunk_coord_hashmap{ chunk_pos }, chunk_pos).first->second;
	
	world_gen.generate_chunk(*chunk, world.seed);

	chunk->whole_chunk_changed(*this);
	
	return chunk;
}
Chunks::Iterator Chunks::unload_chunk (Iterator it) {
	remesh_neighbours(it.it->second.coord);
	// reset this pointer to prevent use after free
	_prev_query_chunk = nullptr;
	// delete chunk
	return Iterator( chunks.erase(it.it) );
}

void Chunks::remesh_all () {
	for (auto& kv : chunks) {
		kv.second.needs_remesh = true;
	}
}

void Chunks::update_chunks_load (World const& world, WorldGenerator& world_gen, Player const& player) {

	// check their actual distance to determine if they should be generated or not
	auto chunk_dist_to_player = [&] (chunk_coord pos) {
		bpos2 chunk_origin = pos * CHUNK_DIM_2D;
		return point_square_nearest_dist((float2)chunk_origin, (float2)CHUNK_DIM_2D, (float2)player.pos);
	};
	auto chunk_is_in_load_radius = [&] (chunk_coord pos) {
		return chunk_dist_to_player(pos) <= chunk_generation_radius;
	};
	auto chunk_is_out_of_unload_radius = [&] (chunk_coord pos) {
		return chunk_dist_to_player(pos) > (chunk_generation_radius + chunk_deletion_hysteresis);
	};
	auto chunk_lod = [&] (chunk_coord pos) {
		return clamp(floori(log2f(chunk_dist_to_player(pos) / chunk_generation_radius * 16)), 0,3);
	};

	{ // chunk unloading
		for (auto it=begin(); it!=end();) {
			if (chunk_is_out_of_unload_radius((*it).coord)) {
				it = unload_chunk(it);
			} else {
				++it;
			}
		}
	}

	{ // chunk loading
		chunk_coord start =	(chunk_coord)floor(	((float2)player.pos - chunk_generation_radius) / (float2)CHUNK_DIM_2D );
		chunk_coord end =	(chunk_coord)ceil(	((float2)player.pos + chunk_generation_radius) / (float2)CHUNK_DIM_2D );

		// check all chunk positions within a square of chunk_generation_radius
		std::vector<chunk_coord> chunks_to_generate;

		chunk_coord cp;
		for (cp.x = start.x; cp.x<end.x; ++cp.x) {
			for (cp.y = start.y; cp.y<end.y; ++cp.y) {
				auto* chunk = query_chunk(cp);

				if (chunk) {
					auto prev_lod = chunk->lod;
					chunk->lod = use_lod ? chunk_lod(cp) : 0;
					if (chunk->lod != prev_lod)
						chunk->needs_remesh = true;
				} else {
					if (chunk_is_in_load_radius(cp)) {
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

		int count = 0;
		for (auto& cp : chunks_to_generate) {
			if (count++ >= max_chunks_generated_per_frame)
				break;

			Chunk* new_chunk = load_chunk(world, world_gen, cp);
			new_chunk->lod = use_lod ? chunk_lod(cp) : 0;
		}
	}

}

template <typename FUNC>
Block clac_block_lod (FUNC get_block) {
	int dark_count = 0;
	for (int i=0; i<8; ++i) {
		if (get_block(i)->dark)
			dark_count++; 
	}
	
	int grass_count = 0;
	for (int i=0; i<8; ++i) {
		if (get_block(i)->type == BT_GRASS)
			if (++grass_count == 4)
				return { BT_GRASS, dark_count > 4, 255 }; 
	}

	for (int j=0; j<8; ++j) {
		auto* b = get_block(j);

		int count = 0;
		for (int i=0; i<8; ++i) {
			if (get_block(i)->type == b->type)
				if (++count == 4) // Dominant block
					return { b->type, dark_count > 4, 255 }; 
		}
	}
	return { get_block(0)->type, dark_count > 4, 255 }; 
}

void Chunk::calc_lod (int level) {
	int3 bp;
	for (bp.z=0; bp.z<CHUNK_DIM_Z >> level; ++bp.z) {
		for (bp.y=0; bp.y<CHUNK_DIM_Y >> level; ++bp.y) {
			for (bp.x=0; bp.x<CHUNK_DIM_X >> level; ++bp.x) {
				*get_block(bp, level) = clac_block_lod([=] (int i) {
					return get_block(bp * 2 + int3(i & 1, (i>>1) & 1, (i>>2) & 1), level - 1);
				});
			}
		}
	}
}
void Chunk::calc_lods () {
	calc_lod(1);
	calc_lod(2);
	calc_lod(3);
}

void Chunks::update_chunks_brightness () {
	for (Chunk& chunk : *this) {
		if (chunk.needs_block_brighness_update) {
			auto timer = Timer::start();

			chunk.update_block_brighness();

			auto time = timer.end();
			brightness_time.push(time);
			logf("Chunk (%3d,%3d) brightness update took %7.3f ms", chunk.coord.x,chunk.coord.y, time * 1000);
		}
	}
}

void Chunks::update_chunk_graphics (ChunkGraphics const& graphics) {
	int count = 0;

	for (Chunk& chunk : *this) {
		if (chunk.needs_remesh) {
			if (count++ >= max_chunks_meshed_per_frame)
				break;

			auto timer = Timer::start();

			chunk.calc_lods();
			chunk.remesh(*this, graphics);

			auto time = timer.end();
			meshing_time.push(time);
			logf("Chunk (%3d,%3d) meshing update took %7.3f ms", chunk.coord.x,chunk.coord.y, time * 1000);
		}
	}
}
