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
#define CHUNK_DIM			bpos(CHUNK_DIM_X, CHUNK_DIM_Y, CHUNK_DIM_Z)
#define CHUNK_DIM_2D		bpos2(CHUNK_DIM_X, CHUNK_DIM_Y)

// get world chunk coord from world block position
inline chunk_coord get_chunk_from_block_pos (bpos2 pos, int lod=0) {

	chunk_coord chunk_pos;
	chunk_pos.x = pos.x >> (CHUNK_DIM_SHIFT_X-lod); // arithmetic shift right instead of divide because we want  -10 / 32  to be  -1 instead of 0
	chunk_pos.y = pos.y >> (CHUNK_DIM_SHIFT_Y-lod);

	return chunk_pos;
}
// get world chunk coord and block pos in chunk from world block position
inline chunk_coord get_chunk_from_block_pos (bpos pos_world, bpos* bpos_in_chunk=nullptr, int lod=0) {

	chunk_coord chunk_pos = get_chunk_from_block_pos((bpos2)pos_world, lod);

	if (bpos_in_chunk) {
		bpos_in_chunk->x = pos_world.x & ((1 << (CHUNK_DIM_SHIFT_X-lod)) -1);
		bpos_in_chunk->y = pos_world.y & ((1 << (CHUNK_DIM_SHIFT_Y-lod)) -1);
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

	bool frustrum_culled;

	// data block

	Block	blocks[_block_count(4)];
	int lod_offsets[4] = {
		_block_count(0),
		_block_count(1),
		_block_count(2),
		_block_count(3),
	};

	// get block ptr
	Block* get_block (bpos pos, int lod=0) {
		Block* level = blocks + lod_offsets[lod];
		return level + pos.z * (CHUNK_DIM_Y >> lod) * (CHUNK_DIM_X >> lod) + pos.y * (CHUNK_DIM_X >> lod) + pos.x;
	}
	Block* get_block_flat (int index) {
		return &blocks[index];
	}

	int lod = -1;

	// Gpu mesh data
	ChunkMesh mesh;

	void calc_lod (int level);
	void calc_lods ();

	void remesh (Chunks& chunks, ChunkGraphics const& graphics);

	void update_block_brighness ();

	void block_only_texture_changed (bpos block_pos_world);
	void block_changed (Chunks& chunks, bpos block_pos_world);

	void whole_chunk_changed (Chunks& chunks);
};

struct GenerateChunkJob {
	// input
	chunk_coord coord;
	WorldGenerator const* world_gen;
	// output
	Chunk* chunk;
	float chunk_gen_time;

	GenerateChunkJob execute ();
};

inline Threadpool<GenerateChunkJob> threadpool = { max(std::thread::hardware_concurrency() - 1, 1), "threadpool" };

class Chunks {
	// avoid hash map lookup most of the time, since a lot of query_chunk's are going to end up in the same chunk (in query_block of clustered blocks)
	// I did profile this at some point and it was measurably faster than just doing to hashmap lookup all the time
	Chunk* _prev_query_chunk = nullptr;

	// Chunk hashmap
	std::unordered_map<chunk_coord_hashmap, std::unique_ptr<Chunk>> chunks;

	Chunk* _lookup_chunk (chunk_coord coord);

public:

	int count_frustrum_culled;

	// load chunks in this radius in order of distance to the player 
	float chunk_generation_radius =	_use_potatomode ? 150.0f : 200.0f;
	
	// prevent rapid loading and unloading chunks
	// better would be a cache in chunks outside this radius get added (cache size based on desired memory use)
	//  and only the "oldest" chunks should be unloaded
	// This way walking back and forth might not even need to load any chunks at all
	float chunk_deletion_hysteresis = CHUNK_DIM_X*1.5f;

	// prevent giant lag because chunk gen is in main thread for now
	int max_chunks_generated_per_frame = 1;

	// prevent giant lag because chunk gen is in main thread for now
	int max_chunks_meshed_per_frame = 20;

	bool use_lod = false;

	RunningAverage<float> chunk_gen_time = { 64 };
	RunningAverage<float> brightness_time = { 64 };
	RunningAverage<float> meshing_time = { 64 };

	void imgui () {
		if (!imgui_push("Chunks")) return;

		ImGui::DragFloat("chunk_generation_radius", &chunk_generation_radius, 1);
		ImGui::DragFloat("chunk_deletion_hysteresis", &chunk_deletion_hysteresis, 1);
		ImGui::DragInt("max_chunks_generated_per_frame", &max_chunks_generated_per_frame, 0.02f);
		ImGui::DragInt("max_chunks_meshed_per_frame", &max_chunks_meshed_per_frame, 0.02f);

		ImGui::Checkbox("use_lod", &use_lod);

		int chunk_count = (int)chunks.size();
		uint64_t block_count = chunk_count * (uint64_t)CHUNK_DIM_X*CHUNK_DIM_Y*CHUNK_DIM_Z;
		uint64_t block_mem = block_count * sizeof(Block);

		ImGui::Text("Voxel data: %4d chunks %11s blocks (%5.0f MB  %5.0f KB avg / chunk)", chunk_count, format_thousands(block_count).c_str(), (float)block_mem/1024/1024, (float)block_mem/1024 / chunk_count);

		uint64_t face_count = 0;
		for (Chunk& c : *this) {
			face_count += c.mesh.opaque_faces.size() / 6;
			face_count += c.mesh.transparent_faces.size() / 6;
		}
		uint64_t mesh_mem = face_count * 6 * sizeof(ChunkMesh::Vertex);

		ImGui::Text("Mesh data:  %11s faces (%5.0f MB  %5.0f KB avg / chunk)", format_thousands(face_count).c_str(), (float)mesh_mem/1024/1024, (float)mesh_mem/1024 / chunk_count);

		imgui_pop();
	}

	struct Iterator {
		decltype(chunks)::iterator it;

		Iterator (decltype(chunks)::iterator it): it{it} {} 

		bool operator== (Iterator const& r) { return it == r.it; }
		bool operator!= (Iterator const& r) { return it != r.it; }

		Chunk& operator* () { return *it->second.get(); }

		Iterator operator++ () { ++it; return *this; }
		Iterator operator++ (int) { it++; return *this; }
	};
	struct CIterator {
		decltype(chunks)::const_iterator it;

		CIterator (decltype(chunks)::const_iterator it): it{it} {} 

		bool operator== (CIterator const& r) { return it == r.it; }
		bool operator!= (CIterator const& r) { return it != r.it; }

		Chunk const& operator* () { return *it->second.get(); }

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

	int count () {
		return (int)chunks.size();
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

