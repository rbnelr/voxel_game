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
#define CHUNK_BLOCK_POS(X,Y,Z, CX, CY, CZ, BX, BY, BZ) do { \
		CX = (X) >> CHUNK_SIZE_SHIFT; \
		CY = (Y) >> CHUNK_SIZE_SHIFT; \
		CZ = (Z) >> CHUNK_SIZE_SHIFT; \
		BX = (X) & CHUNK_SIZE_MASK; \
		BY = (Y) & CHUNK_SIZE_MASK; \
		BZ = (Z) & CHUNK_SIZE_MASK; \
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

static constexpr int3 NEIGHBOURS[6] = {
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
		//LOADED			= 1<<1, // block data valid and safe to use in main thread
		SPARSE_VOXELS	= 1<<2, // voxel_data is a single block id instead of a dense_chunk id
		VOXELS_DIRTY	= 1<<3, // voxels were changed, run checked_sparsify
		REMESH			= 1<<4, // need remesh due to voxel change, neighbour chunk change, etc.
	};

	Flags flags;
	
	int3 pos;

	uint16_t voxel_data; // if SPARSE_VOXELS: non-null block id   if !SPARSE_VOXELS: id to dense_chunks

	ChunkMesh opaque_mesh;
	ChunkMesh transparent_mesh;

	void _validate_flags () {
		if ((flags & ALLOCATED) == 0) assert(flags == (Flags)0);
		if (flags & VOXELS_DIRTY) assert(flags & REMESH);
	}
};
ENUM_BITFLAG_OPERATORS_TYPE(Chunk::Flags, uint32_t)

inline float chunk_dist_sq (int3 const& pos, float3 const& dist_to) {
	int3 chunk_origin = pos * CHUNK_SIZE;
	return point_box_nearest_dist_sqr((float3)chunk_origin, CHUNK_SIZE, dist_to);
}

inline lrgba DBG_CHUNK_COL			= srgba(  0,   0, 255, 255);
inline lrgba DBG_QUEUED_CHUNK_COL	= srgba(  0, 255, 200, 255);
inline lrgba DBG_SPARSE_CHUNK_COL	= srgba( 60,  60,  60,  45);
inline lrgba DBG_CULLED_CHUNK_COL	= srgba(255,   0,   0, 180);
inline lrgba DBG_DENSE_SUBCHUNK_COL	= srgba(255, 255,   0, 255);
inline lrgba DBG_RADIUS_COL			= srgba(200,   0,   0, 255);
inline lrgba DBG_CHUNK_ARRAY_COL	= srgba(  0, 255,   0, 255);

// 3d array to store chunks around the player that moves with the player by not actually moving entries
// but instead simply moving a virtual origin point (offs) that is used when indexing
template <typename T>
struct ScrollingArray {
	T*			arr = nullptr;
	int			size; // allocated size of 3d array (same for each axis, always power of two for access speed; bit masking instead of mod)
	int3		pos; // pos of lower corner of 3d array in the world (in chunk coords)

	~ScrollingArray () {
		free(arr);
	}

	template <typename FREE>
	void update (float required_radius, float3 new_center, FREE free_chunk) {
		int new_size = ceili(max(required_radius, 0.0f) * 2 + 1);
		new_size = (int)round_up_pot((uint32_t)new_size);

		int3 new_pos = roundi(new_center) - new_size / 2;

		if (!arr || new_size != size) {
			// size changed or init, alloc new array
			T* new_arr = (T*)malloc(sizeof(T) * new_size*new_size*new_size);

			// clear array to U16_NULL
			memset(new_arr, -1, sizeof(T) * new_size*new_size*new_size);

			// copy chunks from old array into resized array
			if (arr) {
				size_t i=0;
				for (int z=pos.z; z < pos.z + size; ++z) {
					for (int y=pos.y; y < pos.y + size; ++y) {
						for (int x=pos.x; x < pos.x + size; ++x) {
							chunk_id& c = get(x,y,z);

							if (in_bounds(x,y,z, new_pos, new_size)) {
								// old chunk in new window of chunks, transfer
								auto idx = index(x,y,z, new_pos, new_size);

								assert(new_arr[idx] == U16_NULL);
								new_arr[idx] = c;
							} else {
								// old chunk outside of window of chunks, delete
								if (c != U16_NULL)
									free_chunk(c);
							}
						}
					}
				}

				free(arr);
			}

			arr = new_arr;
			size = new_size;
			pos = new_pos;

		} else if (new_pos != pos) {
			// only position changed

			// free all chunks that are outside of new 3d array now
			// with double iteration for y and z
			int3 dsize = min(abs(new_pos - pos), size);
			int3 csize = size - dsize;

			int3 dpos = select(new_pos >= pos, pos, pos + csize);

			for (int z=pos.z; z < pos.z + size; ++z) {
			for (int y=pos.y; y < pos.y + size; ++y) {
			for (int x=dpos.x; x < dpos.x + dsize.x; ++x) {
				chunk_id& c = get(x,y,z);
				if (c != U16_NULL) { free_chunk(c); c = U16_NULL; }
			}}}

			for (int z=pos.z; z < pos.z + size; ++z) {
			for (int y=dpos.y; y < dpos.y + dsize.y; ++y) {
			for (int x=pos.x; x < pos.x + size; ++x) {
				chunk_id& c = get(x,y,z);
				if (c != U16_NULL) { free_chunk(c); c = U16_NULL; }
			}}}

			for (int z=dpos.z; z < dpos.z + dsize.z; ++z) {
			for (int y=pos.y; y < pos.y + size; ++y) {
			for (int x=pos.x; x < pos.x + size; ++x) {
				chunk_id& c = get(x,y,z);
				if (c != U16_NULL) { free_chunk(c); c = U16_NULL; }
			}}}

			pos = new_pos;
		}
	}

	static bool in_bounds (int x, int y, int z, int3 const& pos, int size) {
		return z >= pos.z && z < pos.z + size &&
		       y >= pos.y && y < pos.y + size &&
		       x >= pos.x && x < pos.x + size;
	}
	static size_t index (int x, int y, int z, int3 const& pos, int size) {
		assert(in_bounds(x,y,z, pos, size));
		uint32_t mask = size -1;
		uint32_t mx = (uint32_t)x & mask;
		uint32_t my = (uint32_t)y & mask;
		uint32_t mz = (uint32_t)z & mask;
		return IDX3D(mx,my,mz, size);
	}

	bool in_bounds (int x, int y, int z) {
		return in_bounds(x,y,z, pos, size);
	}

	T& get (int x, int y, int z) {
		return arr[index(x,y,z, pos, size)];
	}

	T checked_get (int x, int y, int z) {
		return in_bounds(x,y,z) ? get(x,y,z) : U16_NULL;
	}
};

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
typedef std_unordered_set<int3, ChunkKey_Hasher, ChunkKey_Comparer> chunk_pos_set;

struct Chunks {
	ScrollingArray<chunk_id>		chunks_arr;

	BlockAllocator<Chunk>			chunks			= { MAX_CHUNKS };
	BlockAllocator<ChunkVoxels>		dense_chunks	= { MAX_CHUNKS };
	BlockAllocator<SubchunkVoxels>	dense_subchunks	= { MAX_SUBCHUNKS };

	AllocatorBitset					slices_alloc;

	chunk_pos_set					queued_chunks; // queued for async worldgen

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
			if (chunk.flags == 0) continue;
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

	bool mesh_world_border = false;

	bool visualize_chunks = false;
	bool visualize_subchunks = false;
	bool visualize_radius = true;
	bool debug_frustrum_culling = false;

	// distance of chunk to player
	int chunk_lod (float dist) {
		return clamp(floori(log2f(dist / load_radius * 16)), 0,3);
	}

	void imgui (Renderer* renderer);

	void visualize_chunk (Chunk& chunk, bool empty, bool culled);

	// read a block with a world block pos, returns B_NULL for unloaded chunks
	block_id read_block (int x, int y, int z);
	// read a block with a chunk block pos
	block_id read_block (int x, int y, int z, Chunk const* c);

	// 
	void write_block (int x, int y, int z, block_id bid);
	//
	void write_block (int x, int y, int z, Chunk* c, block_id bid);

	void write_block_update_chunk_flags (int x, int y, int z, Chunk* c);

	// instead of sorting the chunks using a exact sorting of a vector ( O(N logN) )
	// simply insert the chunks into a set of buckets ( O(N) )
	// that each span a fixed interval in squared distance to player space
	// using squared distances is easier and has the advantage of having more buckets close to the player
	// this means we still load the chunks in a routhly sorted order but chunks with closeish distances might be out of order
	// by changing the BUCKET_FAC you increase the amount of vectors needed but increase the accuracy of the sort
	static constexpr float BUCKET_FAC = 1.0f / (CHUNK_SIZE*CHUNK_SIZE * 4);
	// check all chunk positions within a square of chunk_generation_radius
	std_vector< std_vector<int3> > chunks_to_generate;

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

template <typename Func>
void raycast_voxels (Chunks& chunks, Ray const& ray, float max_dist, Func hit_voxel) {
	// get direction of each axis of ray_dir as integer (-1, 0, +1)
	// normalize(float) is just float / abs(float)
	int3 step_delta;
	step_delta.x = (int)normalize(ray.dir.x);
	step_delta.y = (int)normalize(ray.dir.y);
	step_delta.z = (int)normalize(ray.dir.z);

	// get how far you have to travel along the ray to move by 1 unit in each axis
	// (ray_dir / abs(ray_dir.x) normalizes the ray_dir so that its x is 1 or -1
	// a zero in ray_dir produces a NaN in step because 0 / 0
	float3 step_dist;
	step_dist.x = length(ray.dir / abs(ray.dir.x));
	step_dist.y = length(ray.dir / abs(ray.dir.y));
	step_dist.z = length(ray.dir / abs(ray.dir.z));

	// get initial positon in block and intial voxel coord
	float ray_pos_floorx = floor(ray.pos.x);
	float ray_pos_floory = floor(ray.pos.y);
	float ray_pos_floorz = floor(ray.pos.z);

	float pos_in_blockx = ray.pos.x - ray_pos_floorx;
	float pos_in_blocky = ray.pos.y - ray_pos_floory;
	float pos_in_blockz = ray.pos.z - ray_pos_floorz;

	int3 cur_voxel;
	cur_voxel.x = (int)ray_pos_floorx;
	cur_voxel.y = (int)ray_pos_floory;
	cur_voxel.z = (int)ray_pos_floorz;

	// how far to step along ray to step into the next voxel for each axis
	float3 next;
	next.x = step_dist.x * (ray.dir.x > 0 ? 1 -pos_in_blockx : pos_in_blockx);
	next.y = step_dist.y * (ray.dir.y > 0 ? 1 -pos_in_blocky : pos_in_blocky);
	next.z = step_dist.z * (ray.dir.z > 0 ? 1 -pos_in_blockz : pos_in_blockz);

	// NaN -> Inf
	next.x = ray.dir.x != 0 ? next.x : INF;
	next.y = ray.dir.y != 0 ? next.y : INF;
	next.z = ray.dir.z != 0 ? next.z : INF;

	auto find_next_axis = [] (float3 const& next) { // index of smallest component
		if (		next.x < next.y && next.x < next.z )	return 0;
		else if (	next.y < next.z )						return 1;
		else												return 2;
	};

	// find the axis of the next voxel step
	int   cur_axis = find_next_axis(next);
	float cur_dist = next[cur_axis];

	auto get_step_face = [&] () {
		return cur_axis*2 +(step_delta[cur_axis] < 0 ? 1 : 0);
	};

	while (!hit_voxel(cur_voxel, get_step_face(), cur_dist)) {

		// find the axis of the cur step
		cur_axis = find_next_axis(next);
		cur_dist = next[cur_axis];

		if (cur_dist > max_dist)
			return; // stop stepping because max_dist is reached

		// clac the distance at which the next voxel step for this axis happens
		next[cur_axis] += step_dist[cur_axis];
		// step into the next voxel
		cur_voxel[cur_axis] += step_delta[cur_axis];
	}
}
