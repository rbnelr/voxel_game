#pragma once
#include "blocks.hpp"
#include "stdint.h"
#include "util/freelist_allocator.hpp"
#include "util/virtual_allocator.hpp"
#include <vector>
#include "assert.h"

class Chunk;
class Chunks;
class Player;

inline constexpr uintptr_t round_up_pot (uintptr_t x, uintptr_t y) {
	return (x + y - 1) & ~(y - 1);
}
static constexpr uint16_t INTNULL = (uint16_t)-1;

namespace world_octree {
	enum Node : uint16_t {
		LEAF_BIT = 0x8000u,
		FARPTR_BIT = 0x4000u,
	};

	typedef Node children_t[8];

	static constexpr int MAX_DEPTH = 20;
	static constexpr int MAX_PAGES = 2 << 14; // LEAF_BIT + FARPTR_BIT leave 14 bits as page index

	static constexpr uint16_t PAGE_SIZE = 4096; // must be power of two

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

	static constexpr uint16_t PAGE_MERGE_THRES   = (uint16_t)(PAGE_NODES * 0.80f);

	struct Page {
		PageInfo		info;

		alignas(sizeof(children_t))
		children_t		nodes[PAGE_NODES];

		uint16_t alloc_node () {
			assert(info.count < PAGE_NODES);

			if (info.freelist == INTNULL) {
				return info.count++;
			}

			info.count++;
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
		}

		uint16_t _dbg_count_nodes (Node node=(Node)0) {
			assert((node & LEAF_BIT) == 0);

			int subtree_size = 1; // count ourself

			// cound children subtrees
			for (int i=0; i<8; ++i) {
				auto& children = nodes[node];
				if ((children[i] & (LEAF_BIT|FARPTR_BIT)) == 0)
					subtree_size += _dbg_count_nodes(children[i]);
			}

			return subtree_size;
		}
	};
	static_assert(sizeof(Page) == PAGE_SIZE, "");

	struct PagedOctree {
		VirtualAllocator<Page> allocator = VirtualAllocator<Page>(MAX_PAGES);

		Page* rootpage = nullptr;

		Page* alloc_page () {
			Page* page = allocator.push_back();
			page->info = PageInfo{};

			return page;
		}

		void _print_children (Page* page) {
			printf("page %d :: children:", indexof(page));
			auto* child = page->info.children_ptr;
			while (child) {
				printf(" %d", indexof(child));
				child = child->info.sibling_ptr;
			}
			printf("\n");
		}
		void _validate_farptrs (Page pages[], Page* page, Node node) {
			assert((node & LEAF_BIT) == 0);

			for (int i=0; i<8; ++i) {
				auto val = page->nodes[node][i];
				if ((val & LEAF_BIT) == 0) {
					if (val & FARPTR_BIT) {
						printf("page %d: farptr %d\n", indexof(page), val & ~FARPTR_BIT);

						assert((val & ~FARPTR_BIT) < size());

						_validate_farptrs(pages, &pages[val & ~FARPTR_BIT], (Node)0);
					} else {
						_validate_farptrs(pages, page, page->nodes[node][i]);
					}
				}
			}
		}

		void validate_farptrs () {
			return;
			
			static int counter = 0;

			printf(">>>>>>>> validate  %d\n", counter++);
			printf(">> %d pages\n", size());

			for (int i=0; i<size(); ++i) {
				_print_children(&(*this)[i]);
			}

			_validate_farptrs(&(*this)[0], rootpage, (Node)0);
		}

		// migrate a page to a new memory location, while updating all references to it with the new location
		void migrate_page (Page* src, Page* dst) {
			// update octree node farptr to us in parent page
			if (src->info.farptr_ptr) {
				assert(*src->info.farptr_ptr == (Node)(FARPTR_BIT | allocator.indexof(src)));
				*src->info.farptr_ptr = (Node)(FARPTR_BIT | allocator.indexof(dst));
			}

			// update left siblings sibling ptr
			auto* parent = src->info.parent_ptr();
			if (parent) {
				Page** childref = &parent->info.children_ptr;
				while (*childref != src) {
					childref = &(*childref)->info.sibling_ptr;
				}

				*childref = dst;
			}

			// update parent ptr in children
			auto* child = src->info.children_ptr;
			while (child) {
				child->info.farptr_ptr = (Node*)((uintptr_t)child->info.farptr_ptr + (dst - src) * PAGE_SIZE);
				child = child->info.sibling_ptr;
			}

			// copy all the data, overwriting the dst page
			memcpy(dst, src, sizeof(Page));
		}

		// free a page by swapping it with the last and then shrinking the contiguous page memory by one page
		void free_page (Page* page) {
			validate_farptrs();

			printf(">>>>>>free page %d\n", indexof(page));

			// remove from parent
			auto* parent = page->info.parent_ptr();
			parent->remove_child(page);
			
			// make sure we don't leak pages
			assert(page->info.children_ptr == nullptr);

			Page* lastpage = allocator[(uint16_t)allocator.size() -1];
			if (page != lastpage) {
				// overwrite page to be freed with last page 
				migrate_page(allocator[(uint16_t)allocator.size() -1], page);
			}

			// shrink allocated memory to free last page memory
			allocator.pop_back();


			validate_farptrs();

			printf("------------------------------------------------------------------\n");
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

		int			root_scale = 8;//10;
		int3		root_pos = -(1 << (root_scale - 1));

		PagedOctree pages;

		//
		bool debug_draw_octree = false;

		int debug_draw_octree_min = 4;
		int debug_draw_octree_max = 20;

		bool debug_draw_pages = true;
		bool debug_draw_air = true;

		void imgui ();
		void pre_update (Player const& player);
		void post_update ();

		void add_chunk (Chunk& chunk);
		void remove_chunk (Chunk& chunk);

		void update_block (Chunk& chunk, int3 bpos, block_id id);
	};
}
using world_octree::WorldOctree;
