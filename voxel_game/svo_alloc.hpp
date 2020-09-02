#pragma once
#include "stdafx.hpp"
#include "util/virtual_allocator.hpp"

static constexpr int CHUNK_SIZE = 64;
static constexpr uint32_t CHUNK_SCALE = 6;

static constexpr uint32_t MAX_CHUNKS = 1u << 16;

namespace svo {
	static constexpr uint32_t MAX_NODES = 1u << 16;

	static constexpr uint32_t MAX_DEPTH = 20;

	struct Node {
		uint16_t children[8] = {};
		uint8_t leaf_mask = 0xff;
		uint8_t pad[15];
	};
	static_assert(is_pot(sizeof(Node)), "sizeof(Node) must be power of two!");

	struct SVOAllocator;

	struct StackNode {
		Node* node; // parent node
		int3 pos;
		int child_indx;
	};

	enum ChunkFlags : uint8_t {
		LOCKED = 1, // read only, still displayed but not allowed to be modified
		GPU_DIRTY = 2,
	};
	ENUM_BITFLAG_OPERATORS(ChunkFlags, uint8_t)

	struct Chunk {
		Node* nodes;

		int3	pos;
		uint8_t	scale;

		ChunkFlags flags;

		uint32_t alloc_ptr;
		uint32_t dead_count; // TODO: cant actually keep track of nodes that disappear because of octree_writes with scale != 0, so maybe just keep a write counter that then occasionally triggers compactings
		uint32_t commit_ptr;

		char _pad[28];

		Node tinydata[6];

		Chunk (int3	pos, uint8_t scale): pos{pos}, scale{scale} {
			nodes = tinydata;

			flags = GPU_DIRTY;

			alloc_ptr = 0;
			dead_count = 0;
			commit_ptr = (uint32_t)ARRLEN(tinydata);

		#if DBG_MEMSET
			memset(tinydata, DBG_MEMSET_VAL, sizeof(tinydata));
		#endif
		}

		inline uint16_t alloc_node (SVOAllocator& alloc, StackNode* stack=nullptr);
	};
	static constexpr uint32_t _sz = sizeof(Chunk);

	struct SVOAllocator {

		static_assert(is_pot(sizeof(Chunk)), "sizeof(Chunk) needs to be power of two!");
		static_assert(is_pot(sizeof(Node)), "SparseAllocator<T>: sizeof(Node) needs to be power of two!");
		static_assert(is_pot(sizeof(Node[MAX_NODES])), "SparseAllocator<T>: sizeof(Node[MAX_NODES]) needs to be power of two!");

		// Memory layout, entire memory is reserved, used parts are comitted
		// error C2148: total size of array must not exceed 0x7fffffff bytes, MSVC bug/issue
		struct Memory {
			Chunk chunks[MAX_CHUNKS]; // sparse based on free_chunks
			Node nodes[100/*workaround MAX_CHUNKS*/][MAX_NODES]; // sparse based on commit_ptr in Chunk
		};
		static constexpr uintptr_t RESERVE_SIZE = sizeof(Chunk) * MAX_CHUNKS + sizeof(Node) * MAX_CHUNKS * MAX_NODES; //sizeof(Memory);

		Memory*					mem;

		uint32_t				chunk_count = 0;

		// sparse chunk paging
		Bitset					free_chunks;
		uint32_t				paging_shift_mask;
		uint32_t				paging_mask;

		SVOAllocator () {
			uint32_t chunks_per_page = (uint32_t)(os_page_size / sizeof(Chunk));
			assert(chunks_per_page <= 64); // cannot support more than 64 slots per os page, would require multiple bitset reads to check commit status

			uint32_t tmp = max(chunks_per_page, 1); // prevent 0 problems
			paging_shift_mask = 0b111111u ^ (tmp - 1); // indx in 64 bitset block, then round down to slots_per_page
			paging_mask = (1u << tmp) - 1; // simply slots_per_page 1 bits

			mem = (Memory*)reserve_address_space(RESERVE_SIZE);
		}

		~SVOAllocator () {
			release_address_space(mem, RESERVE_SIZE);
		}

		// Allocate first possible slot
		Chunk* alloc_chunk () {
			assert(chunk_count < MAX_CHUNKS);

			uint64_t prev_bits;
			uint32_t idx = free_chunks.clear_first_1(&prev_bits);
			chunk_count++;

			Chunk* chunk = &mem->chunks[idx];

			// get bits representing prev alloc status of all slots in affected page
			prev_bits >>= idx & paging_shift_mask;

			// commit page if all page slots were free before
			if (((uint32_t)prev_bits & paging_mask) == paging_mask) {
				commit_pages((void*)((uintptr_t)chunk & ~(uintptr_t)(os_page_size-1)), os_page_size);

			#if DBG_MEMSET
				memset(chunk, DBG_MEMSET_VAL, os_page_size);
			#endif
			}

			return chunk;
		}

		// Free random slot
		void free_chunk (Chunk* chunk) {
			if (chunk->nodes != chunk->tinydata)
				decommit_pages(chunk->nodes, sizeof(Node)*MAX_NODES);

			uint32_t idx = indexof(chunk);

			uint64_t new_bits;
			free_chunks.set_bit(idx, &new_bits);
			chunk_count--;

			// get bits representing new alloc status of all slots in affected page
			new_bits >>= idx & paging_shift_mask;

			// decommit page if all page slots are free now
			if (((uint32_t)new_bits & paging_mask) == paging_mask) {
				decommit_pages((void*)((uintptr_t)chunk & ~(uintptr_t)(os_page_size-1)), os_page_size);
			}
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

	inline uint16_t Chunk::alloc_node (SVOAllocator& alloc, StackNode* stack) {
		if (alloc_ptr >= commit_ptr) {
			if (nodes == tinydata) {
				nodes = alloc.mem->nodes[alloc.indexof(this)];

				uint32_t commit_block = os_page_size / sizeof(Node);
				commit_ptr = commit_block;
				commit_pages(nodes, os_page_size);

			#if DBG_MEMSET
				memset(nodes, DBG_MEMSET_VAL, os_page_size);
			#endif

				memcpy(nodes, tinydata, sizeof(tinydata));

				if (stack) { // fixup stack after we invalidated some of the nodes
					for (int i=0; i<MAX_DEPTH; ++i) {
						uintptr_t indx = stack[i].node - tinydata;
						if (indx < ARRLEN(tinydata)) {
							stack[i].node = nodes + indx;
						}
					}
				}

			#if DBG_MEMSET
				memset(tinydata, DBG_MEMSET_VAL, sizeof(tinydata));
			#endif
			} else {
				if (commit_ptr < MAX_NODES) {
					uint32_t commit_block = os_page_size / sizeof(Node);

					commit_pages(nodes + commit_ptr, os_page_size);

				#if DBG_MEMSET
					memset(nodes + commit_ptr, DBG_MEMSET_VAL, os_page_size);
				#endif
					commit_ptr += commit_block;

				} else {
					assert(false);
					throw std::runtime_error("Octree allocation overflow"); // crash is preferable to corrupting our octree
				}
			}

		}
		return (uint16_t)alloc_ptr++;
	}
}
