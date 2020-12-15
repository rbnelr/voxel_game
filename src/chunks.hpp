#pragma once
#include "common.hpp"
#include "blocks.hpp"

#define CHUNK_SIZE			64
#define CHUNK_SIZE_SHIFT	6 // for pos >> CHUNK_SIZE_SHIFT
#define CHUNK_SIZE_MASK		63
#define CHUNK_ROW_OFFS		CHUNK_SIZE
#define CHUNK_LAYER_OFFS	(CHUNK_SIZE * CHUNK_SIZE)

#define CHUNK_BLOCK_COUNT	(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE)

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

static inline constexpr int _block_count (int lod_levels) {
	int count = 0;
	for (int lod=0; lod<lod_levels; ++lod) {
		count += (CHUNK_SIZE >> lod) * (CHUNK_SIZE >> lod) * (CHUNK_SIZE >> lod);
	}
	return count;
};

struct ChunkData {
	block_id ids[CHUNK_BLOCK_COUNT];

	static constexpr size_t pos_to_index (int3 pos) {
		return (size_t)pos.z * CHUNK_SIZE * CHUNK_SIZE + (size_t)pos.y * CHUNK_SIZE + (size_t)pos.x;
	}
};

inline constexpr uint16_t U16_NULL = (uint16_t)-1;
typedef uint16_t slice_id;

typedef uint16_t chunk_id;
static constexpr uint16_t MAX_CHUNKS = (1<<16) - 1;
static constexpr int MAX_SLICES = 32;

struct Chunk {
	enum Flags : uint32_t {
		ALLOCATED = 1, // identify non-allocated chunks in chunk array, default true so that this get's set 
		LOADED = 2, // block data valid and safe to use in main thread
		REMESH = 4, // blocks were changed, need remesh
	};

	const int3 pos;

	Flags flags;

	chunk_id neighbours[6];

	slice_id opaque_slices;
	slice_id transparent_slices;

	ChunkData* blocks;
	
	Chunk (int3 pos): pos{pos} {
		flags = ALLOCATED;
		memset(neighbours, -1, sizeof(neighbours));

		opaque_slices = U16_NULL;
		transparent_slices = U16_NULL;
	}
	~Chunk () {
		flags = (Flags)0;
	}

	void _validate_flags () {
		if (flags & LOADED) assert(flags & ALLOCATED);
		if (flags & REMESH) assert(flags & LOADED);
	}

	void set_block (int3 pos, block_id b);
	block_id get_block (int3 pos) const;
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

struct ChunkRendererSlice {
	// data is implicitly placed in allocs based on the slice id
	uint16_t		vertex_count;
	slice_id		next;
};
struct Chunks {
	Chunk* chunks;
	uint32_t max_id = 0; // max chunk id needed to iterate chunks
	uint32_t count = 0; // number of ALLOCATED chunks

	char* commit_ptr; // end of committed chunk memory
	AllocatorBitset id_alloc;

	AllocatorBitset slices_alloc;

	std_vector<ChunkRendererSlice>	slices;

	slice_id alloc_slice () {
		slice_id id = slices_alloc.alloc();

		if (id >= slices.size())
			slices.resize((size_t)id+1);

		slices[id].next = U16_NULL;
		return id;
	}
	void free_slices (slice_id id) {
		while (id != U16_NULL) {
			slices_alloc.free(id);
			id = slices[id].next;
		}

		slices.resize(slices_alloc.alloc_end);
	}
	int _count_slices (slice_id id) {
		int count = 0;
		while (id != U16_NULL) {
			count++;
			id = slices[id].next;
		}
		return count;
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

	PROFILE_NOINLINE chunk_id alloc_chunk (int3 pos) {
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

		assert((chunks[id].flags & Chunk::ALLOCATED) == 0);
		{
			ZoneScopedN("new (&chunks[id]) Chunk (pos)");
			new (&chunks[id]) Chunk (pos);
		}

		assert(pos_to_id.find(pos) == pos_to_id.end());
		{
			ZoneScopedN("pos_to_id.emplace");
			pos_to_id.emplace(pos, id);
		}

		for (int i=0; i<6; ++i) {
			ZoneScopedN("alloc_chunk::get neighbour");

			chunks[id].neighbours[i] = query_chunk_id(pos + OFFSETS[i]);
			if (chunks[id].neighbours[i] != U16_NULL) {
				assert(chunks[ chunks[id].neighbours[i] ].neighbours[i^1] == U16_NULL);
				chunks[ chunks[id].neighbours[i] ].neighbours[i^1] = id;
			}
		}

		{
			ZoneScopedNC("malloc ChunkData", tracy::Color::Crimson);
			chunks[id].blocks = (ChunkData*)malloc(sizeof(ChunkData));
		}

		return id;
	}
	PROFILE_NOINLINE void free_chunk (chunk_id id) {
		ZoneScoped;

		if (&chunks[id] == _query_cache)
			_query_cache = nullptr; // invalidate cache

		for (int i=0; i<6; ++i) {
			auto* n = query_chunk(chunks[id].pos + OFFSETS[i]);
			if (n) {
				assert(n->neighbours[i^1] == id); // i^1 flips direction
				n->neighbours[i^1] = U16_NULL;
			}
		}

		pos_to_id.erase(chunks[id].pos);

		id_alloc.free(id);

		free_slices(chunks[id].opaque_slices);
		free_slices(chunks[id].transparent_slices);

		{
			ZoneScopedNC("free ChunkData", tracy::Color::Crimson);
			free(chunks[id].blocks);
		}
		chunks[id].~Chunk();

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

	void imgui (std::function<void()> chunk_renderer) {
		if (!imgui_push("Chunks")) return;

		ImGui::DragFloat("load_radius", &load_radius, 1);
		ImGui::DragFloat("unload_hyster", &unload_hyster, 1);

		uint64_t block_count = count * (uint64_t)CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
		uint64_t block_mem = count * sizeof(ChunkData);

		ImGui::Text("Voxel data: %4d chunks %11s blocks %5.0f MB RAM",
			count, format_thousands(block_count).c_str(), (float)block_mem/1024/1024);

		if (ImGui::TreeNode("chunks")) {
			for (chunk_id id=0; id<max_id; ++id) {
				if ((chunks[id].flags & Chunk::ALLOCATED) == 0)
					ImGui::Text("[%5d] <not allocated>", id);
				else
					ImGui::Text("[%5d] %+4d,%+4d,%+4d - %2d, %2d slices", id, chunks[id].pos.x,chunks[id].pos.y,chunks[id].pos.z,
						_count_slices(chunks[id].opaque_slices), _count_slices(chunks[id].transparent_slices));
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("chunks alloc")) {
			print_bitset_allocator(id_alloc, sizeof(Chunk), os_page_size);
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("count_hash_collisions")) {
			size_t collisions = 0, max_bucket_size = 0, empty_buckets = 0;
			for (size_t i=0; i<pos_to_id.bucket_count(); ++i) {
				size_t c = pos_to_id.bucket_size(i);
				if (c > 1) collisions += c - 1;
				if (c == 0) empty_buckets++;
				max_bucket_size = std::max(max_bucket_size, c);
			}

			ImGui::Text("chunks: %5d  collisions: %d (buckets: %5d, max_bucket_size: %5d, empty_buckets: %5d)",
				count, collisions, pos_to_id.bucket_count(), max_bucket_size, empty_buckets);

			if (ImGui::TreeNode("bucket counts")) {
				for (size_t i=0; i<pos_to_id.bucket_count(); ++i)
					ImGui::Text("[%5d] %5d", i, pos_to_id.bucket_size(i));
				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

		chunk_renderer();

		imgui_pop();
	}

	Chunk* _query_cache = nullptr;

	// lookup a chunk with a chunk coord, returns nullptr chunk not loaded
	chunk_id query_chunk_id (int3 coord) {
		if (_query_cache && _query_cache->pos == coord)
			return (chunk_id)(_query_cache - chunks);

		auto it = pos_to_id.find(coord);
		if (it == pos_to_id.end())
			return U16_NULL;

		_query_cache = &chunks[it->second];
		return it->second;
	}
	Chunk* query_chunk (int3 coord) {
		if (_query_cache && _query_cache->pos == coord)
			return _query_cache;
		
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
	void update_chunk_loading (World const& world, WorldGenerator const& wg, Player const& player);
};
