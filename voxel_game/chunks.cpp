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

void Chunks::delete_all_chunks () {
	_prev_query_chunk = nullptr;
	chunks.clear();
}

Chunk* Chunks::create (chunk_pos chunk_pos) {
	auto kv = chunks.emplace(s64v2_hashmap{ chunk_pos }, chunk_pos).first;
	return &kv->second;
}
void Chunks::remove (chunk_pos chunk_pos) {
	chunks.erase(s64v2_hashmap{ chunk_pos });
}

