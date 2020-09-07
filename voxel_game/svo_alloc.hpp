#pragma once
#include "stdafx.hpp"
#include "util/virtual_allocator.hpp"

static constexpr int CHUNK_SIZE = 64;
static constexpr int CHUNK_SCALE = 6;

static constexpr uint32_t MAX_CHUNKS = 1 << 16;

namespace svo {
	static constexpr uint32_t MAX_NODES = 1 << 16;

	static constexpr int MAX_DEPTH = 20;

	typedef uint32_t Voxel;

	enum VoxelType : uint32_t { // 2 bit
		NODE_PTR		=0, // uint32_t node index into current chunk
		BLOCK_ID		=1, // block_id leaf node
		CHUNK_PTR		=2, // uint32_t chunk index
	};
	ENUM_BITFLAG_OPERATORS_TYPE(VoxelType, uint32_t)

	static constexpr uint16_t ONLY_BLOCK_IDS = 0x5555u;

	struct Node {
		uint16_t children_types;
		uint16_t _pad[15];
		Voxel children[8];

		Voxel& get_child (int child_idx, VoxelType* out_type) {
			*out_type = VoxelType((children_types >> child_idx*2) & 3);
			return children[child_idx];
		}
		void set_child (int child_idx, VoxelType type, Voxel vox) {
			children_types &= ~(3 << child_idx*2);
			children_types |= type << child_idx*2;
			children[child_idx] = vox;
		}
	};

	struct SVOAllocator;

	struct StackNode {
		Node* node; // parent node
		int3 pos;
		int child_indx;
	};

	enum ChunkFlags : uint8_t {
		LOCKED = 1, // read only, still displayed but not allowed to be modified
		GPU_DIRTY = 2,
		MESHING_DIRTY = 4,
	};
	ENUM_BITFLAG_OPERATORS_TYPE(ChunkFlags, uint8_t)

	struct Chunk {
		Node* nodes;

		int3	pos;
		uint8_t	scale;

		ChunkFlags flags;

		uint32_t alloc_ptr;
		uint32_t dead_count; // TODO: cant actually keep track of nodes that disappear because of octree_writes with scale != 0, so maybe just keep a write counter that then occasionally triggers compactings
		uint32_t commit_ptr;
		uint32_t gpu_commit_ptr;

		char _pad[24];

		Chunk (int3	pos, uint8_t scale): pos{pos}, scale{scale} {
			nodes = nullptr;

			alloc_ptr = 0;
			dead_count = 0;
			commit_ptr = 0;
			gpu_commit_ptr = 0;

			flags = GPU_DIRTY | MESHING_DIRTY;
		}

		inline void realloc_nodes (SVOAllocator& alloc);
		inline uint32_t alloc_node (SVOAllocator& alloc);
	};
	static constexpr auto _sz = sizeof(Chunk);

	typedef vector_key<int4> ChunkKey; // xyz: pos, w: scale

	template <typename VALT>
	struct ChunkHashmap {
		std::unordered_map<ChunkKey, VALT> chunks;

		bool get (int3 pos, int scale, VALT* val) {
			auto it = chunks.find(int4(pos, scale));
			if (it != chunks.end()) {
				*val = it->second;
				return true;
			} else {
				return false;
			}
		}
		VALT* get (int3 pos, int scale) {
			auto it = chunks.find(int4(pos, scale));
			if (it != chunks.end()) {
				return &it->second;
			} else {
				return nullptr;
			}
		}
		bool contains (int3 pos, int scale) {
			VALT val;
			return get(pos, scale, &val);
		}
		// insert new, error if already exist 
		void insert (int3 pos, int scale, VALT chunk) {
			assert(!contains(pos, scale));
			chunks.emplace(int4(pos, scale), chunk);
		}
		// remove, return old if did exist
		VALT remove (int3 pos, int scale) {
			auto it = chunks.find(int4(pos, scale));
			if (it != chunks.end()) {
				VALT val = it->second;
				chunks.erase(it);
				return val;
			} else {
				return VALT();
			}
		}

		int count () {
			return (int)chunks.size();
		}

		struct Iterator {
			typename decltype(chunks)::iterator it;

			Chunk* operator* () { return it->second; }
			Iterator& operator++ () {
				++it;
				return *this;
			}
			bool operator!= (Iterator const& r) { return it != r.it; }
			bool operator== (Iterator const& r) { return it != r.it; }
		};
		Iterator begin () {
			Iterator it = { chunks.begin() };
			return it;
		}
		Iterator end () {
			return { chunks.end() };
		}
	};

	struct SVO;

	struct glSparseBuffer {
		int gpu_page_size = 0;
		GLuint ssbo = 0;
		GLuint64EXT ssbo_ptr = 0;

		glSparseBuffer ();
		~glSparseBuffer () {
			if (ssbo != 0)
				glDeleteBuffers(1, &ssbo);
		}

		void upload_changes (SVO& svo);
	};

	struct SVOAllocator {

		static_assert(is_pot(sizeof(Chunk)), "sizeof(Chunk) needs to be power of two!");
		
		// Memory layout, entire memory is reserved, used parts are comitted
		// error C2148: total size of array must not exceed 0x7fffffff bytes, MSVC bug/issue
		struct Memory {
			Chunk chunks[MAX_CHUNKS]; // sparse based on free_chunks
			Node nodes[100/*workaround error C2148 MAX_CHUNKS*/][MAX_NODES]; // sparse based on commit_ptr in Chunk
		};
		static constexpr uintptr_t RESERVE_SIZE = sizeof(Chunk) * MAX_CHUNKS + sizeof(Node) * MAX_CHUNKS * MAX_NODES; //sizeof(Memory);

		Memory*					mem;
		int						chunk_count = 0;
		
		// size of committed regions of pages for chunk structs
		int						commit_ptr = 0;

		Bitset					free_chunks;

		glSparseBuffer			gpu_nodes;

		SVOAllocator () {
			mem = (Memory*)reserve_address_space(RESERVE_SIZE);
		}

		~SVOAllocator () {
			release_address_space(mem, RESERVE_SIZE);
		}

		// Allocate first possible slot
		Chunk* alloc_chunk () {
			assert(chunk_count < MAX_CHUNKS);
			if (chunk_count >= MAX_CHUNKS)
				throw new std::runtime_error("Octree chunk allocation overflow!");

			int idx = free_chunks.clear_first_1();
			chunk_count++;

			Chunk* chunk = &mem->chunks[idx];

			// commit page if all page slots were free before
			if (idx >= commit_ptr) {
				commit_pages(mem->chunks + commit_ptr, os_page_size);
				commit_ptr += os_page_size / (int)sizeof(Chunk);

			#if DBG_MEMSET
				memset(chunk, DBG_MEMSET_VAL, os_page_size);
			#endif
			}

			TracyAlloc(chunk, sizeof(Chunk));
			return chunk;
		}

		// Free random slot
		void free_chunk (Chunk* chunk) {
			free_chunk_nodes(chunk);

			int idx = (int)indexof(chunk);

			free_chunks.set_bit(idx);
			chunk_count--;

			int last_alloc = free_chunks.bitscan_reverse_0(); // -1 on all free
			
			// decommit last page(s)
			while (last_alloc < commit_ptr - os_page_size / (int)sizeof(Chunk)) {
				commit_ptr -= os_page_size / (int)sizeof(Chunk);
				decommit_pages(mem->chunks + commit_ptr, os_page_size);
			}
			
			TracyFree(chunk);
		}

		void free_chunk_nodes (Chunk* chunk) {
			if (chunk->nodes)
				decommit_pages(chunk->nodes, sizeof(Node)*MAX_NODES);

			chunk->nodes = nullptr;
			chunk->alloc_ptr = 0;
			chunk->commit_ptr = 0;
			chunk->dead_count = 0;
		}

		uint32_t size () {
			return chunk_count;
		}
		uint32_t indexof (Chunk* ptr) const {
			return (uint32_t)(ptr - mem->chunks);
		}
		Chunk* operator[] (uint32_t index) const {
			return &mem->chunks[index];
		}

		void imgui_print_free_slots () {
			for (int i=0; i<free_chunks.bits.size(); ++i) {
				char bits[64+1];
				bits[64] = '\0';
				for (int j=0; j<64; ++j)
					bits[j] = (free_chunks.bits[i] & (1ull << j)) ? '_':'#';
				ImGui::Text(bits);
			}
		}
	};

	inline void Chunk::realloc_nodes (SVOAllocator& alloc) {
		assert(alloc_ptr >= commit_ptr);

		if (nodes == nullptr) {
			nodes = alloc.mem->nodes[alloc.indexof(this)];

			uint32_t commit_block = os_page_size / sizeof(Node);
			commit_ptr = commit_block;
			commit_pages(nodes, os_page_size);

			TracyAlloc(nodes, os_page_size);

		#if DBG_MEMSET
			memset(nodes, DBG_MEMSET_VAL, os_page_size);
		#endif
		} else {
			if (commit_ptr < MAX_NODES) {
				uint32_t commit_block = os_page_size / sizeof(Node);

				commit_pages(nodes + commit_ptr, os_page_size);

				// free & alloc with tracy to keep all allocs at the same address instead of many tiny allocs
				TracyFree(nodes);
				TracyAlloc(nodes, commit_ptr * sizeof(Node) + os_page_size);

			#if DBG_MEMSET
				memset(nodes + commit_ptr, DBG_MEMSET_VAL, os_page_size);
			#endif
				commit_ptr += commit_block;

			} else {
				assert(false);
				throw std::runtime_error("Octree node allocation overflow!"); // crash is preferable to corrupting our octree
			}
		}
	}
	inline uint32_t Chunk::alloc_node (SVOAllocator& alloc) {
		if (alloc_ptr >= commit_ptr)
			realloc_nodes(alloc);

		return alloc_ptr++;
	}
}
