#pragma once
#include "common.hpp"
#include "blocks.hpp"
#include "assets.hpp"
#include "player.hpp"

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

#define IDX3D(X,Y,Z, SZ) (size_t)(Z) * (SZ)*(SZ) + (size_t)(Y) * (SZ) + (size_t)(X)
#define IDX3DV(X,Y,Z, SZ) (size_t)(Z) * (SZ).y*(SZ).x + (size_t)(Y) * (SZ).x + (size_t)(X)

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
#define CHECK_BLOCK(b) (assert((b) >= B_NULL && (b) < (block_id)g_assets.block_types.blocks.size()) , b)
//#define CHECK_BLOCK(b) ( ((b) >= B_NULL && (b) < (block_id)g_assets.block_types.blocks.size()) ? b : B_NULL )

struct Chunk {
	enum Flags : uint32_t {
		ALLOCATED		= 1u<<0, // Set when chunk was allocated, exists so that zero-inited memory allocated by BlockAllocator is interpreted as unallocated chunks (so we can simply iterate over the memory while checking flags)
		
		VOXELS_DIRTY	= 1u<<1, // voxels were changed, run checked_sparsify
		REMESH			= 1u<<2, // need remesh due to voxel change, neighbour chunk change, etc.

		LOADED_PHASE2	= 1u<<3, // not set: phase 1

		DIRTY_FACE		= 1u<<3,
		DIRTY_EDGE		= 1u<<4,
		DIRTY_CORNER	= 1u<<5,

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

	int3 dirty_rect_min;
	int3 dirty_rect_max;

	void clear_dirty_rect () {
		dirty_rect_min = INT_MAX;
		dirty_rect_max = INT_MIN;
	}

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
using chunk_pos_map = std::unordered_map<int3, T, ChunkKey_Hasher, ChunkKey_Comparer>;

typedef std::unordered_set<int3, ChunkKey_Hasher, ChunkKey_Comparer> chunk_pos_set;

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

struct ChunkFileData {
	ChunkVoxels voxels;
	SubchunkVoxels subchunks[CHUNK_SUBCHUNK_COUNT];
};
inline std::string get_chunk_filename (int3 const& pos, char const* dirname) {
	return prints("%s/%+3d,%+3d,%+3d.bin", dirname, pos.x, pos.y, pos.z);
}
chunk_id try_load_chunk_from_disk (Chunks& chunks, int3 const& pos, char const* dirname);
void save_chunk_to_disk (Chunks& chunks, chunk_id cid, char const* dirname);

struct VoxelEdits {
	SERIALIZE(VoxelEdits, open, brush_block, brush_mode, brush_size, brush_repeat,
		brush_ray_max_dist, brush_plane, brush_plane_normal)
	
	enum BrushMode : int {
		RAYCAST,
		PLANE,
	};
	
	bool        open = false;

	std::string brush_block = "earth";

	BrushMode   brush_mode = RAYCAST;

	float       brush_size = 1;
	bool        brush_repeat = true;

	//bool        brush_snap = true; // false=smooth sphere from hit point, cant do currently since raycast only returns ints
	
	float       brush_ray_max_dist = 50;

	float3      brush_plane = 0;
	float3      brush_plane_normal = float3(0,0,1);

	void imgui (Input& I) {
		ImGui::Checkbox("Voxel Edits", &open);
		if (I.buttons[KEY_K].went_down)
			open = !open;

		if (open && ImGui::Begin("Voxel Edits", &open)) {

			ImGui::InputText("brush_block", &brush_block);

			ImGui::Combo("brush_mode", (int*)&brush_mode, "RAYCAST\0PLANE");
			
			ImGui::DragFloat("brush_size", &brush_size, 0.05f);
			ImGui::Checkbox("brush_repeat", &brush_repeat);
			//ImGui::Checkbox("brush_snap", &brush_snap);

			ImGui::DragFloat("brush_ray_max_dist", &brush_ray_max_dist, 0.05f);
			ImGui::DragFloat3("brush_plane", &brush_plane.x, 0.05f);
			ImGui::DragFloat3("brush_plane_normal", &brush_plane_normal.x, 0.05f);

			ImGui::End();
		}
	}
	void update (Input& I, Game& game);
};

struct Chunks {
	SERIALIZE(Chunks, load_radius, load_from_disk, unload_hyster, mesh_world_border,
		visualize_chunks, visualize_subchunks, visualize_radius, debug_frustrum_culling,
		edits)

	BlockAllocator<Chunk>			chunks			= { MAX_CHUNKS };
	BlockAllocator<ChunkVoxels>		chunk_voxels	= { MAX_CHUNKS }; // TODO: get rid of alloc bitset here;  always same id as chunk, ie. this is just a SOA array together with chunks
	BlockAllocator<SubchunkVoxels>	subchunks		= { MAX_SUBCHUNKS };

	BlockAllocator<SliceNode>		slices			= { MAX_SLICES };

	chunk_pos_map<chunk_id>			chunks_map;

	chunk_pos_set					queued_chunks; // queued for async worldgen

	BlueNoiseTexture				blue_noise_tex;

	VoxelEdits                      edits;

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

	void sparse_chunk_from_worldgen (chunk_id cid, block_id* raw_voxels);

	void flag_touching_neighbours (Chunk* c);

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

	// load chunks in this radius in order of distance to the player 
	float load_radius = 700.0f;
	bool load_from_disk = false;

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

	void save_chunks_to_disk (const char* save_dirname);

	// instead of sorting the chunks using a exact sorting of a vector ( O(N logN) )
	// simply insert the chunks into a set of buckets ( O(N) )
	// that each span a fixed interval in squared distance to player space
	// using squared distances is easier and has the advantage of having more buckets close to the player
	// this means we still load the chunks in a routhly sorted order but chunks with closeish distances might be out of order
	// by changing the BUCKET_FAC you increase the amount of vectors needed but increase the accuracy of the sort
	std::vector< std::vector<int3> > chunks_to_generate;
	uint32_t pending_chunks = 0; // chunks waiting to be queued

	// queue and finialize chunks that should be generated
	void update_chunk_loading (Game& game);
	
	struct UploadSlice {
		slice_id		sliceid;
		ChunkSliceData*	data;
	};
	std::vector<UploadSlice> upload_slices;

	std::vector<chunk_id> upload_voxels; // VOXELS_DIRTY of this frame

	// queue and finialize chunks that should be generated
	void update_chunk_meshing (Game& game);
	
	//
	void fill_sphere (float3 const& center, float radius, block_id bid);

	bool raycast_breakable_blocks (Ray const& ray, float max_dist, VoxelHit& hit, bool hit_at_max_dist=false);
	
};

inline int _toint (float f) { return *(int*)&f; }
inline float _tofloat (int i) { return *(float*)&i; }

inline int face_from_stepmask (int axis, float3 const& ray_dir) {
	if      (axis == 0) return ray_dir.x >= 0 ? 0 : 1;
	else if (axis == 1) return ray_dir.y >= 0 ? 2 : 3;
	else                return ray_dir.z >= 0 ? 4 : 5;
}
template <typename Func>
void raycast_voxels (Chunks& chunks, Ray const& ray, Func hit_voxel) {
	int3 stepdir;
	stepdir.x = ray.dir.x >= 0 ? 1 : -1;
	stepdir.y = ray.dir.y >= 0 ? 1 : -1;
	stepdir.z = ray.dir.z >= 0 ? 1 : -1;

	float3 inv_dir = 1.0f / abs(ray.dir);

	float3 floored = floor(ray.pos);

	float3 next;
	next.x = inv_dir.x * (ray.dir.x >= 0 ? floored.x+1 - ray.pos.x : ray.pos.x - floored.x);
	next.y = inv_dir.y * (ray.dir.y >= 0 ? floored.y+1 - ray.pos.y : ray.pos.y - floored.y);
	next.z = inv_dir.z * (ray.dir.z >= 0 ? floored.z+1 - ray.pos.z : ray.pos.z - floored.z);

	int3 coord = (int3)floored;

	float dist = 0.0;
	int axis = -1;

	while (!hit_voxel(coord, axis, dist)) {
		dist = min(min(next.x, next.y), next.z);

		// step on axis where exit distance is lowest
		if (next.x == dist) {
			coord.x += stepdir.x;
			next.x += inv_dir.x;
			axis = 0;
		} else if (next.y == dist) {
			coord.y += stepdir.y;
			next.y += inv_dir.y;
			axis = 1;
		} else {
			coord.z += stepdir.z;
			next.z += inv_dir.z;
			axis = 2;
		}
	}
}

void _dev_raycast (Chunks& chunks, Camera_View& view);

struct Test {
	
	struct Cell {
		int dirs[4];
	};

	Cell light    [CHUNK_SIZE][CHUNK_SIZE] = {};
	Cell light_buf[CHUNK_SIZE][CHUNK_SIZE] = {};

	int3 chunk_pos = int3(-1,0,-2);
	int z = 35;

	int max_light = 50; // 128

	bool init = false;

	bool auto_prop = true;

	void update (Game& game);
};

inline Test test;
