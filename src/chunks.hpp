#pragma once
#include "kissmath.hpp"
#include "blocks.hpp"
#include "util/move_only_class.hpp"
#include "util/string.hpp"
#include "util/running_average.hpp"
#include "util/threadpool.hpp"
#include "util/raw_array.hpp"
#include "util/block_allocator.hpp"
#include "graphics/graphics.hpp" // for ChunkMesh
using namespace kiss;

#include "stdint.h"
#include <unordered_map>

typedef int		bpos_t;
typedef int2	bpos2;
typedef int3	bpos;

typedef int		chunk_pos_t;
typedef int3	chunk_coord;

#define CHUNK_DIM			32
#define CHUNK_DIM_SHIFT		5 // for coord >> CHUNK_DIM_SHIFT
#define CHUNK_DIM_MASK		((1 << CHUNK_DIM_SHIFT) -1)
#define CHUNK_ROW_OFFS		(CHUNK_DIM+2)
#define CHUNK_LAYER_OFFS	((CHUNK_DIM+2)*(CHUNK_DIM+2))

#define CHUNK_BLOCK_COUNT	(CHUNK_DIM * CHUNK_DIM * CHUNK_DIM)

// get world chunk coord from world block position
inline chunk_coord get_chunk_from_block_pos (bpos pos) {

	chunk_coord chunk_pos;
	chunk_pos.x = pos.x >> CHUNK_DIM_SHIFT; // arithmetic shift right instead of divide because we want  -10 / 32  to be  -1 instead of 0
	chunk_pos.y = pos.y >> CHUNK_DIM_SHIFT;
	chunk_pos.z = pos.z >> CHUNK_DIM_SHIFT;

	return chunk_pos;
}
// get world chunk coord and block pos in chunk from world block position
inline chunk_coord get_chunk_from_block_pos (bpos pos_world, bpos* bpos_in_chunk) {

	chunk_coord chunk_pos = get_chunk_from_block_pos(pos_world);

	if (bpos_in_chunk) {
		bpos_in_chunk->x = pos_world.x & CHUNK_DIM_MASK;
		bpos_in_chunk->y = pos_world.y & CHUNK_DIM_MASK;
		bpos_in_chunk->z = pos_world.z & CHUNK_DIM_MASK;
	}

	return chunk_pos;
}

// Hashmap key type for chunk_pos
struct chunk_coord_hashmap {
	chunk_coord v;

	bool operator== (chunk_coord_hashmap const& r) const { // for hash map
		return v.x == r.v.x && v.y == r.v.y && v.z == r.v.z;
	}
};

inline size_t hash (chunk_coord v) {
	size_t h;
	h  = std::hash<bpos_t>()(v.x);
	h = 53ull * (h + 53ull);

	h += std::hash<bpos_t>()(v.y);
	h = 53ull * (h + 53ull);

	h += std::hash<bpos_t>()(v.z);
	return h;
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
		count += (CHUNK_DIM >> lod) * (CHUNK_DIM >> lod) * (CHUNK_DIM >> lod);
	}
	return count;
};

////////////// Chunk

struct ChunkData {
	static constexpr uint64_t COUNT = (CHUNK_DIM+2) * (CHUNK_DIM+2) * (CHUNK_DIM+2);

	block_id	id[COUNT];
	uint8		block_light[COUNT];
	uint8		sky_light[COUNT];
	uint8		hp[COUNT];

	static constexpr uint64_t pos_to_index (bpos pos) {
		return (pos.z + 1) * (CHUNK_DIM+2) * (CHUNK_DIM+2) + (pos.y + 1) * (CHUNK_DIM+2) + (pos.x + 1);
	}

	Block get (bpos pos) {
		assert(all(pos >= -1 && pos <= CHUNK_DIM+1));

		Block b;

		auto indx = pos_to_index(pos);

		b.id			= id[indx];
		b.block_light	= block_light[indx];
		b.sky_light		= sky_light[indx];
		b.hp			= hp[indx];

		return b;
	}
	void set (bpos pos, Block b) {
		assert(all(pos >= -1 && pos <= CHUNK_DIM+1));

		auto indx = pos_to_index(pos);

		id[indx]			= b.id;
		block_light[indx]	= b.block_light;
		sky_light[indx]		= b.sky_light;
		hp[indx]			= b.hp;
	}

	void init_border ();
};

static constexpr uint64_t MESHING_BLOCK_BYTESIZE = (1024 * 1024);
static constexpr uint64_t MESHING_BLOCK_COUNT = MESHING_BLOCK_BYTESIZE / sizeof(ChunkMesh::Vertex);

union MeshingBlock {
	ChunkMesh::Vertex verts[MESHING_BLOCK_COUNT];

	// for padding to make size be exactly MESHING_BLOCK_BYTESIZE, to maybe get faster allocation when allocating POT sized
	char _padding[MESHING_BLOCK_BYTESIZE];
};

// one for each thread (also gets initialized for the threads that don't need it I think, but thats ok, does not do anything reall on construction)
extern BlockAllocator<MeshingBlock> meshing_allocator;

// To avoid allocation and memcpy when the meshing data grows larger than predicted,
//  we output the mesh data into blocks, which can be allocated by BlockAllocator, which reuses freed blocks
//  instead a list of 
struct MeshingData {
	static constexpr uint64_t BLOCK_COUNT = 64;

	MeshingBlock* blocks[BLOCK_COUNT]; // large enough
	uint64_t vertex_count;
	uint64_t block_count;

	ChunkMesh::Vertex* cur;
	ChunkMesh::Vertex* end;

	void add_block () {
		blocks[block_count] = meshing_allocator.alloc_threadsafe();

		cur = &blocks[block_count]->verts[0];
		end = &blocks[block_count]->verts[MESHING_BLOCK_COUNT];

		block_count++;
	}

	void init () {
		vertex_count = 0;
		block_count = 0;
		add_block();
	}

	ChunkMesh::Vertex* push () {
		if (cur == end) {
			if (block_count >= BLOCK_COUNT)
				return nullptr; // whoops
			add_block();
		}
		vertex_count++;
		return cur++;
	}

	// upload (and free data in this structure)
	void upload (Mesh<ChunkMesh::Vertex>& mesh) {
		//mesh._alloc(vertex_count);
		//mesh.vertex_count = vertex_count;

		uint64_t offset = 0;
		uint64_t remain = vertex_count;
		int cur_block = 0;

		while (remain > 0 && cur_block < block_count) {

			//mesh._sub_upload(&blocks[cur_block++]->verts[0], offset, std::min(remain, MESHING_BLOCK_COUNT));
			cur_block++;

			offset += MESHING_BLOCK_COUNT;
			remain -= MESHING_BLOCK_COUNT;
		}

		for (int i=0; i<block_count; ++i) {
			meshing_allocator.free_threadsafe(blocks[i]); // technically this does not need to be threadsafe since all the threads are done by the time the main thread starts uploading, but just to be sure
		}
	}
};

struct MeshingResult {
	MeshingData opaque_vertices;
	MeshingData tranparent_vertices;
};

class Chunk {
	NO_MOVE_COPY_CLASS(Chunk)
public:
	const chunk_coord coord;

	Chunk (chunk_coord coord);
	void init_blocks ();

	static bpos chunk_pos_world (chunk_coord coord) {
		return coord * CHUNK_DIM;
	}
	bpos chunk_pos_world () const {
		return chunk_pos_world(coord);
	}

	// access blocks raw, only use in World Generator since neighbours are not notified of block changed with these!
	void set_block_unchecked (bpos pos, Block b); // only use in World Generator

	// get block
	Block get_block (bpos pos) const;
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
	friend struct ChunkGenerator;
	friend void update_sky_light_column (Chunk* chunk, bpos pos_in_chunk);
	// block data
	//  with border that stores a copy of the blocks of our neighbour along the faces (edges and corners are invalid)
	//  border gets automatically kept in sync if only set_block() is used to update blocks
	std::unique_ptr<ChunkData> blocks = nullptr;
public:

	// Gpu mesh data
	ChunkMesh mesh;
	uint64_t face_count;

	void reupload (MeshingResult& result);
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

struct ParallelismJob { // Chunk remesh
	// input
	Chunk* chunk;
	Chunks* chunks; // not modfied
	Graphics const* graphics;
	WorldGenerator const* wg; 
	// output
	MeshingResult meshing_result;
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
	float generation_radius = 250.0f;
	
	// prevent rapid loading and unloading chunks
	// better would be a cache in chunks outside this radius get added (cache size based on desired memory use)
	//  and only the "oldest" chunks should be unloaded
	// This way walking back and forth might not even need to load any chunks at all
	float deletion_hysteresis = CHUNK_DIM*1.5f;

	float active_radius = 200.0f;

	// artifically limit (delay) meshing of chunks to prevent complete freeze of main thread at the cost of some visual artefacts
	int max_chunk_gens_processed_per_frame = 64; // limit both queueing and finalizing, since (at least for now) the queuing takes too long (causing all chunks to be generated in the first frame, not like I imagined...)
	int max_chunks_meshed_per_frame = max(parallelism_threads*2, 4); // max is 2 meshings per cpu core per frame

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
		uint64_t block_count = chunk_count * (uint64_t)CHUNK_DIM * CHUNK_DIM * CHUNK_DIM;
		uint64_t block_mem = chunk_count * sizeof(ChunkData);

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
	Block query_block (bpos p, Chunk** out_chunk=nullptr, bpos* out_block_pos_chunk=nullptr);

	// unload chunk at coord (invalidates iterators, so dont call this in a loop)
	ChunkHashmap::Iterator unload_chunk (ChunkHashmap::Iterator it);

	void remesh_all ();

	// queue and finialize chunks that should be generated
	void update_chunk_loading (World const& world, WorldGenerator const& world_gen, Player const& player);

	// chunk meshing to prepare for drawing
	void update_chunks (Graphics const& graphics, WorldGenerator const& wg, Player const& player);
};
