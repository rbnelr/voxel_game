#include "chunks.hpp"

Chunk* Chunks::_lookup_chunk (chunk_pos coord) {
	auto kv = chunks.find(s64v2_hashmap{ coord });
	if (kv == chunks.end())
		return nullptr;

	Chunk* chunk = &kv->second;
	_prev_query_chunk = chunk;
	return chunk;
}

Chunk* Chunks::query_chunk (chunk_pos coord) {
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
		return (Block*)&B_OUT_OF_BOUNDS;

	bpos block_pos_chunk;
	chunk_pos chunk_pos = get_chunk_from_block_pos(p, &block_pos_chunk);

	Chunk* chunk = query_chunk(chunk_pos);
	if (!chunk)
		return (Block*)&B_NO_CHUNK;

	if (out_chunk)
		*out_chunk = chunk;
	return chunk->get_block(block_pos_chunk);
}

Chunk* Chunks::create (chunk_pos chunk_pos) {
	auto kv = chunks.emplace(s64v2_hashmap{ chunk_pos }, chunk_pos).first;
	return &kv->second;
}
void Chunks::remove (chunk_pos chunk_pos) {
	chunks.erase(s64v2_hashmap{ chunk_pos });
}

void Chunks::delete_all () {
	_prev_query_chunk = nullptr;
	chunks.clear();
}
void Chunks::remesh_all () {
	for (auto& kv : chunks) {
		kv.second.needs_remesh = true;
	}
}

void Chunk_Mesher::mesh (Chunks& chunks, ChunkGraphics& graphics, Chunk* chunk) {
	//PROFILE_BEGIN(profile_remesh_total);

	alpha_test = graphics.alpha_test;

	opaque_vertices = &chunk->mesh.opaque_faces;
	tranparent_vertices = &chunk->mesh.transparent_faces;

	opaque_vertices->clear();
	tranparent_vertices->clear();

#if AVOID_QUERY_CHUNK_HASH_LOOKUP
	neighbour_chunks[0][0] = chunks.query_chunk(chunk->coord +chunk_pos(-1,-1));
	neighbour_chunks[0][1] = chunks.query_chunk(chunk->coord +chunk_pos( 0,-1));
	neighbour_chunks[0][2] = chunks.query_chunk(chunk->coord +chunk_pos(+1,-1));
	neighbour_chunks[1][0] = chunks.query_chunk(chunk->coord +chunk_pos(-1, 0));
	neighbour_chunks[1][1] = chunk                                          ;
	neighbour_chunks[1][2] = chunks.query_chunk(chunk->coord +chunk_pos(+1, 0));
	neighbour_chunks[2][0] = chunks.query_chunk(chunk->coord +chunk_pos(-1,+1));
	neighbour_chunks[2][1] = chunks.query_chunk(chunk->coord +chunk_pos( 0,+1));
	neighbour_chunks[2][2] = chunks.query_chunk(chunk->coord +chunk_pos(+1,+1));
#endif

	{
		bpos i;
		for (i.z=0; i.z<CHUNK_DIM_Z; ++i.z) {
			for (i.y=0; i.y<CHUNK_DIM_Y; ++i.y) {
				for (i.x=0; i.x<CHUNK_DIM_X; ++i.x) {
					auto* block = chunk->get_block(i);

					if (block->type != BT_AIR) {

						b = block;
						block_pos_x = i.x;
						block_pos_y = i.y;
						block_pos_z = i.z;
						tile = graphics.tile_textures.block_tile_info[b->type];

						if (block_props[block->type].transparency == TM_TRANSPARENT)
							cube_transperant();
						else
							cube_opaque();

					}
				}
			}
		}
	}

	chunk->mesh.opaque_mesh.upload(chunk->mesh.opaque_faces);
	chunk->mesh.transparent_mesh.upload(chunk->mesh.transparent_faces);

	//PROFILE_END_ACCUM(profile_remesh_total);
	//PROFILE_PRINT(profile_remesh_total, "");
}
