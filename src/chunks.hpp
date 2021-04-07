#pragma once
#include "common.hpp"
#include "blocks.hpp"
#include "assets.hpp"

#if 1
#define CHUNK_SIZE			64 // size of chunk in blocks per axis
#define CHUNK_SIZE_SHIFT	6 // for pos >> CHUNK_SIZE_SHIFT
#define CHUNK_SIZE_MASK		63
#else
#define CHUNK_SIZE			32 // size of chunk in blocks per axis
#define CHUNK_SIZE_SHIFT	5 // for pos >> CHUNK_SIZE_SHIFT
#define CHUNK_SIZE_MASK		31
#endif

#if 0
#define SUBCHUNK_SIZE		4 // size of subchunk in blocks per axis
#define SUBCHUNK_SHIFT		2
#define SUBCHUNK_MASK		3
#elif 1
#define SUBCHUNK_SIZE		8 // size of subchunk in blocks per axis
#define SUBCHUNK_SHIFT		3
#define SUBCHUNK_MASK		7
#else
#define SUBCHUNK_SIZE		16 // size of subchunk in blocks per axis
#define SUBCHUNK_SHIFT		4
#define SUBCHUNK_MASK		15
#endif

#define SUBCHUNK_COUNT		(CHUNK_SIZE / SUBCHUNK_SIZE) // size of chunk in subchunks per axis

static_assert(SUBCHUNK_COUNT == (CHUNK_SIZE / SUBCHUNK_SIZE), "");
static_assert((1 << SUBCHUNK_SHIFT) == SUBCHUNK_SIZE, "");
static_assert(SUBCHUNK_MASK == SUBCHUNK_SIZE-1, "");

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

static constexpr uint16_t CHUNK_SLICE_LENGTH = 16 * 1024;
static constexpr size_t CHUNK_SLICE_SIZE = CHUNK_SLICE_LENGTH * sizeof(BlockMeshInstance);

static constexpr int3 NEIGHBOURS[6] = {
	int3(-1,0,0), int3(+1,0,0),
	int3(0,-1,0), int3(0,+1,0),
	int3(0,0,-1), int3(0,0,+1),
};

static constexpr int3 FULL_NEIGHBOURS[26] = {
	int3(-1,-1,-1),
	int3( 0,-1,-1),
	int3(+1,-1,-1),
	int3(-1, 0,-1),
	int3( 0, 0,-1),
	int3(+1, 0,-1),
	int3(-1,+1,-1),
	int3( 0,+1,-1),
	int3(+1,+1,-1),

	int3(-1,-1, 0),
	int3( 0,-1, 0),
	int3(+1,-1, 0),
	int3(-1, 0, 0),
	//int3( 0, 0, 0),
	int3(+1, 0, 0),
	int3(-1,+1, 0),
	int3( 0,+1, 0),
	int3(+1,+1, 0),

	int3(-1,-1,+1),
	int3( 0,-1,+1),
	int3(+1,-1,+1),
	int3(-1, 0,+1),
	int3( 0, 0,+1),
	int3(+1, 0,+1),
	int3(-1,+1,+1),
	int3( 0,+1,+1),
	int3(+1,+1,+1),
};

struct World;
struct WorldGenerator;
struct Player;
struct Assets;
struct Chunks;
class Renderer;
struct ChunkSliceData;
struct WorldgenJob;

inline constexpr block_id g_null_chunk[CHUNK_VOXEL_COUNT] = {}; // chunk data filled with B_NULL to optimize meshing with non-loaded neighbours

// linked list in Chunks::slices
struct SliceNode {
	slice_id next;
};

static constexpr uint32_t SUBC_SPARSE_BIT = 0x80000000u;

struct SubchunkVoxels {
	block_id voxels[SUBCHUNK_VOXEL_COUNT];
};
struct ChunkVoxels {
	uint32_t subchunks[CHUNK_SUBCHUNK_COUNT];
};

inline constexpr uint32_t MAX_SUBCHUNKS = (uint32_t)( (32ull *GB) / sizeof(SubchunkVoxels) );

// Use comma operator to assert and return value in expression
#define CHECK_BLOCK(b) (assert((b) > B_NULL && (b) < (block_id)g_assets.block_types.blocks.size()) , b)
//#define CHECK_BLOCK(b) ( ((b) > B_NULL && (b) < (block_id)g_assets.block_types.blocks.size()) ? b : B_NULL )

struct Chunk {
	enum Flags : uint32_t {
		ALLOCATED		= 1u<<0, // Set when chunk was allocated, exists so that zero-inited memory allocated by BlockAllocator is interpreted as unallocated chunks (so we can simply iterate over the memory while checking flags)
		
		VOXELS_DIRTY	= 1u<<1, // voxels were changed, run checked_sparsify
		REMESH			= 1u<<2, // need remesh due to voxel change, neighbour chunk change, etc.

		LOADED_PHASE2	= 1u<<3, // not set: phase 1

		// Flags for if neighbours[i] contains null to skip neighbour loop in iterate chunk loading for performance
		NEIGHBOUR0_NULL = 1u<<26,
		NEIGHBOUR1_NULL = 1u<<27,
		NEIGHBOUR2_NULL = 1u<<28,
		NEIGHBOUR3_NULL = 1u<<29,
		NEIGHBOUR4_NULL = 1u<<30,
		NEIGHBOUR5_NULL = 1u<<31,
	};
	static constexpr Flags NEIGHBOUR_NULL_MASK = (Flags)(0b111111u << 26);

	Flags flags;
	int3 pos;

	chunk_id neighbours[6];
	// make sure there are still at 4 bytes following this so that 16-byte sse loads of neighbours can never segfault

	slice_id opaque_mesh_slices;
	slice_id transp_mesh_slices;

	uint32_t opaque_mesh_vertex_count;
	uint32_t transp_mesh_vertex_count;

	void init_meshes () {
		opaque_mesh_slices = U16_NULL;
		transp_mesh_slices = U16_NULL;
		opaque_mesh_vertex_count = 0;
		transp_mesh_vertex_count = 0;
	}

	void _validate_flags () {
		if ((flags & ALLOCATED) == 0) assert(flags == (Flags)0);
		if (flags & VOXELS_DIRTY) assert(flags & REMESH);

		for (int i=0; i<6; ++i) {
			assert((neighbours[i] == U16_NULL) == ((flags & (NEIGHBOUR0_NULL << i)) != 0));
		}
	}
};
ENUM_BITFLAG_OPERATORS_TYPE(Chunk::Flags, uint32_t)

inline int _slices_count (uint32_t vertex_count) { // just for imgui
	return (vertex_count + CHUNK_SLICE_LENGTH-1) / CHUNK_SLICE_LENGTH;
}

inline constexpr size_t _chunk_sz = sizeof(Chunk); // only for checking value in intellisense

inline lrgba DBG_SPARSE_CHUNK_COL	= srgba(  0, 140,   3,  40);
inline lrgba DBG_CHUNK_COL			= srgba( 45, 255,   0, 255);
inline lrgba DBG_STAGE1_COL			= srgba(128,   0, 255, 255);

inline lrgba DBG_CULLED_CHUNK_COL	= srgba(255,   0,   0, 180);
inline lrgba DBG_DENSE_SUBCHUNK_COL	= srgba(255, 255,   0, 255);
inline lrgba DBG_RADIUS_COL			= srgba(200,   0,   0, 255);
inline lrgba DBG_CHUNK_ARRAY_COL	= srgba(  0, 255,   0, 255);

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

template <typename T>
using chunk_pos_map = std_unordered_map<int3, T, ChunkKey_Hasher, ChunkKey_Comparer>;

typedef std_unordered_set<int3, ChunkKey_Hasher, ChunkKey_Comparer> chunk_pos_set;

// TODO: Maybe a 128x128x16 Texture might have less artefacting than a 64^3 texture because trees are mainly horizontally placed
struct BlueNoiseTexture {
	float* data;

	static constexpr size_t SIZE = 64;

	struct BlueNoiseRawFileHeader {
		uint32_t Version;
		uint32_t nChannel;
		uint32_t nDimension;
		uint32_t Shape[3]; // actually Shape[nDimension], but assume 3 for our case, not sure if zyx or xyz, but is irrelevant in my usecase
		// uint32_t Pixels[Shape[0]][Shape[1]][Shape[2]][nChannel]
	};
	bool load_file (const char* filename) {
		uint64_t size;
		auto file = load_binary_file(filename, &size);
		if (!file) return false;

		if (size < sizeof(BlueNoiseRawFileHeader)) return false;
		auto* header = (BlueNoiseRawFileHeader*)file.get();

		if (header->Version != 1) return false;
		if (header->nChannel != 1) return false;
		if (header->nDimension != 3) return false;
		if (header->Shape[0] != (uint32_t)SIZE) return false;
		if (header->Shape[1] != (uint32_t)SIZE) return false;
		if (header->Shape[2] != (uint32_t)SIZE) return false;
		if (header->Shape[0] != header->Shape[1]) return false;
		if (header->Shape[0] != header->Shape[2]) return false;

		if (size < sizeof(BlueNoiseRawFileHeader) + sizeof(int)*SIZE*SIZE*SIZE) return false;
		uint32_t* data_in = (uint32_t*)(file.get() + sizeof(BlueNoiseRawFileHeader));

		data = (float*)malloc(sizeof(float)*SIZE*SIZE*SIZE);

		static constexpr float FAC = 1.0f / (float)(SIZE*SIZE*SIZE);
		for (size_t i=0; i<SIZE*SIZE*SIZE; ++i) {
			data[i] = (float)data_in[i] * FAC;
		}

		return true;
	}

	BlueNoiseTexture () {
		if (!load_file("textures/bluenoise64.raw"))
			throw std::runtime_error("textures/bluenoise64.raw could not be loaded!");
	}
	~BlueNoiseTexture () {
		free(data);
	}

	float sample (int x, int y, int z) {
		x &= (int)SIZE-1;
		y &= (int)SIZE-1;
		z &= (int)SIZE-1;
		return data[z * SIZE*SIZE + y * SIZE + x];
	}
};

struct Chunks {

	BlockAllocator<Chunk>			chunks			= { MAX_CHUNKS };
	BlockAllocator<ChunkVoxels>		chunk_voxels	= { MAX_CHUNKS }; // TODO: get rid of alloc bitset here;  always same id as chunk, ie. this is just a SOA array together with chunks
	BlockAllocator<SubchunkVoxels>	subchunks		= { MAX_SUBCHUNKS };

	BlockAllocator<SliceNode>		slices			= { MAX_SLICES };

	chunk_pos_map<chunk_id>			chunks_map;

	chunk_pos_set					queued_chunks; // queued for async worldgen

	BlueNoiseTexture				blue_noise_tex;

	chunk_id query_chunk (int3 const& pos) {
		//ZoneScoped;
		auto it = chunks_map.find(pos);
		return it != chunks_map.end() ? it->second : U16_NULL;
	}

	void destroy ();
	~Chunks () {
		destroy();
	}

	void free_slices (slice_id sid) {
		while (sid != U16_NULL) {
			auto next = slices[sid].next;
			slices.free(sid);
			sid = next;
		}
	}

	void free_voxels (chunk_id cid, Chunk& chunk);

	void densify_subchunk (ChunkVoxels& vox, uint32_t& subc);

	void checked_sparsify_chunk (chunk_id cid);
	bool checked_sparsify_subchunk (ChunkVoxels& vox, uint32_t& subc);

	void sparse_chunk_from_worldgen (chunk_id cid, Chunk& chunk, block_id* raw_voxels);

	Chunk& operator[] (chunk_id id) {
		return chunks[id];
	}
	// End of array of chunks for iteration (not all are allocated, check flags)
	chunk_id end () {
		return (chunk_id)chunks.slots.alloc_end;
	}

	chunk_id alloc_chunk (int3 pos);
	void free_chunk (chunk_id cid);

	// for renderer switch
	void renderer_switch () {
		//assert(upload_slices.empty()); // Can have upload_slices here if a renderer did not consume them last frame, but these will simply be overwritten by newer duplicate versions, which is safe
		
		for (chunk_id cid=0; cid<end(); ++cid) {
			auto& chunk = chunks[cid];
			if (chunk.flags == 0) continue;
			chunks[cid].flags |= Chunk::REMESH; // remesh chunk to make sure new renderer gets all meshes uploaded again
		}
	}

	SERIALIZE(Chunks, load_radius, unload_hyster, mesh_world_border, visualize_chunks, visualize_subchunks, visualize_radius, debug_frustrum_culling)

	// load chunks in this radius in order of distance to the player 
	float load_radius = 700.0f;
	
	// prevent rapid loading and unloading chunks
	// This way walking back and forth small distances does not cause load on the system
	float unload_hyster = 40;

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

	void visualize_chunk (chunk_id cid, Chunk& chunk, bool empty, bool culled);

	// read a block with a world block pos, returns B_NULL for unloaded chunks
	block_id read_block (int x, int y, int z);
	// read a block with a chunk block pos
	block_id read_block (int x, int y, int z, chunk_id cid);

	// 
	void write_block (int x, int y, int z, block_id bid);
	//
	void write_block (int x, int y, int z, chunk_id cid, block_id bid);

	void write_block_update_chunk_flags (int x, int y, int z, Chunk* c);

	// instead of sorting the chunks using a exact sorting of a vector ( O(N logN) )
	// simply insert the chunks into a set of buckets ( O(N) )
	// that each span a fixed interval in squared distance to player space
	// using squared distances is easier and has the advantage of having more buckets close to the player
	// this means we still load the chunks in a routhly sorted order but chunks with closeish distances might be out of order
	// by changing the BUCKET_FAC you increase the amount of vectors needed but increase the accuracy of the sort
	struct GenChunk {
		int3 pos;
	};
	std_vector< std_vector<GenChunk> > chunks_to_generate;
	uint32_t pending_chunks = 0; // chunks waiting to be queued

	// queue and finialize chunks that should be generated
	void update_chunk_loading (Game& game);
	
	struct UploadSlice {
		slice_id		sliceid;
		ChunkSliceData*	data;
	};
	std_vector<UploadSlice> upload_slices;

	std_vector<chunk_id> upload_voxels; // VOXELS_DIRTY of this frame

	// queue and finialize chunks that should be generated
	void update_chunk_meshing (Game& game);

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

void _dev_raycast (Chunks& chunks, Camera_View& view);
