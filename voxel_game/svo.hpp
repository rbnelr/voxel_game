#pragma once
#include "stdafx.hpp"
#include "blocks.hpp"
#include "util/freelist_allocator.hpp"
#include "util/virtual_allocator.hpp"

class Chunk;
class Chunks;
class Player;
struct WorldGenerator;

namespace svo {
	inline constexpr uintptr_t round_up_pot (uintptr_t x, uintptr_t y) {
		return (x + y - 1) & ~(y - 1);
	}

	enum Node : uint16_t {
		LEAF_BIT = 0x8000u,
		FARPTR_BIT = 0x4000u,
	};

	static constexpr int MAX_DEPTH = 20;
	static constexpr int MAX_PAGES = 2 << 14; // LEAF_BIT + FARPTR_BIT leave 14 bits as page index

	static constexpr Node NULLNODE = (Node)(LEAF_BIT | B_NULL);
	
	struct Page;
	struct PagedOctree;

	struct NodeChildren {
		Node			children[8];
	};

	static constexpr uint16_t PAGE_SIZE = 4096 * 2; // must be power of two

	// get the page that contains a node
	inline Page* page_from_node (Node* node) {
		return (Page*)( (uintptr_t)node & ~(PAGE_SIZE - 1) ); // round down the ptr to get the page pointer
	}
	inline Page* page_from_node (NodeChildren* node) {
		return (Page*)( (uintptr_t)node & ~(PAGE_SIZE - 1) ); // round down the ptr to get the page pointer
	}
	// get the 8 nodes that a node is grouped into (siblings)
	inline NodeChildren* siblings_from_node (Node* node) {
		return (NodeChildren*)((uintptr_t)node & ~(sizeof(NodeChildren) -1)); // round down the ptr to get the siblings pointer
	}

	struct PageInfo {
		uint16_t		count = 0;
		uint16_t		freelist = 0; // 0 can never be a valid freelist value, because root (first) node should never be freed
		uint16_t		scale = 0;

		Node*			farptr_ptr = nullptr;
		Page*			sibling_ptr = nullptr; // ptr to next child of parent
		Page*			children_ptr = nullptr; // ptr to first child

		Page* parent_ptr () {
			return page_from_node(farptr_ptr);
		}
	};

	static constexpr uint16_t INFO_SIZE = (uint16_t)round_up_pot(sizeof(PageInfo), sizeof(NodeChildren));
	static constexpr uint16_t PAGE_NODES = (uint16_t)((PAGE_SIZE - INFO_SIZE) / (sizeof(NodeChildren)));

	static constexpr uint16_t PAGE_MERGE_THRES   = (uint16_t)(PAGE_NODES * 0.85f);

	static constexpr float ROOT_MOVE_HISTER		= 0.05f;


	struct Page {
		PageInfo		info;

		NodeChildren	nodes[PAGE_NODES];

		bool nodes_full () {
			return info.count >= PAGE_NODES;
		}

		uint16_t alloc_node () {
			assert(info.count < PAGE_NODES);

			auto ptr = info.count++;

			if (!info.freelist) {
				return ptr;
			}

			uint16_t ret = info.freelist;
			info.freelist = *((uint16_t*)&nodes[info.freelist]);
			return ret;
		}
		void free_node (uint16_t node) {
			assert(node > 0 && info.count > 0 && (node & (FARPTR_BIT|LEAF_BIT)) == 0 && node < PAGE_NODES);

		#if !NDEBUG
			memset(&nodes[node], 0, sizeof(nodes[node]));
		#endif

			*((uint16_t*)&nodes[node]) = info.freelist;
			info.freelist = node;
			info.count--;
		}

		void free_all_nodes () {
			info.count = 0;
			info.freelist = 0;
		}

		void add_child (Node* farptr_ptr, Page* child) {
			child->info.farptr_ptr = farptr_ptr;
			child->info.sibling_ptr = info.children_ptr;
			info.children_ptr = child;

			assert(child->info.parent_ptr() == this);
		}

		void remove_all_children () {
			auto* cur = info.children_ptr;
			info.children_ptr = nullptr;
			while (cur) {
				auto* child = cur;
				cur = child->info.sibling_ptr;
				child->info.sibling_ptr = nullptr;
				child->info.farptr_ptr = nullptr;
			}
		}

		void remove_child (Page* child) {
			Page** childref = &info.children_ptr;
			while (*childref != child) {
				childref = &(*childref)->info.sibling_ptr;
			}

			*childref = (*childref)->info.sibling_ptr;
			child->info.farptr_ptr = nullptr;
		}
	};
	static_assert(sizeof(Page) == PAGE_SIZE, "");

	struct PagedOctree {
		SparseAllocator<Page> allocator = SparseAllocator<Page>(MAX_PAGES);

		Page* alloc_page () {
			Page* page = allocator.alloc_threadsafe();
			page->info = PageInfo{};

			return page;
		}

		void free_page (Page* page) {
			// NOTE: This gets called from background threads when they generate the SVO for a chunk
			// The following ptr updates could seem unsafe, but at no point do the threads ever own pointers to any but the nodes they just allocated
			// So this is perfectly safe without requiring more sync

			// remove from parent
			auto* parent = page->info.parent_ptr();
			if (parent)
				parent->remove_child(page);
			
			// free child pages
			auto* child = page->info.children_ptr;
			while (child) {
				Page* next = child->info.sibling_ptr;
				free_page(child);
				child = next;
			}

			// free page
			allocator.free_threadsafe(page);
		}
		
		Page& operator[] (uint16_t i) { return *allocator[i]; }

		uint16_t indexof (Page* page) {
			return (uint16_t)allocator.indexof(page);
		}
	};

	class SVO {
	public:

		int			root_scale = 10;
		int3		root_pos = -(1 << (root_scale - 1));

		float3		root_move_hister = 0;

		PagedOctree pages;

		SVO ();

		//std::unordered_set<chunk_coord_hashmap, std::unique_ptr<Chunk>> queued_chunks;
		bool is_chunk_load_queued (Chunks& chunks, int3 coord);

		void queue_chunk_load (Chunks& chunks, WorldGenerator& world_gen, int3 coord);
		void unload_chunk (Chunks& chunks, int3 coord);

		//
		bool debug_draw_octree = 0;//1;

		int debug_draw_octree_min = 4;
		int debug_draw_octree_max = 20;

		bool debug_draw_page_color = true;
		bool debug_draw_pages = true;
		bool debug_draw_air = true;

		int page_split_counter = 0;
		int page_merge_counter = 0;

		void imgui ();
		void pre_update (Player const& player);
		void post_update ();

		Node chunk_to_octree (Chunk& chunk);
		void chunk_loading (Chunks& chunks, WorldGenerator& world_gen, float3 player_pos);

		void update_block (Chunk& chunk, int3 bpos, block_id id);
	};
}
using svo::SVO;
