#pragma once
#include "common.hpp"
#include "blocks.hpp"
#include "assets.hpp"

#define CHUNK_SIZE			64
#define CHUNK_SIZE_SHIFT	6 // for pos >> CHUNK_SIZE_SHIFT
#define CHUNK_SIZE_MASK		63
#define CHUNK_ROW_OFFS		CHUNK_SIZE
#define CHUNK_LAYER_OFFS	(CHUNK_SIZE * CHUNK_SIZE)

#define CHUNK_VOXEL_COUNT	(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE)

#define POS2IDX(x,y,z) ((size_t)(z) * CHUNK_LAYER_OFFS + (size_t)(y) * CHUNK_ROW_OFFS + (size_t)(x))
#define OFFS2IDX(x,y,z) ((int64_t)(z) * CHUNK_LAYER_OFFS + (int64_t)(y) * CHUNK_ROW_OFFS + (int64_t)(x))

#define VEC2IDX(xyz) ((size_t)(xyz).z * CHUNK_LAYER_OFFS + (size_t)(xyz).y * CHUNK_ROW_OFFS + (size_t)(xyz).x)

#define U16_NULL			((uint16_t)-1)

typedef uint16_t			slice_id;
typedef uint16_t			chunk_id;

#define MAX_CHUNKS			((1<<16)-1)
#define MAX_SLICES			((1<<16)-1)
#define MAX_CHUNK_SLICES	32

#define CHUNK_SLICE_BYTESIZE	(64 * 1024)
static_assert(CHUNK_SLICE_BYTESIZE / sizeof(BlockMeshInstance) < ((1<<16)-1), "");
static constexpr uint16_t CHUNK_SLICE_LENGTH = CHUNK_SLICE_BYTESIZE / sizeof(BlockMeshInstance);

static constexpr int3 OFFSETS[6] = {
	int3(-1,0,0), int3(+1,0,0),
	int3(0,-1,0), int3(0,+1,0),
	int3(0,0,-1), int3(0,0,+1),
};
static constexpr int BLOCK_OFFSETS[6] = {
	-1,
	+1,
	-CHUNK_ROW_OFFS,
	+CHUNK_ROW_OFFS,
	-CHUNK_LAYER_OFFS,
	+CHUNK_LAYER_OFFS,
};

// get world chunk position from world block position
inline int3 to_chunk_pos (int3 pos) {

	int3 chunk_pos;
	chunk_pos.x = pos.x >> CHUNK_SIZE_SHIFT;
	chunk_pos.y = pos.y >> CHUNK_SIZE_SHIFT;
	chunk_pos.z = pos.z >> CHUNK_SIZE_SHIFT;

	return chunk_pos;
}
inline int3 to_chunk_pos (int3 pos, int3* block_pos) {

	int3 chunk_pos;
	chunk_pos.x = pos.x >> CHUNK_SIZE_SHIFT;
	chunk_pos.y = pos.y >> CHUNK_SIZE_SHIFT;
	chunk_pos.z = pos.z >> CHUNK_SIZE_SHIFT;

	block_pos->x = pos.x & CHUNK_SIZE_MASK;
	block_pos->y = pos.y & CHUNK_SIZE_MASK;
	block_pos->z = pos.z & CHUNK_SIZE_MASK;

	return chunk_pos;
}

struct World;
struct WorldGenerator;
struct Player;
struct Assets;
struct Chunks;
class Renderer;

struct ChunkVoxels {
	block_id* ids;
	block_id sparse_id;

	void init () {
		ids = nullptr;
		alloc_ids(); // alloc non-sparse initnally bcause worldgen will need it this way
		sparse_id = B_NULL;
	}

	bool is_sparse () {
		return ids == nullptr;
	}

	block_id get_block (int x, int y, int z) const {
		assert(x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE);
		if (ids)
			return ids[POS2IDX(x,y,z)];
		else
			return sparse_id;
	}
	void set_block (int x, int y, int z, block_id id) {
		assert(x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE);
		if (!ids) {
			if (sparse_id == id)
				return;

			densify();
		}
		ids[POS2IDX(x,y,z)] = id;
	}

	void densify () {
		//clog(INFO, ">> densify");
		ZoneScopedC(tracy::Color::Chocolate);

		alloc_ids();

		for (int i=0; i<CHUNK_VOXEL_COUNT; ++i)
			ids[i] = sparse_id;

		sparse_id = B_NULL;
	}
	void sparsify (block_id id) {
		//clog(INFO, ">> sparsify");
		ZoneScopedC(tracy::Color::Chocolate);

		free_ids();
		sparse_id = id;
	}

	void check_sparsify () {
		if (!ids) return;
		ZoneScopedC(tracy::Color::Chocolate);

		bool sparse = true;
	#if 0
		for (size_t i=0; i<CHUNK_VOXEL_COUNT; ++i) {
			if (ids[i] != ids[0]) {
				sparse = false;
				break;
			}
		}
	#elif 1
		uint64_t val = (uint64_t)ids[0] | ((uint64_t)ids[0] << 16) | ((uint64_t)ids[0] << 32) | ((uint64_t)ids[0] << 48);

		uint64_t const* ptr = (uint64_t const*)ids;
		for (size_t i=0; i<CHUNK_VOXEL_COUNT/4; ++i) {
			if (ptr[i] != val) {
				sparse = false;
				break;
			}
		}
	#elif 1
		__m128i ref = _mm_set1_epi16(ids[0]);
		__m128i const* ptr = (__m128i const*)ids;
		__m128i const* end = (__m128i const*)&ids[CHUNK_VOXEL_COUNT];
		do {
			__m128i vals = _mm_load_si128(ptr);
			__m128i cmp = _mm_xor_si128(vals, ref);
			ptr++;

			if (_mm_test_all_zeros(cmp, cmp) == 0) {
				sparse = false;
				break;
			}
		} while (ptr < end);
	#else
	#endif

		if (sparse)
			sparsify(ids[0]);
	}

	void alloc_ids () {
		assert(ids == nullptr);
		ZoneScopedC(tracy::Color::Crimson);
		ids = (block_id*)malloc(sizeof(block_id) * CHUNK_VOXEL_COUNT);
	}
	void free_ids () {
		assert(ids != nullptr);
		ZoneScopedC(tracy::Color::Crimson);
		::free(ids);
		ids = nullptr;
	}

	void free () {
		if (ids)
			free_ids();
	}

	size_t _alloc_size () {
		return ids ? sizeof(block_id) * CHUNK_VOXEL_COUNT : 0;
	}

	void _validate_ids (BlockTypes& blocks) {
		if (ids) {
			for (int i=0; i<CHUNK_VOXEL_COUNT; ++i) {
				assert(ids[i] >= 0 && ids[i] < blocks.count());
			}
		} else {
			assert(sparse_id >= 0 && sparse_id < blocks.count());
		}
	}
};

inline constexpr block_id g_null_chunk[CHUNK_VOXEL_COUNT] = {}; // chunk data filled with B_NULL to optimize meshing with non-loaded neighbours

struct ChunkMesh {
	uint32_t vertex_count;
	slice_id slices[MAX_CHUNK_SLICES];

	int slices_count () {
		return (vertex_count + CHUNK_SLICE_LENGTH-1) / CHUNK_SLICE_LENGTH;
	}
};

struct Chunk {
	enum Flags : uint32_t {
		ALLOCATED	= 1<<0, // identify non-allocated chunks in chunk array, default true so that this get's set 
		LOADED		= 1<<1, // block data valid and safe to use in main thread
		DIRTY		= 1<<2, // blocks were changed, need remesh
	};

	int3 pos;

	Flags flags;

	chunk_id neighbours[6];

	ChunkMesh opaque_mesh;
	ChunkMesh transparent_mesh;

	ChunkVoxels voxels;

	void _validate_flags () {
		if (flags & LOADED) assert(flags & ALLOCATED);
		if (flags & DIRTY) assert(flags & LOADED);
	}
};
ENUM_BITFLAG_OPERATORS_TYPE(Chunk::Flags, uint32_t)

inline float chunk_dist_sq (int3 pos, float3 dist_to) {
	int3 chunk_origin = pos * CHUNK_SIZE;
	return point_box_nearest_dist_sqr((float3)chunk_origin, CHUNK_SIZE, dist_to);
}

struct ChunkKey_Hasher {
	size_t operator() (int3 const& key) const {
		return std::hash<int3>()(key);
	}
};
struct ChunkKey_Comparer {
	bool operator() (int3 const& l, int3 const& r) const {
		return l == r;
	}
};
typedef std_unordered_map<int3, chunk_id, ChunkKey_Hasher, ChunkKey_Comparer> chunk_pos_to_id_map;

struct Chunks {
	Chunk* chunks;
	uint32_t max_id = 0; // max chunk id needed to iterate chunks
	uint32_t count = 0; // number of ALLOCATED chunks

	char* commit_ptr; // end of committed chunk memory
	AllocatorBitset id_alloc;
	AllocatorBitset slices_alloc;

	void init_mesh (ChunkMesh& m) {
		m.vertex_count = 0;
		memset(m.slices, -1, sizeof(m.slices));
	}
	void free_mesh (ChunkMesh& m) {
		for (auto s : m.slices) {
			if (s != U16_NULL)
				slices_alloc.free(s);
			s = U16_NULL;
		}
	}

	chunk_pos_to_id_map pos_to_id;

	Chunk& operator[] (chunk_id id) {
		assert(id < max_id);
		return chunks[id];
	}

	Chunks () {
		chunks = (Chunk*)reserve_address_space(sizeof(Chunk) * MAX_CHUNKS);
		commit_ptr = (char*)chunks;
	}
	~Chunks () {
		for (chunk_id id=0; id<max_id; ++id) {
			if ((chunks[id].flags & Chunk::ALLOCATED) == 0) continue;
			free_chunk(id);
		}

		assert(count == 0);
		assert(max_id == 0);
		assert(pos_to_id.empty());

		release_address_space(chunks, sizeof(Chunk) * MAX_CHUNKS);
	}

	chunk_id alloc_chunk (int3 pos) {
		ZoneScoped;

		if (count >= MAX_CHUNKS)
			throw std::runtime_error("MAX_CHUNKS reached!");

		chunk_id id;
		{
			ZoneScopedN("id_alloc.alloc()");
			id = (chunk_id)id_alloc.alloc();
		}

		max_id = std::max(max_id, (uint32_t)id +1);
		count++;

		if ((char*)&chunks[id+1] > commit_ptr) { // commit pages one at a time when needed
			ZoneScopedNC("commit_pages", tracy::Color::Crimson);
			commit_pages(commit_ptr, os_page_size);
			memset(commit_ptr, 0, os_page_size); // zero new chunks to init flags
			commit_ptr += os_page_size;
		}

		auto& chunk = chunks[id];

		assert((chunks[id].flags & Chunk::ALLOCATED) == 0);
		
		{ // init
			chunk.flags = Chunk::ALLOCATED;
			chunk.pos = pos;
			memset(chunk.neighbours, -1, sizeof(chunk.neighbours));
			init_mesh(chunk.opaque_mesh);
			init_mesh(chunk.transparent_mesh);
			chunk.voxels.init();
		}

		assert(pos_to_id.find(pos) == pos_to_id.end());
		{
			ZoneScopedN("pos_to_id.emplace");
			pos_to_id.emplace(pos, id);
		}

		for (int i=0; i<6; ++i) {
			ZoneScopedN("alloc_chunk::get neighbour");

			chunks[id].neighbours[i] = query_chunk_id(pos + OFFSETS[i]); // add neighbour to our list
			if (chunks[id].neighbours[i] != U16_NULL) {
				assert(chunks[ chunks[id].neighbours[i] ].neighbours[i^1] == U16_NULL);
				chunks[ chunks[id].neighbours[i] ].neighbours[i^1] = id; // add ourself from neighbours neighbour list
			}
		}

		return id;
	}
	void free_chunk (chunk_id id) {
		ZoneScoped;

		auto& chunk = chunks[id];

		chunk.flags = (Chunk::Flags)0;

		for (int i=0; i<6; ++i) {
			auto n = chunk.neighbours[i];
			if (n != U16_NULL) {
				assert(chunks[n].neighbours[i^1] == id); // i^1 flips direction
				chunks[n].neighbours[i^1] = U16_NULL; // delete ourself from neighbours neighbour list
			}
		}

		free_mesh(chunk.opaque_mesh);
		free_mesh(chunk.transparent_mesh);
		chunk.voxels.free();

		pos_to_id.erase(chunks[id].pos);
		id_alloc.free(id);

		while ((char*)&chunks[id_alloc.alloc_end] <= commit_ptr - os_page_size) { // free pages one by one when needed
			ZoneScopedNC("decommit_pages", tracy::Color::Crimson);
			commit_ptr -= os_page_size;
			decommit_pages(commit_ptr, os_page_size);
		}

		max_id = (chunk_id)id_alloc.alloc_end;
		count--;
	}

	// load chunks in this radius in order of distance to the player 
	float load_radius = 700.0f;
	
	// prevent rapid loading and unloading chunks
	// better would be a cache in chunks outside this radius get added (cache size based on desired memory use)
	//  and only the "oldest" chunks should be unloaded
	// This way walking back and forth might not even need to load any chunks at all
	float unload_hyster = CHUNK_SIZE*1.5f;

	int background_queued_count = 0;

	// distance of chunk to player
	int chunk_lod (float dist) {
		return clamp(floori(log2f(dist / load_radius * 16)), 0,3);
	}

	void imgui (Renderer& renderer);

	Chunk* _query_cache = nullptr;

	// lookup a chunk with a chunk coord, returns nullptr chunk not loaded
	chunk_id query_chunk_id (int3 coord) {
		auto it = pos_to_id.find(coord);
		if (it == pos_to_id.end())
			return U16_NULL;

		_query_cache = &chunks[it->second];
		return it->second;
	}
	Chunk* query_chunk (int3 coord) {
		auto it = pos_to_id.find(coord);
		if (it == pos_to_id.end())
			return nullptr;
		Chunk* c = &this->operator[](it->second);
		c->_validate_flags();

		_query_cache = c;
		return c;
	}
	// lookup a block with a world block pos, returns BT_NO_CHUNK for unloaded chunks or BT_OUT_OF_BOUNDS if out of bounds in z
	block_id query_block (int3 pos, Chunk** out_chunk=nullptr, int3* out_block_pos=nullptr);

	void set_block (int3 pos, block_id b);

	// queue and finialize chunks that should be generated
	void update_chunk_loading (World const& world, Player const& player);
};
