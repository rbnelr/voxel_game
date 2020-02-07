#pragma once
#include "kissmath.hpp"
#include "blocks.hpp"
#include "graphics/graphics.hpp"
#include "util/move_only_class.hpp"

#include "stdint.h"
#include <unordered_map>

typedef int		bpos_t;
typedef int2	bpos2;
typedef int3	bpos;

typedef int		chunk_pos_t;
typedef int2	chunk_coord;

#define CHUNK_DIM_X			32
#define CHUNK_DIM_Y			32
#define CHUNK_DIM_Z			64
#define CHUNK_DIM_SHIFT_X	5 // for coord >> CHUNK_DIM_SHIFT
#define CHUNK_DIM_SHIFT_Y	5 // for coord >> CHUNK_DIM_SHIFT
#define CHUNK_DIM_SHIFT_Z	6 // for coord >> CHUNK_DIM_SHIFT
#define CHUNK_DIM			bpos(CHUNK_DIM_X, CHUNK_DIM_Y, CHUNK_DIM_Z)
#define CHUNK_DIM_2D		bpos2(CHUNK_DIM_X, CHUNK_DIM_Y)

// get world chunk coord from world block position
inline chunk_coord get_chunk_from_block_pos (bpos2 pos) {

	chunk_coord chunk_pos;
	chunk_pos.x = pos.x >> CHUNK_DIM_SHIFT_X; // arithmetic shift right instead of divide because we want  -10 / 32  to be  -1 instead of 0
	chunk_pos.y = pos.y >> CHUNK_DIM_SHIFT_Y;

	return chunk_pos;
}
// get world chunk coord and block pos in chunk from world block position
inline chunk_coord get_chunk_from_block_pos (bpos pos_world, bpos* bpos_in_chunk=nullptr) {

	chunk_coord chunk_pos = get_chunk_from_block_pos((bpos2)pos_world);

	if (bpos_in_chunk) {
		bpos_in_chunk->x = pos_world.x & ((1 << CHUNK_DIM_SHIFT_X) -1);
		bpos_in_chunk->y = pos_world.y & ((1 << CHUNK_DIM_SHIFT_Y) -1);
		bpos_in_chunk->z = pos_world.z;
	}

	return chunk_pos;
}

// Hashmap key type for chunk_pos
struct chunk_coord_hashmap {
	chunk_coord v;

	bool operator== (chunk_coord_hashmap const& r) const { // for hash map
		return v.x == r.v.x && v.y == r.v.y;
	}
};

inline size_t hash (chunk_coord v) {
	return 53ull * (std::hash<bpos_t>()(v.x) + 53ull) + std::hash<bpos_t>()(v.y);
};

namespace std {
	template<> struct hash<chunk_coord_hashmap> { // for hash map
		size_t operator() (chunk_coord_hashmap const& v) const {
			return ::hash(v.v);
		}
	};
}

class World;
struct WorldGenerator;
class Player;

class Chunk {
	NO_MOVE_COPY_CLASS(Chunk)
public:
	const chunk_coord coord;

	Chunk (chunk_coord coord);

	static bpos chunk_pos_world (chunk_coord coord) {
		return bpos(coord * CHUNK_DIM_2D, 0);
	}
	bpos chunk_pos_world () const {
		return chunk_pos_world(coord);
	}

	// update flags
	bool needs_remesh = true;
	bool needs_block_brighness_update = true;

	// data block
	Block	blocks[CHUNK_DIM_Z][CHUNK_DIM_Y][CHUNK_DIM_X];

	// Gpu mesh data
	ChunkMesh mesh;

	// get block ptr
	Block* get_block (bpos pos) {
		return &blocks[pos.z][pos.y][pos.x];
	}

	void remesh (Chunks& chunks, ChunkGraphics const& graphics);

	void update_block_brighness ();

	void block_only_texture_changed (bpos block_pos_world);
	void block_changed (Chunks& chunks, bpos block_pos_world);

	void whole_chunk_changed (Chunks& chunks);
};

class Chunks {
	// avoid hash map lookup most of the time, since a lot of query_chunk's are going to end up in the same chunk (in query_block of clustered blocks)
	// I did profile this at some point and it was measurably faster than just doing to hashmap lookup all the time
	Chunk* _prev_query_chunk = nullptr;

	// Chunk hashmap
	std::unordered_map<chunk_coord_hashmap, Chunk> chunks;

	Chunk* _lookup_chunk (chunk_coord coord);

public:
	// load chunks in this radius in order of distance to the player 
	float chunk_generation_radius =	_use_potatomode ? 20.0f : 140.0f;
	
	// prevent rapid loading and unloading chunks
	// better would be a cache in chunks outside this radius get added (cache size based on desired memory use)
	//  and only the "oldest" chunks should be unloaded
	// This way walking back and forth might not even need to load any chunks at all
	float chunk_deletion_hysteresis = CHUNK_DIM_X*1.5f;

	// prevent giant lag because chunk gen is in main thread for now
	int max_chunks_generated_per_frame = 1;

	struct Iterator {
		decltype(chunks)::iterator it;

		Iterator (decltype(chunks)::iterator it): it{it} {} 

		bool operator== (Iterator const& r) { return it == r.it; }
		bool operator!= (Iterator const& r) { return it != r.it; }

		Chunk& operator* () { return it->second; }

		Iterator operator++ () { ++it; return *this; }
		Iterator operator++ (int) { it++; return *this; }
	};
	struct CIterator {
		decltype(chunks)::const_iterator it;

		CIterator (decltype(chunks)::const_iterator it): it{it} {} 

		bool operator== (CIterator const& r) { return it == r.it; }
		bool operator!= (CIterator const& r) { return it != r.it; }

		Chunk const& operator* () { return it->second; }

		CIterator operator++ () { ++it; return *this; }
		CIterator operator++ (int) { it++; return *this; }
	};
	Iterator begin () {
		return { chunks.begin() };
	}
	Iterator end () {
		return { chunks.end() };
	}
	CIterator begin () const {
		return { chunks.begin() };
	}
	CIterator end () const {
		return { chunks.end() };
	}

	// lookup a chunk with a chunk coord, returns nullptr chunk not loaded
	Chunk* query_chunk (chunk_coord coord);
	// lookup a block with a world block pos, returns BT_NO_CHUNK for unloaded chunks or BT_OUT_OF_BOUNDS if out of bounds in z
	Block* query_block (bpos p, Chunk** out_chunk=nullptr);

	// load chunk at coord (invalidates iterators, so dont call this in a loop)
	Chunk* load_chunk (World const& world, WorldGenerator const& world_gen, chunk_coord chunk_pos);
	// unload chunk at coord (invalidates iterators, so dont call this in a loop)
	Iterator unload_chunk (Iterator it);

	void remesh_neighbours (chunk_coord coord);

	void remesh_all ();

	void update_chunks_load (World const& world, WorldGenerator const& world_gen, Player const& player);

	void update_chunks_brightness ();

	void update_chunk_graphics (ChunkGraphics const& graphics);
};

