#pragma once
#include "kissmath.hpp"
#include "blocks.hpp"
#include "util/move_only_class.hpp"
#include "util/string.hpp"
#include "util/running_average.hpp"
#include "util/threadpool.hpp"
#include "graphics/graphics.hpp" // for ChunkMesh
using namespace kiss;

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
#define CHUNK_DIM_MASK_X	((1 << CHUNK_DIM_SHIFT_X) -1)
#define CHUNK_DIM_MASK_Y	((1 << CHUNK_DIM_SHIFT_Y) -1)
#define CHUNK_DIM_MASK_Z	((1 << CHUNK_DIM_SHIFT_Z) -1)
#define CHUNK_ROW_OFFS		(CHUNK_DIM_X+2)
#define CHUNK_LAYER_OFFS	((CHUNK_DIM_Y+2)*(CHUNK_DIM_X+2))

#define CHUNK_DIM			bpos(CHUNK_DIM_X, CHUNK_DIM_Y, CHUNK_DIM_Z)
#define CHUNK_DIM_2D		bpos2(CHUNK_DIM_X, CHUNK_DIM_Y)

#define CHUNK_BLOCK_COUNT	(CHUNK_DIM_X * CHUNK_DIM_Y * CHUNK_DIM_Z)

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
		bpos_in_chunk->x = pos_world.x & CHUNK_DIM_MASK_X;
		bpos_in_chunk->y = pos_world.y & CHUNK_DIM_MASK_Y;
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

static inline constexpr int _block_count (int lod_levels) {
	int count = 0;
	for (int lod=0; lod<lod_levels; ++lod) {
		count += (CHUNK_DIM_Z >> lod) * (CHUNK_DIM_Y >> lod) * (CHUNK_DIM_X >> lod);
	}
	return count;
};

////////////// Chunk

// Raw malloced array to avoid unique_ptr<T[]> clearing the data with really bad performance
template <typename T>
struct RawArray {
	T* ptr = nullptr;

	RawArray () {}

	RawArray (uint64_t size) {
		ptr = (T*)malloc(size * sizeof(T));
	}
	~RawArray () {
		if (ptr)
			free(ptr);
	}

	RawArray (RawArray const& r) {				std::swap(ptr, r.ptr); }
	RawArray (RawArray&& r) {					std::swap(ptr, r.ptr); }
	RawArray& operator= (RawArray const& r) {	std::swap(ptr, r.ptr); return *this; }
	RawArray& operator= (RawArray&& r) {		std::swap(ptr, r.ptr); return *this; }
};

struct MeshingResult {

	//std::vector<ChunkMesh::Vertex> opaque_vertices;
	//std::vector<ChunkMesh::Vertex> tranparent_vertices;
	RawArray<ChunkMesh::Vertex> opaque_vertices;
	RawArray<ChunkMesh::Vertex> tranparent_vertices;
	unsigned opaque_count;
	unsigned tranparent_count;
};

class Chunk {
	NO_MOVE_COPY_CLASS(Chunk)
public:
	const chunk_coord coord;

	Chunk (chunk_coord coord);
	void init_blocks ();

	static bpos chunk_pos_world (chunk_coord coord) {
		return bpos(coord * CHUNK_DIM_2D, 0);
	}
	bpos chunk_pos_world () const {
		return chunk_pos_world(coord);
	}

	// access blocks raw, only use in World Generator since neighbours are not notified of block changed with these!
	Block* get_block_unchecked (bpos pos);
	// access blocks raw, only use in World Generator since neighbours are not notified of block changed with these!
	void set_block_unchecked (bpos pos, Block b); // only use in World Generator

	// get block
	Block const& get_block (bpos pos) const;
	// get block
	Block const& get_block (bpos_t x, bpos_t y, bpos_t z) const;
	// set block (used in light updater)
	void _set_block_no_light_update (Chunks& chunks, bpos pos, Block b);
	// set block (expensive, -> potentially updates light and copies block data to neighbours if block is at the border of a chunk)
	void set_block (Chunks& chunks, bpos pos, Block b);

	void update_neighbour_blocks(Chunks& chunks);

	// update flags
	bool needs_remesh = true;

	// block update etc.
	bool active;

	// true: invisible to player -> don't draw
	bool culled;

private:
	friend struct Chunk_Mesher;
	// block data
	//  with border that stores a copy of the blocks of our neighbour along the faces (edges and corners are invalid)
	//  border gets automatically kept in sync if only set_block() is used to update blocks
	Block	blocks[CHUNK_DIM_Z+2][CHUNK_DIM_Y+2][CHUNK_DIM_X+2];
public:

	// Gpu mesh data
	ChunkMesh mesh;
	uint64_t face_count;

	void reupload (MeshingResult const& result);
};

////////////// Chunks
struct BackgroundJob { // Chunk gen
	// input
	Chunk* chunk;
	WorldGenerator const* world_gen;
	// output
	float time;

	BackgroundJob execute ();
};

struct ParallelismJob { // CHunk remesh
	// input
	Chunk* chunk;
	Chunks* chunks; // not modfied
	Graphics const* graphics;
	// output
	MeshingResult remesh_result;
	float time;

	ParallelismJob execute ();
};

extern const int background_threads;
extern const int parallelism_threads;

extern Threadpool<BackgroundJob > background_threadpool ;
extern Threadpool<ParallelismJob> parallelism_threadpool;

struct ChunkHashmap {
	typedef std::unordered_map<chunk_coord_hashmap, std::unique_ptr<Chunk>> hashmap_t; 

	// avoid hash map lookup most of the time, since a lot of query_chunk's are going to end up in the same chunk (in query_block of clustered blocks)
	// I did profile this at some point and it was measurably faster than just doing to hashmap lookup all the time
	Chunk* _prev_query_chunk = nullptr;

	hashmap_t hashmap;

	Chunk* alloc_chunk (chunk_coord coord);
	Chunk* _lookup_chunk (chunk_coord coord);

	Chunk* query_chunk (chunk_coord coord);

	struct Iterator {
		hashmap_t::iterator it;

		Iterator (hashmap_t::iterator it): it{it} {} 

		bool operator== (Iterator const& r) { return it == r.it; }
		bool operator!= (Iterator const& r) { return it != r.it; }

		Chunk& operator* () { return *it->second.get(); }

		Iterator operator++ () { ++it; return *this; }
		Iterator operator++ (int) { it++; return *this; }
	};
	struct CIterator {
		hashmap_t::const_iterator it;

		CIterator (hashmap_t::const_iterator it): it{it} {} 

		bool operator== (CIterator const& r) { return it == r.it; }
		bool operator!= (CIterator const& r) { return it != r.it; }

		Chunk const& operator* () { return *it->second.get(); }

		CIterator operator++ () { ++it; return *this; }
		CIterator operator++ (int) { it++; return *this; }
	};
	Iterator begin () {
		return { hashmap.begin() };
	}
	Iterator end () {
		return { hashmap.end() };
	}
	CIterator begin () const {
		return { hashmap.begin() };
	}
	CIterator end () const {
		return { hashmap.end() };
	}

	int count () {
		return (int)hashmap.size();
	}

	Iterator erase_chunk (Iterator it);
};

class Chunks {
public:
	ChunkHashmap pending_chunks;
	ChunkHashmap chunks;

	int count_culled;

	// load chunks in this radius in order of distance to the player 
	float generation_radius =	_use_potatomode ? 150.0f : 200.0f;
	
	// prevent rapid loading and unloading chunks
	// better would be a cache in chunks outside this radius get added (cache size based on desired memory use)
	//  and only the "oldest" chunks should be unloaded
	// This way walking back and forth might not even need to load any chunks at all
	float deletion_hysteresis = CHUNK_DIM_X*1.5f;

	float active_radius =	_use_potatomode ? 150.0f : 200.0f;

	// artifically limit (delay) meshing of chunks to prevent complete freeze of main thread at the cost of some visual artefacts
	int max_chunks_meshed_per_frame = max(std::thread::hardware_concurrency()*2, 4); // max is 2 meshings per cpu core per frame

	RunningAverage<float> chunk_gen_time = { 64 };
	RunningAverage<float> block_light_time = { 64 };
	RunningAverage<float> meshing_time = { 64 };

	void imgui () {
		if (!imgui_push("Chunks")) return;

		ImGui::DragFloat("generation_radius", &generation_radius, 1);
		ImGui::DragFloat("deletion_hysteresis", &deletion_hysteresis, 1);
		ImGui::DragFloat("active_radius", &active_radius, 1);

		ImGui::DragInt("max_chunks_meshed_per_frame", &max_chunks_meshed_per_frame, 0.02f);

		int chunk_count = chunks.count();
		uint64_t block_count = chunk_count * (uint64_t)CHUNK_DIM_X*CHUNK_DIM_Y*CHUNK_DIM_Z;
		uint64_t block_mem = chunk_count * (uint64_t)(CHUNK_DIM_X+2)*(CHUNK_DIM_Y+2)*(CHUNK_DIM_Z+2) * sizeof(Block);

		ImGui::Text("Voxel data: %4d chunks %11s blocks (%5.0f MB  %5.0f KB avg / chunk)", chunk_count, format_thousands(block_count).c_str(), (float)block_mem/1024/1024, (float)block_mem/1024 / chunk_count);

		uint64_t face_count = 0;
		for (Chunk& c : chunks) {
			face_count += c.face_count;
		}
		uint64_t mesh_mem = face_count * 6 * sizeof(ChunkMesh::Vertex);

		ImGui::Text("Mesh data:  %11s faces (%5.0f MB  %5.0f KB avg / chunk)", format_thousands(face_count).c_str(), (float)mesh_mem/1024/1024, (float)mesh_mem/1024 / chunk_count);

		imgui_pop();
	}

	// in chunks
	// lookup a chunk with a chunk coord, returns nullptr chunk not loaded
	Chunk* query_chunk (chunk_coord coord);
	// lookup a block with a world block pos, returns BT_NO_CHUNK for unloaded chunks or BT_OUT_OF_BOUNDS if out of bounds in z
	Block const& query_block (bpos p, Chunk** out_chunk=nullptr, bpos* out_block_pos_chunk=nullptr);

	// unload chunk at coord (invalidates iterators, so dont call this in a loop)
	ChunkHashmap::Iterator unload_chunk (ChunkHashmap::Iterator it);

	void remesh_all ();

	// queue and finialize chunks that should be generated
	void update_chunk_loading (World const& world, WorldGenerator const& world_gen, Player const& player);

	// chunk meshing to prepare for drawing
	void update_chunks (Graphics const& graphics, Player const& player);
};

