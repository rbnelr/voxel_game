#pragma once
#include "common.hpp"
#include "blocks.hpp"
#include "assets.hpp"

#define CHUNK_SIZE			64 // size of chunk in blocks per axis
#define CHUNK_SIZE_SHIFT	6 // for pos >> CHUNK_SIZE_SHIFT
#define CHUNK_SIZE_MASK		63

#if 1
#define SUBCHUNK_SIZE		4 // size of subchunk in blocks per axis
#define SUBCHUNK_COUNT		(CHUNK_SIZE / SUBCHUNK_SIZE) // size of chunk in subchunks per axis
#define SUBCHUNK_SHIFT		2
#define SUBCHUNK_MASK		3
#else
#define SUBCHUNK_SIZE		8 // size of subchunk in blocks per axis
#define SUBCHUNK_COUNT		(CHUNK_SIZE / SUBCHUNK_SIZE) // size of chunk in subchunks per axis
#define SUBCHUNK_SHIFT		3
#define SUBCHUNK_MASK		7
#endif

#define IDX3D(x,y,z, sz) (size_t)(z) * (sz)*(sz) + (size_t)(y) * (sz) + (size_t)(x)

// Get array index of subchunk in chunk data from chunk-relative xyz
#define SUBCHUNK_IDX(x,y,z) ( (uint32_t)((z) >> SUBCHUNK_SHIFT) * SUBCHUNK_COUNT * SUBCHUNK_COUNT \
                            + (uint32_t)((y) >> SUBCHUNK_SHIFT) * SUBCHUNK_COUNT \
                            + (uint32_t)((x) >> SUBCHUNK_SHIFT) )
// Get array index of block in subchunk from chunk-relative xyz
#define BLOCK_IDX(x,y,z)  ( (uint32_t)((z) & SUBCHUNK_MASK) * SUBCHUNK_SIZE * SUBCHUNK_SIZE \
                          + (uint32_t)((y) & SUBCHUNK_MASK) * SUBCHUNK_SIZE \
                          + (uint32_t)((x) & SUBCHUNK_MASK) )

// Seperate world-block pos into chunk pos and block pos in chunk
#define CHUNK_BLOCK_POS(X,Y,Z, CHUNK_POS, BX, BY, BZ) do { \
		(CHUNK_POS).x = (X) >> CHUNK_SIZE_SHIFT; \
		(CHUNK_POS).y = (Y) >> CHUNK_SIZE_SHIFT; \
		(CHUNK_POS).z = (Z) >> CHUNK_SIZE_SHIFT; \
		BX = (X) & SUBCHUNK_MASK; \
		BY = (Y) & SUBCHUNK_MASK; \
		BZ = (Z) & SUBCHUNK_MASK; \
	} while (0)

#define CHUNK_VOXEL_COUNT		(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE) // number of voxels per chunk
#define CHUNK_SUBCHUNK_COUNT	(SUBCHUNK_COUNT * SUBCHUNK_COUNT * SUBCHUNK_COUNT) // number of subchunks per chunk
#define SUBCHUNK_VOXEL_COUNT	(SUBCHUNK_SIZE * SUBCHUNK_SIZE * SUBCHUNK_SIZE) // number of voxels per subchunk

#define U16_NULL			((uint16_t)-1)

typedef uint16_t			slice_id;
typedef uint16_t			chunk_id;

#define MAX_CHUNKS			((1<<16)-1) // one less than POT to allow i<N loop condition and leave -1u as null value
#define MAX_SLICES			((1<<16)-1) // one less than POT to allow i<N loop condition and leave -1u as null value
#define MAX_CHUNK_SLICES	32

#define CHUNK_SLICE_BYTESIZE	(64 *KB)
static_assert(CHUNK_SLICE_BYTESIZE / sizeof(BlockMeshInstance) < ((1<<16)-1), "");
static constexpr uint16_t CHUNK_SLICE_LENGTH = CHUNK_SLICE_BYTESIZE / sizeof(BlockMeshInstance);

static constexpr int3 OFFSETS[6] = {
	int3(-1,0,0), int3(+1,0,0),
	int3(0,-1,0), int3(0,+1,0),
	int3(0,0,-1), int3(0,0,+1),
};

struct World;
struct WorldGenerator;
struct Player;
struct Assets;
struct Chunks;
class Renderer;
struct ChunkSliceData;
struct WorldgenJob;

#define MAX_SUBCHUNKS		((32ull *GB) / sizeof(SubchunkVoxels))

inline constexpr block_id g_null_chunk[CHUNK_VOXEL_COUNT] = {}; // chunk data filled with B_NULL to optimize meshing with non-loaded neighbours

struct ChunkMesh {
	uint32_t vertex_count;
	slice_id slices[MAX_CHUNK_SLICES];

	int slices_count () {
		return (vertex_count + CHUNK_SLICE_LENGTH-1) / CHUNK_SLICE_LENGTH;
	}
};

// 3-Level Sparse storage system for voxels
// this system is similar to how a octree would work
// but instead of every power of two being a size at which regions can be sparse
// this system has 'chunks' and 2nd level 'subchunks' which can store their contents sparsely
// 3-Level because 1st level is a sparse chunk, 2nd is a sparse subchunk, 3rd is a fully dense voxel
//   Chunk-Subchunk-Block vs Chunk-Block vs 128-64-32-16-8-4-2-1 (Octree)
// Chunks are currently 64^3 and are the blocks worldgen happens in,
// which means they can not only be sparse (only 1 block id in entire region) but also additionally unloaded (queries currently return B_NULL)

struct SubchunkVoxels {
	block_id voxels[SUBCHUNK_VOXEL_COUNT];
};
struct ChunkVoxels {
	// data for all subchunks
	// sparse subchunk:  block_id of all subchunk voxels
	// dense  subchunk:  id of subchunk
	uint32_t sparse_data[CHUNK_SUBCHUNK_COUNT];

	// packed bits for all subchunks, where  0: dense subchunk  1: sparse subchunk
	uint64_t sparse_bits[CHUNK_SUBCHUNK_COUNT / 64];

	// Use comma operator to assert and return value in expression
#define CHECK_BLOCK(b) (assert((b) > B_NULL && (b) < (block_id)g_assets.block_types.blocks.size()) , b)
	//#define CHECK_BLOCK(b) ( ((b) > B_NULL && (b) < (block_id)g_assets.block_types.blocks.size()) ? b : B_NULL )

	bool is_subchunk_sparse (uint32_t subc_i) {
		auto test = sparse_bits[subc_i >> 6] & (1ull << (subc_i & 63));
		return test != 0;
	}
	void set_subchunk_sparse (uint32_t subc_i) {
		sparse_bits[subc_i >> 6] |= 1ull << (subc_i & 63);
	}
	void set_subchunk_dense (uint32_t subc_i) {
		sparse_bits[subc_i >> 6] &= ~(1ull << (subc_i & 63));
	}
};

struct Chunk {
	enum Flags : uint32_t {
		ALLOCATED		= 1<<0, // Set when chunk was allocated, exists so that zero-inited memory allocated by BlockAllocator is interpreted as unallocated chunks (so we can simply iterate over the memory while checking flags)
		LOADED			= 1<<1, // block data valid and safe to use in main thread
		SPARSE_VOXELS	= 1<<2, // voxel_data is a single block id instead of a dense_chunk id
		VOXELS_DIRTY	= 1<<3, // voxels were changed, run checked_sparsify
		REMESH			= 1<<4, // need remesh due to voxel change, neighbour chunk change, etc.
	};

	Flags flags;
	
	int3 pos;

	uint16_t voxel_data; // if SPARSE_VOXELS: non-null block id   if !SPARSE_VOXELS: id to dense_chunks

	chunk_id neighbours[6];

	ChunkMesh opaque_mesh;
	ChunkMesh transparent_mesh;

	void _validate_flags () {
		if ((flags & ALLOCATED) == 0) assert(flags == (Flags)0);
		if (flags & VOXELS_DIRTY) assert(flags & REMESH);
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

const lrgba DBG_CHUNK_COL			= srgba(0, 0, 255, 255);
const lrgba DBG_SPARSE_CHUNK_COL	= srgba(0, 0, 200, 20);
const lrgba DBG_CULLED_CHUNK_COL	= srgba(255, 0, 0, 180);
const lrgba DBG_DENSE_SUBCHUNK_COL	= srgba(234, 90, 0, 255);

struct Chunks {
	BlockAllocator<Chunk>			chunks			= { MAX_CHUNKS };
	BlockAllocator<ChunkVoxels>		dense_chunks	= { MAX_CHUNKS };
	BlockAllocator<SubchunkVoxels>	dense_subchunks	= { MAX_SUBCHUNKS };

	AllocatorBitset slices_alloc;

	void init_mesh (ChunkMesh& m) {
		m.vertex_count = 0;
		memset(m.slices, -1, sizeof(m.slices));
	}
	void free_mesh (ChunkMesh& m) {
		for (auto& s : m.slices) {
			if (s != U16_NULL)
				slices_alloc.free(s);
			s = U16_NULL;
		}
		m.vertex_count = 0;
	}

	void init_voxels (Chunk& c);
	void free_voxels (Chunk& c);

	void densify_chunk (Chunk& c);
	void densify_subchunk (ChunkVoxels& dc, uint32_t subchunk_i, uint32_t& subchunk_val);

	void checked_sparsify_chunk (Chunk& c);
	bool checked_sparsify_subchunk (ChunkVoxels& dc, uint32_t subchunk_i);

	void sparse_chunk_from_worldgen (Chunk& c, WorldgenJob& j);

	chunk_pos_to_id_map pos_to_id;

	Chunk& operator[] (chunk_id id) {
		return chunks[id];
	}
	// End of array of chunks for iteration (not all are allocated, check flags)
	chunk_id end () {
		return (chunk_id)chunks.slots.alloc_end;
	}

	chunk_id alloc_chunk (int3 pos);
	void free_chunk (chunk_id id);

	// for renderer switch
	void renderer_switch () {
		//assert(upload_slices.empty()); // Can have upload_slices here if a renderer did not consume them last frame, but these will simply be overwritten by newer duplicate versions, which is safe
		
		for (chunk_id cid=0; cid<end(); ++cid) {
			auto& chunk = chunks[cid];
			if ((chunk.flags & Chunk::LOADED) == 0) continue;
			chunks[cid].flags |= Chunk::REMESH; // remesh chunk to make sure new renderer gets all meshes uploaded again
		}
	}

	// load chunks in this radius in order of distance to the player 
	float load_radius = 400;//700.0f;
	
	// prevent rapid loading and unloading chunks
	// better would be a cache in chunks outside this radius get added (cache size based on desired memory use)
	//  and only the "oldest" chunks should be unloaded
	// This way walking back and forth might not even need to load any chunks at all
	float unload_hyster = 0;//CHUNK_SIZE*1.5f;

	int background_queued_count = 0;

	bool mesh_world_border = false;

	bool visualize_chunks = false;
	bool visualize_subchunks = false;
	bool debug_frustrum_culling = false;

	// distance of chunk to player
	int chunk_lod (float dist) {
		return clamp(floori(log2f(dist / load_radius * 16)), 0,3);
	}

	void imgui (Renderer* renderer);

	void visualize_chunk (Chunk& chunk, bool empty, bool culled);

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

	// read a block with a world block pos, returns B_NULL for unloaded chunks
	block_id read_block (int x, int y, int z);
	// read a block with a chunk block pos
	block_id read_block (int x, int y, int z, Chunk const* c);

	// 
	void write_block (int x, int y, int z, block_id bid);
	//
	void write_block (int x, int y, int z, Chunk* c, block_id bid);

	// queue and finialize chunks that should be generated
	void update_chunk_loading (World const& world, Player const& player);
	
	struct UploadSlice {
		slice_id		sliceid;
		ChunkSliceData*	data;
	};
	std_vector<UploadSlice> upload_slices;

	// queue and finialize chunks that should be generated
	void update_chunk_meshing (World const& world);
};
