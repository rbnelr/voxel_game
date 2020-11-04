#pragma once
#include "common.hpp"
#include "blocks.hpp"
#include "graphics.hpp"

#define CHUNK_SIZE			32
#define CHUNK_SIZE_SHIFT	5 // for pos >> CHUNK_SIZE_SHIFT
#define CHUNK_SIZE_MASK		((1 << CHUNK_SIZE_SHIFT) -1)
#define CHUNK_ROW_OFFS		(CHUNK_SIZE+2)
#define CHUNK_LAYER_OFFS	((CHUNK_SIZE+2)*(CHUNK_SIZE+2))

#define CHUNK_BLOCK_COUNT	(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE)



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
struct Graphics;
struct Chunks;

static inline constexpr int _block_count (int lod_levels) {
	int count = 0;
	for (int lod=0; lod<lod_levels; ++lod) {
		count += (CHUNK_SIZE >> lod) * (CHUNK_SIZE >> lod) * (CHUNK_SIZE >> lod);
	}
	return count;
};

////////////// Chunk

struct ChunkData {
	static constexpr uint64_t COUNT = (CHUNK_SIZE+2) * (CHUNK_SIZE+2) * (CHUNK_SIZE+2);

	block_id	id[COUNT];
	uint8		block_light[COUNT];
	uint8		sky_light[COUNT];
	uint8		hp[COUNT];

	static constexpr uint64_t pos_to_index (int3 pos) {
		return (pos.z + 1) * (CHUNK_SIZE+2) * (CHUNK_SIZE+2) + (pos.y + 1) * (CHUNK_SIZE+2) + (pos.x + 1);
	}

	Block get (int3 pos) {
		assert(all(pos >= -1 && pos <= CHUNK_SIZE+1));

		Block b;

		auto indx = pos_to_index(pos);

		b.id			= id[indx];
		b.block_light	= block_light[indx];
		b.sky_light		= sky_light[indx];
		b.hp			= hp[indx];

		return b;
	}
	void set (int3 pos, Block b) {
		assert(all(pos >= -1 && pos <= CHUNK_SIZE+1));

		auto indx = pos_to_index(pos);

		id[indx]			= b.id;
		block_light[indx]	= b.block_light;
		sky_light[indx]		= b.sky_light;
		hp[indx]			= b.hp;
	}

	void init_border ();
};

struct Chunk {
	enum Flags : uint32_t {
		ALLOCATED = 1, // identify non-allocated chunks in chunk array, default true so that this get's set 
		LOADED = 2, // block data valid and safe to use in main thread
		REMESH = 4, // blocks were changed, need remesh
	};

	const int3 pos;

	Flags flags;

	// block data
	//  with border that stores a copy of the blocks of our neighbour along the faces (edges and corners are invalid)
	//  border gets automatically kept in sync if only set_block() is used to update blocks
	std::unique_ptr<ChunkData> blocks = nullptr;
	
	Chunk (int3 pos): pos{pos} {
		flags = ALLOCATED;
	}
	~Chunk () {
		flags = (Flags)0;
	}

	void init_blocks ();

	// access blocks raw, only use in World Generator since neighbours are not notified of block changed with these!
	void set_block_unchecked (int3 pos, Block b); // only use in World Generator

	// get block
	Block get_block (int3 pos) const;
	// set block (used in light updater)
	void _set_block_no_light_update (Chunks& chunks, int3 pos, Block b);
	// set block (expensive, -> potentially updates light and copies block data to neighbours if block is at the border of a chunk)
	void set_block (Chunks& chunks, int3 pos, Block b);

	void update_neighbour_blocks(Chunks& chunks);
};
ENUM_BITFLAG_OPERATORS_TYPE(Chunk::Flags, uint32_t)

typedef uint16_t chunk_id;
static constexpr uint16_t MAX_CHUNKS = (1<<16) - 2; // -2 to fit max_id in 16 bit int

struct ChunkAllocator {
	Chunk* chunks;
	chunk_id max_id = 0; // max chunk id needed to iterate chunks
	chunk_id count = 0; // number of ALLOCATED chunks

	Chunk& operator[] (chunk_id id) {
		assert(id < max_id);
		return chunks[id];
	}

	BitsetAllocator id_alloc;
	char* commit_ptr; // end of committed chunk memory

	std::unordered_map<int3, chunk_id> pos_to_id;

	ChunkAllocator () {
		chunks = (Chunk*)reserve_address_space(sizeof(Chunk) * MAX_CHUNKS);
		commit_ptr = (char*)chunks;
	}
	~ChunkAllocator () {
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
		if (count >= MAX_CHUNKS)
			throw std::runtime_error("MAX_CHUNKS reached!");

		auto id = (chunk_id)id_alloc.alloc();

		max_id = max(max_id, id+1);
		count++;

		if ((char*)&chunks[id+1] > commit_ptr) { // commit pages one at a time when needed
			commit_pages(commit_ptr, os_page_size);
			memset(commit_ptr, 0, os_page_size); // zero new chunks to init flags
			commit_ptr += os_page_size;
		}

		assert((chunks[id].flags & Chunk::ALLOCATED) == 0);
		new (&chunks[id]) Chunk (pos);

		assert(pos_to_id.find(pos) == pos_to_id.end());
		pos_to_id.emplace(pos, id);

		return id;
	}
	void free_chunk (chunk_id id) {
		pos_to_id.erase(chunks[id].pos);

		chunks[id].~Chunk();

		id_alloc.free(id);
		
		auto last = id_alloc.scan_reverse_allocated();
		while ((char*)&chunks[last+1] <= commit_ptr - os_page_size) { // free pages one by one when needed
			commit_ptr -= os_page_size;
			decommit_pages(commit_ptr, os_page_size);
		}

		max_id = (chunk_id)(last+1);
		count--;
	}
};

inline float chunk_dist_sq (int3 pos, float3 dist_to) {
	int3 chunk_origin = pos * CHUNK_SIZE;
	return point_box_nearest_dist((float3)chunk_origin, CHUNK_SIZE, dist_to);
}

////////////// Chunks
struct Chunks {
	ChunkAllocator chunks;

	int count_culled;

	Chunk& operator[] (chunk_id id) {
		assert(id < chunks.max_id);
		return chunks.chunks[id];
	}

	// load chunks in this radius in order of distance to the player 
	float generation_radius = 250.0f;
	
	// prevent rapid loading and unloading chunks
	// better would be a cache in chunks outside this radius get added (cache size based on desired memory use)
	//  and only the "oldest" chunks should be unloaded
	// This way walking back and forth might not even need to load any chunks at all
	float deletion_hysteresis = CHUNK_SIZE*1.5f;

	float active_radius = 200.0f;

	// distance of chunk to player
	int chunk_lod (float dist) {
		return clamp(floori(log2f(dist / generation_radius * 16)), 0,3);
	}

	void imgui () {
		if (!imgui_push("Chunks")) return;

		ImGui::DragFloat("generation_radius", &generation_radius, 1);
		ImGui::DragFloat("deletion_hysteresis", &deletion_hysteresis, 1);
		ImGui::DragFloat("active_radius", &active_radius, 1);

		uint64_t block_count = chunks.count * (uint64_t)CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
		uint64_t block_mem = chunks.count * sizeof(ChunkData);

		ImGui::Text("Voxel data: %4d chunks %11s blocks (%5.0f MB  %5.0f KB avg / chunk)",
			chunks.count, format_thousands(block_count).c_str(), (float)block_mem/1024/1024, (float)block_mem/1024 / chunks.count);

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
	Chunk* query_chunk (int3 coord) {
		auto it = chunks.pos_to_id.find(coord);
		if (it == chunks.pos_to_id.end())
			return nullptr;
		return &this->operator[](it->second);
	}
	// lookup a block with a world block pos, returns BT_NO_CHUNK for unloaded chunks or BT_OUT_OF_BOUNDS if out of bounds in z
	Block query_block (int3 pos, Chunk** out_chunk=nullptr, int3* out_block_pos=nullptr);

	void set_block (int3 pos, Block& b);

	// queue and finialize chunks that should be generated
	void update_chunk_loading (World const& world, WorldGenerator const& wg, Player const& player);
};
