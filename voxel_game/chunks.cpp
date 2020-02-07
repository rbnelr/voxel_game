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

Chunk* Chunks::load_chunk (World const& world, WorldGenerator const& world_gen, chunk_coord chunk_pos) {
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

void Chunks::update_chunks_load (World const& world, WorldGenerator const& world_gen, Player const& player) {

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
				if (chunk_is_in_load_radius(cp) && !query_chunk(cp)) {
					// chunk is within chunk_generation_radius and not yet generated
					chunks_to_generate.push_back(cp);
				}
			}
		}

		// load chunks nearest to player first
		std::sort(chunks_to_generate.begin(), chunks_to_generate.end(),
			[&] (chunk_coord l, chunk_coord r) { return chunk_dist_to_player(l) < chunk_dist_to_player(r); }
		);

		int count = 0;
		for (auto& cp : chunks_to_generate) {
			Chunk* new_chunk = load_chunk(world, world_gen, cp);

			if (++count == max_chunks_generated_per_frame)
				break;
		}
	}

}

extern int frame_counter;

void Chunks::update_chunks_brightness () {
	int count = 0;
	auto timer = Timer::start();

	for (Chunk& chunk : *this) {
		if (chunk.needs_block_brighness_update) {
			chunk.update_block_brighness();
			++count;
		}
	}

	if (count != 0)
		logf("brightness update took %7.3f ms  (frame: %3d chunk count: %d)\n", timer.end() * 1000, frame_counter, count);
}

void Chunks::update_chunk_graphics (ChunkGraphics const& graphics) {
	int count = 0;
	auto timer = Timer::start();

	for (Chunk& chunk : *this) {

		if (chunk.needs_remesh) {
			chunk.remesh(*this, graphics);
			++count;
		}
	}

	if (count != 0)
		logf("mesh update took %7.3f ms  (frame: %3d chunk count: %d)\n", timer.end() * 1000, frame_counter, count);
}
