#pragma once
#include "common.hpp"
#include "blocks.hpp"

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

struct World;
struct WorldGenerator;
struct Player;
struct Graphics;
struct Chunks;

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

struct ChunkMesh {
	struct Vertex {
		float3	pos_model;
		float2	uv;
		uint8	tex_indx;
		uint8	block_light;
		uint8	sky_light;
		uint8	hp;

		//static void bind (Attributes& a) {
		//	int cur = 0;
		//	a.add    <decltype(pos_model  )>(cur++, "pos_model" ,  sizeof(Vertex), offsetof(Vertex, pos_model  ));
		//	a.add    <decltype(uv         )>(cur++, "uv",          sizeof(Vertex), offsetof(Vertex, uv         ));
		//	a.add_int<decltype(tex_indx   )>(cur++, "tex_indx",    sizeof(Vertex), offsetof(Vertex, tex_indx   ));
		//	a.add    <decltype(block_light)>(cur++, "block_light", sizeof(Vertex), offsetof(Vertex, block_light), true);
		//	a.add    <decltype(sky_light  )>(cur++, "sky_light",   sizeof(Vertex), offsetof(Vertex, sky_light  ), true);
		//	a.add    <decltype(hp         )>(cur++, "hp",          sizeof(Vertex), offsetof(Vertex, hp         ), true);
		//}
	};

	//std::vector<Vertex> opaque_faces;
	//std::vector<Vertex> transparent_faces;

	//Mesh<Vertex> opaque_mesh;
	//Mesh<Vertex> transparent_mesh;
};

static constexpr uint64_t MESHING_BLOCK_BYTESIZE = (1024 * 1024);
static constexpr uint64_t MESHING_BLOCK_COUNT = MESHING_BLOCK_BYTESIZE / sizeof(ChunkMesh::Vertex);

union MeshingBlock {
	ChunkMesh::Vertex verts[MESHING_BLOCK_COUNT];

	// for padding to make size be exactly MESHING_BLOCK_BYTESIZE, to maybe get faster allocation when allocating POT sized
	char _padding[MESHING_BLOCK_BYTESIZE];
};

// Custom memory allocator that allocates in fixed blocks using a freelist
// used to avoid malloc and free overhead
template <typename T>
class BlockAllocator {
	union Block {
		Block*	next; // link to next block in linked list of free blocks
		T		data; // data if allocated
	};

	Block* freelist = nullptr;

	mutable std::mutex m;

public:
	// allocate a T (not threadsafe)
	Block* _alloc () {
		if (!freelist)
			return nullptr;

		// remove first Block of freelist
		Block* block = freelist;
		freelist = block->next;

		return block;
	}

	T* alloc () {
		Block* block = _alloc();
		if (!block) {
			ZoneScopedN("malloc BlockAllocator::alloc");
			// allocate new blocks as needed
			block = (Block*)malloc(sizeof(Block));
		}
		return &block->data;
	}

	// free a ptr (not threadsafe)
	void free (T* ptr) {
		// blocks are never freed for now

		Block* block = (Block*)ptr;

		// add block to freelist
		block->next = freelist;
		freelist = block;
	}

	T* alloc_threadsafe () {
		Block* block;
		{
			ZoneScopedN("mutex BlockAllocator::alloc_threadsafe");

			std::lock_guard<std::mutex> lock(m);
			block = _alloc();
		}
		// Do the malloc outside the block because malloc can take a long time, which can catastrophically block an entire threadpool
		if (!block) {
			ZoneScopedN("malloc BlockAllocator::alloc_threadsafe");

			// allocate new blocks as needed
			block = (Block*)malloc(sizeof(Block)); // NOTE: malloc itself mutexes, so there is little point to putting this outside of the lock
		}
		return &block->data;
	}

	void free_threadsafe (T* ptr) {
		ZoneScopedN("mutex BlockAllocator::free_threadsafe");

		std::lock_guard<std::mutex> lock(m);
		free(ptr);
	}

	~BlockAllocator () {
		while (freelist) {
			ZoneScopedN("free() ~BlockAllocator");

			Block* block = freelist;
			freelist = block->next;

			::free(block);
		}
	}

	int count () {
		int count = 0;

		Block* block = freelist;
		while (block) {
			block = block->next;
			count++;
		}

		return count;
	}
};

// one for each thread (also gets initialized for the threads that don't need it I think, but thats ok, does not do anything reall on construction)
inline BlockAllocator<MeshingBlock> meshing_allocator;

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
	void upload (/*Mesh<ChunkMesh::Vertex>& mesh*/) {
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

struct Chunk {
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

	friend struct Chunk_Mesher;
	friend struct ChunkGenerator;
	friend void update_sky_light_column (Chunk* chunk, bpos pos_in_chunk);
	// block data
	//  with border that stores a copy of the blocks of our neighbour along the faces (edges and corners are invalid)
	//  border gets automatically kept in sync if only set_block() is used to update blocks
	std::unique_ptr<ChunkData> blocks = nullptr;

	// Gpu mesh data
	ChunkMesh mesh;
	uint64_t face_count;

	void reupload (MeshingResult& result);
};

////////////// Chunks
inline const int logical_cores = std::thread::hardware_concurrency();

// as many background threads as there are logical cores to allow background threads to use even the main threats time when we are gpu bottlenecked or at an fps cap
inline const int background_threads  = max(logical_cores, 1);

// main thread + parallelism_threads = logical cores to allow the main thread to join up with the rest of the cpu to work on parallel work that needs to be done immidiately
inline const int parallelism_threads = max(logical_cores >= 4 ? logical_cores - 1 : logical_cores, 1); // leave one thread for system and background apps, but not on dual core systems

inline Threadpool background_threadpool  = Threadpool{ background_threads , ThreadPriority::LOW, ">> background threadpool"  };
inline Threadpool parallelism_threadpool = Threadpool{ parallelism_threads - 1, ThreadPriority::HIGH,   ">> parallelism threadpool" }; // parallelism_threads - 1 to let main thread contribute work too


struct ChunkHashmap {
	typedef std::unordered_map<int3, std::unique_ptr<Chunk>> hashmap_t; 

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

struct Chunks {
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

	void imgui () {
		if (!imgui_push("Chunks")) return;

		ImGui::DragFloat("generation_radius", &generation_radius, 1);
		ImGui::DragFloat("deletion_hysteresis", &deletion_hysteresis, 1);
		ImGui::DragFloat("active_radius", &active_radius, 1);

		int chunk_count = chunks.count();
		uint64_t block_count = chunk_count * (uint64_t)CHUNK_DIM * CHUNK_DIM * CHUNK_DIM;
		uint64_t block_mem = chunk_count * sizeof(ChunkData);

		ImGui::Text("Voxel data: %4d chunks %11s blocks (%5.0f MB  %5.0f KB avg / chunk)", chunk_count, format_thousands(block_count).c_str(), (float)block_mem/1024/1024, (float)block_mem/1024 / chunk_count);

		//uint64_t face_count = 0;
		//for (Chunk& c : chunks) {
		//	face_count += c.face_count;
		//}
		//uint64_t mesh_mem = face_count * 6 * sizeof(ChunkMesh::Vertex);
		//
		//ImGui::Text("Mesh data:  %11s faces (%5.0f MB  %5.0f KB avg / chunk)", format_thousands(face_count).c_str(), (float)mesh_mem/1024/1024, (float)mesh_mem/1024 / chunk_count);

		imgui_pop();
	}

	// in chunks
	// lookup a chunk with a chunk coord, returns nullptr chunk not loaded
	Chunk* query_chunk (chunk_coord coord);
	// lookup a block with a world block pos, returns BT_NO_CHUNK for unloaded chunks or BT_OUT_OF_BOUNDS if out of bounds in z
	Block query_block (bpos pos, Chunk** out_chunk=nullptr, bpos* out_block_pos_chunk=nullptr);

	void set_block (bpos pos, Block& b);

	// unload chunk at coord (invalidates iterators, so dont call this in a loop)
	ChunkHashmap::Iterator unload_chunk (ChunkHashmap::Iterator it);

	void remesh_all ();

	// queue and finialize chunks that should be generated
	void update_chunk_loading (World const& world, WorldGenerator const& wg, Player const& player);

	// chunk meshing to prepare for drawing
	void update_chunks (Graphics const& g, WorldGenerator const& wg, Player const& player);
};
