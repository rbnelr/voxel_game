#pragma once
#include "common.hpp"
#include "blocks.hpp"
#include "assets.hpp"

#define CHUNK_SIZE			64 // size of chunk in blocks per axis
#define CHUNK_SIZE_SHIFT	6 // for pos >> CHUNK_SIZE_SHIFT
#define CHUNK_SIZE_MASK		63

#define SUBCHUNK_SIZE		4 // size of subchunk in blocks per axis
#define SUBCHUNK_COUNT		(CHUNK_SIZE / SUBCHUNK_SIZE) // size of chunk in subchunks per axis
#define SUBCHUNK_SHIFT		2
#define SUBCHUNK_MASK		3

#define IDX3D(x,y,z, sz) (size_t)(z) * (sz)*(sz) + (size_t)(y) * (sz) + (size_t)(x)

// Get array index of subchunk in chunk data from chunk-relative xyz
#define SUBCHUNK_IDX(x,y,z) ( (size_t)((z) >> SUBCHUNK_SHIFT) * SUBCHUNK_COUNT * SUBCHUNK_COUNT \
                            + (size_t)((y) >> SUBCHUNK_SHIFT) * SUBCHUNK_COUNT \
                            + (size_t)((x) >> SUBCHUNK_SHIFT) )
// Get array index of block in subchunk from chunk-relative xyz
#define BLOCK_IDX(x,y,z)  ( (size_t)((z) & SUBCHUNK_MASK) * SUBCHUNK_SIZE * SUBCHUNK_SIZE \
                          + (size_t)((y) & SUBCHUNK_MASK) * SUBCHUNK_SIZE \
                          + (size_t)((x) & SUBCHUNK_MASK) )

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

struct World;
struct WorldGenerator;
struct Player;
struct Assets;
struct Chunks;
class Renderer;

// 3-Level Sparse storage system for voxels
// this system is similar to how a octree would work
// but instead of every power of two being a size at which regions can be sparse
// this system has 'chunks' and 2nd level 'subchunks' which can store their contents sparsely
// 3-Level because 1st level is a sparse chunk, 2nd is a sparse subchunk, 3rd is a fully dense voxel
//   Chunk-Subchunk-Block vs Chunk-Block vs 128-64-32-16-8-4-2-1 (Octree)
// Chunks are currently 64^3 and are the blocks worldgen happens in,
// which means they can not only be sparse (only 1 block id in entire region) but also additionally unloaded (queries currently return B_NULL)

struct ChunkVoxels {
	// sparse chunks only contain one voxel data (block id) and an empty pointer, while non-sparse chunks

	struct DenseChunk {
		// data for all subchunks
		// sparse subchunk:  block_id of all subchunk voxels
		// dense  subchunk:  index into dense_data where subchunk data is found
		uint16_t sparse_data[CHUNK_SUBCHUNK_COUNT];

		// packed bits for all subchunks, where  0: dense subchunk  1: sparse subchunk
		uint64_t sparse_bits[CHUNK_SUBCHUNK_COUNT / 64];

		struct SubchunkData {
			block_id voxels[SUBCHUNK_SIZE * SUBCHUNK_SIZE * SUBCHUNK_SIZE];
		};

		SubchunkData* dense_data; // resizeable list of dense subchunks indexed by sparse_data indices

		// Initial allocation is fully non-sparse with uninited data for worldgen to fill
		void init_dense () {
			for (uint16_t i=0; i<CHUNK_SUBCHUNK_COUNT; ++i)
				sparse_data[i] = i;

			memset(&sparse_bits, 0, sizeof(sparse_bits));

			dense_data = (SubchunkData*)malloc(sizeof(SubchunkData) * CHUNK_SUBCHUNK_COUNT);
		}
		// Init chunk entirely to one data (so simply keep all subchunks sparse)
		// this is for writes to completely sparse chunks that make them non-sparse (one subchunk then get's written afterwards)
		void init_sparse (block_id data) {
			for (uint16_t i=0; i<CHUNK_SUBCHUNK_COUNT; ++i)
				sparse_data[i] = data;

			memset(&sparse_bits, -1, sizeof(sparse_bits));

			dense_data = nullptr;
		}

		// Use comma operator to assert and return value in expression
		#define CHECK_BLOCK(b) (assert((b) > B_NULL && (b) < (block_id)g_assets.block_types.blocks.size()) , b)
		//#define CHECK_BLOCK(b) ( ((b) > B_NULL && (b) < (block_id)g_assets.block_types.blocks.size()) ? b : B_NULL )

		bool is_subchunk_sparse (size_t subc_i) {
			auto test = sparse_bits[subc_i >> 6] & (1ull << (subc_i & 63));
			return test != 0;
		}
		void set_subchunk_sparse (size_t subc_i) {
			sparse_bits[subc_i >> 6] |= 1ull << (subc_i & 63);
		}
		void set_subchunk_dense (size_t subc_i) {
			sparse_bits[subc_i >> 6] &= ~(1ull << (subc_i & 63));
		}

		block_id read_block (int x, int y, int z) {
			size_t subc_i = SUBCHUNK_IDX(x,y,z);
			auto sparse_val = sparse_data[subc_i];

			if (is_subchunk_sparse(subc_i)) {
				return CHECK_BLOCK( (block_id)sparse_val );
			}

			auto& subchunk = dense_data[sparse_val];

			auto blocki = BLOCK_IDX(x,y,z);
			auto block = subchunk.voxels[blocki];

			return CHECK_BLOCK(block);
		}
		void write_block (int x, int y, int z, block_id data) {
			size_t subc_i = SUBCHUNK_IDX(x,y,z);
			auto sparse_val = sparse_data[subc_i];

			assert(false);

			if (is_subchunk_sparse(subc_i)) {
				// to implement
				assert(false);
			}

			dense_data[sparse_val].voxels[BLOCK_IDX(x,y,z)] = data;
		}

		bool checked_sparsify () {
			
			//for (size_t subc_i=0; subc_i < CHUNK_SUBCHUNK_COUNT; ++subc_i) {
			//	
			//}

			return false;
		}
	};
	
	block_id sparse_data; // if sparse: non-null block id   if non-sparse: B_NULL
	DenseChunk* dense; // if non-sparse pointer to DenseChunk
	
	bool is_sparse () {
		return sparse_data != B_NULL;
	}

	// Initial allocation is non-sparse with uninited data for worldgen to fill
	void init () {
		dense = nullptr;
		sparse_data = B_NULL;
		alloc_dense_chunk();
		dense->init_dense();
	}
	void free () {
		if (dense)
			free_dense_chunk();
	}

	void alloc_dense_chunk () {
		assert(dense == nullptr);
		ZoneScopedC(tracy::Color::Crimson);
		dense = (DenseChunk*)malloc(sizeof(DenseChunk));
	}
	void free_dense_chunk () {
		assert(dense != nullptr);
		ZoneScopedC(tracy::Color::Crimson);
		::free(dense);
		dense = nullptr;
	}
	
	// memory use for imgui display
	size_t _alloc_size () {
		size_t sz = dense ? sizeof(DenseChunk) : 0;

	}

	// read block with chunk-relative block pos
	block_id read_block (int x, int y, int z) const {
		assert(x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE);
		
		if (sparse_data != B_NULL)
			return sparse_data; // sparse chunk

		assert(dense);
		return dense->read_block(x,y,z);
	}
	
	void write_block (int x, int y, int z, block_id data) {
		assert(x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE);
		if (sparse_data != B_NULL) {
			if (sparse_data == data)
				return;

			densify();
		}

		assert(dense);

		dense->write_block(x,y,z, data);
	}

	// Realloc from sparse data to dense data (Keeping the data)
	void densify () {
		//clog(INFO, ">> densify");
		ZoneScopedC(tracy::Color::Chocolate);

		alloc_dense_chunk();
		dense->init_sparse(sparse_data);

		sparse_data = B_NULL;
	}
	// Realloc from dense data (after determining that it actually contains sparse data) and setting sparse data to id
	void sparsify (block_id data) {
		//clog(INFO, ">> sparsify");
		ZoneScopedC(tracy::Color::Chocolate);

		free_dense_chunk();
		sparse_data = data;
	}

	// Scan data to see if dense data should be sparsify'ed (run once per frame)
	void checked_sparsify () {
		if (!is_sparse()) return;
		ZoneScopedC(tracy::Color::Chocolate);
		
		if (dense->checked_sparsify()) {
			// chunk was completely sparse
			sparsify(dense->sparse_data[0]);
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

struct ChunkSliceData;

const lrgba DBG_CHUNK_COL			= srgba(0, 0, 255, 255);
const lrgba DBG_SPARSE_CHUNK_COL	= srgba(0, 0, 200, 20);
const lrgba DBG_CULLED_CHUNK_COL	= srgba(255, 0, 0, 180);
const lrgba DBG_DENSE_SUBCHUNK_COL	= srgba(234, 90, 0, 255);

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
		for (auto& s : m.slices) {
			if (s != U16_NULL)
				slices_alloc.free(s);
			s = U16_NULL;
		}
		m.vertex_count = 0;
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

	// for renderer switch
	void renderer_switch () {
		//assert(upload_slices.empty()); // Can have upload_slices here if a renderer did not consume them last frame, but these will simply be overwritten by newer duplicate versions, which is safe
		
		for (chunk_id cid=0; cid<max_id; ++cid) {
			auto& chunk = chunks[cid];
			if ((chunk.flags & Chunk::LOADED) == 0) continue;
			chunk.flags |= Chunk::DIRTY; // remesh chunk to make sure new renderer gets all meshes uploaded again
		}
	}

	// load chunks in this radius in order of distance to the player 
	float load_radius = 700.0f;
	
	// prevent rapid loading and unloading chunks
	// better would be a cache in chunks outside this radius get added (cache size based on desired memory use)
	//  and only the "oldest" chunks should be unloaded
	// This way walking back and forth might not even need to load any chunks at all
	float unload_hyster = CHUNK_SIZE*1.5f;

	int background_queued_count = 0;

	bool mesh_world_border = false;

	bool visualize_chunks = false;
	bool debug_frustrum_culling = false;

	// distance of chunk to player
	int chunk_lod (float dist) {
		return clamp(floori(log2f(dist / load_radius * 16)), 0,3);
	}

	void imgui (Renderer* renderer);

	void visualize_chunk (Chunk& chunk, bool empty, bool culled) {
		lrgba const* col;
		if (debug_frustrum_culling) {
			if (empty) return;
			col = culled ? &DBG_CULLED_CHUNK_COL : &DBG_CHUNK_COL;
		} else if (visualize_chunks) {
			col = chunk.voxels.is_sparse() ? &DBG_SPARSE_CHUNK_COL : &DBG_CHUNK_COL;

			if (!chunk.voxels.is_sparse()) {
				assert(chunk.voxels.dense);

				size_t subc_i = 0;

				for (int sz=0; sz<CHUNK_SIZE; sz += SUBCHUNK_SIZE) {
					for (int sy=0; sy<CHUNK_SIZE; sy += SUBCHUNK_SIZE) {
						for (int sx=0; sx<CHUNK_SIZE; sx += SUBCHUNK_SIZE) {
							auto sparse_bit = chunk.voxels.dense->is_subchunk_sparse(subc_i);

							if (!sparse_bit) {
								float3 pos = (float3)(chunk.pos * CHUNK_SIZE + int3(sx,sy,sz)) + SUBCHUNK_SIZE/2;
								g_debugdraw.wire_cube(pos, (float3)SUBCHUNK_SIZE * 0.997f, DBG_DENSE_SUBCHUNK_COL);
							}

							subc_i++;
						}
					}
				}
			}
		}

		g_debugdraw.wire_cube(((float3)chunk.pos + 0.5f) * CHUNK_SIZE, (float3)CHUNK_SIZE * 0.997f, *col);
	}

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
	block_id query_block (int3 pos, Chunk** out_chunk=nullptr, int3* out_block_pos=nullptr) {
		if (out_chunk)
			*out_chunk = nullptr;

		int bx, by, bz;
		int3 chunk_pos;
		CHUNK_BLOCK_POS(pos.x, pos.y, pos.z, chunk_pos, bx,by,bz);

		if (out_block_pos)
			*out_block_pos = int3(bx, by, bz);

		Chunk* chunk = query_chunk(chunk_pos);
		if (!chunk || (chunk->flags & Chunk::LOADED) == 0)
			return B_NULL;

		if (out_chunk)
			*out_chunk = chunk;
		return chunk->voxels.read_block(bx,by,bz);
	}

	void set_block (int3 pos, block_id b) {
		int bx, by, bz;
		int3 chunk_pos;
		CHUNK_BLOCK_POS(pos.x, pos.y, pos.z, chunk_pos, bx,by,bz);

		Chunk* chunk = query_chunk(chunk_pos);
		assert(chunk);
		if (!chunk)
			return;

		chunk->voxels.write_block(bx, by, bz, b);
	}

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
