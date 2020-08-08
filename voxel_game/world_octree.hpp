#pragma once
#include "stdafx.hpp"
#include "blocks.hpp"
#include "util/freelist_allocator.hpp"
#include "util/virtual_allocator.hpp"

class Chunk;
class Chunks;
class Player;

namespace world_octree {
	inline constexpr uintptr_t round_up_pot (uintptr_t x, uintptr_t y) {
		return (x + y - 1) & ~(y - 1);
	}
	static constexpr uint16_t INTNULL = (uint16_t)-1;

	enum Node : uint16_t {
		LEAF_BIT = 0x8000u,
		FARPTR_BIT = 0x4000u,
	};

	typedef Node children_t[8];

	static constexpr int MAX_DEPTH = 20;
	static constexpr int MAX_PAGES = 2 << 14; // LEAF_BIT + FARPTR_BIT leave 14 bits as page index

	static constexpr uint16_t PAGE_SIZE = 4096 * 2; // must be power of two

	struct Page;
	struct PagedOctree;

	// get the page that contains a node
	inline Page* page_from_node (Node* node) {
		return (Page*)( (uintptr_t)node & ~(PAGE_SIZE - 1) ); // round down the ptr to get the page pointer
	}
	// get the 8 nodes that a node is grouped into (siblings)
	inline children_t* siblings_from_node (Node* node) {
		return (children_t*)((uintptr_t)node & ~(sizeof(children_t) -1)); // round down the ptr to get the siblings pointer
	}

	struct PageInfo {
		uint16_t		count = 0;
		uint16_t		freelist = INTNULL;

		Node*			farptr_ptr = nullptr;
		Page*			sibling_ptr = nullptr; // ptr to next child of parent
		Page*			children_ptr = nullptr; // ptr to first child

		Page* parent_ptr () {
			return page_from_node(farptr_ptr);
		}
	};

	static constexpr uint16_t INFO_SIZE = (uint16_t)round_up_pot(sizeof(PageInfo), sizeof(children_t));
	static constexpr uint16_t PAGE_NODES = (uint16_t)((PAGE_SIZE - INFO_SIZE) / sizeof(children_t));

	static constexpr uint16_t PAGE_MERGE_THRES   = (uint16_t)(PAGE_NODES * 0.8f);

	struct Page {
		PageInfo		info;

		alignas(sizeof(children_t))
		children_t		nodes[PAGE_NODES];

		bool nodes_full () {
			return info.count >= PAGE_NODES;
		}

		uint16_t alloc_node () {
			assert(info.count < PAGE_NODES);

			auto ptr = info.count++;

			if (info.freelist == INTNULL) {
				return ptr;
			}

			uint16_t ret = info.freelist;
			info.freelist = *((uint16_t*)&nodes[info.freelist]);
			return ret;
		}
		void free_node (uint16_t node) {
			assert(info.count > 0 && (node & (FARPTR_BIT|LEAF_BIT)) == 0 && node < PAGE_NODES);

			*((uint16_t*)&nodes[node]) = info.freelist;
			info.freelist = node;
			info.count--;
		}

		void free_all_nodes () {
			info.count = 0;
			info.freelist = INTNULL;
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
			Page* page = allocator.alloc();
			page->info = PageInfo{};

			return page;
		}

		void free_page (Page* page) {
			// remove from parent
			auto* parent = page->info.parent_ptr();
			if (parent)
				parent->remove_child(page);
			
			// make sure we don't leak pages
			assert(page->info.children_ptr == nullptr);

			// free page
			allocator.free(page);
		}
		
		uint16_t size () { return (uint16_t)allocator.size(); }
		Page& operator[] (uint16_t i) { return *allocator[i]; }

		uint16_t indexof (Page* page) {
			uintptr_t index = allocator.indexof(page);
			assert(index < allocator.size());
			return (uint16_t)index;
		}
	};

	class WorldOctree {
	public:

		int			root_scale = 11;
		int3		root_pos = -(1 << (root_scale - 1));

		PagedOctree pages;

		//
		bool debug_draw_octree = 1;

		int debug_draw_octree_min = 4;
		int debug_draw_octree_max = 20;

		bool debug_draw_pages = true;
		bool debug_draw_air = true;

		int page_split_counter = 0;
		int page_merge_counter = 0;
		int page_free_counter = 0;

		void imgui ();
		void pre_update (Player const& player);
		void post_update ();

		void add_chunk (Chunk& chunk);
		void remove_chunk (Chunk& chunk);

		void update_block (Chunk& chunk, int3 bpos, block_id id);
	};
}
using world_octree::WorldOctree;
