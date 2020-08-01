#pragma once
#include <mutex>
#include <cstdlib>

#define FREELIST_ALLOC_DEBUG_VALUES (!NDEBUG)

// Custom memory allocator that allocates in fixed blocks using a freelist
// used to avoid malloc and free overhead
template <typename T, int ALIGN=alignof(std::max_align_t)>
class FreelistAllocator {
	union Block {
		Block*	next; // link to next block in linked list of free blocks
		T		data; // data if allocated
	};

	Block* freelist = nullptr;

	mutable std::mutex m;

public:
	// allocate a T (not threadsafe)
	Block* _alloc () {
		if (!freelist)
			return nullptr;

		// remove first Block of freelist
		Block* block = freelist;
		freelist = block->next;

		return block;
	}

	T* _init (Block* block) {
		if (!block) {
			// allocate new blocks as needed
			block = (Block*)_aligned_malloc(sizeof(Block), ALIGN);
		}

	#if FREELIST_ALLOC_DEBUG_VALUES
		memset(block, 0xcd, sizeof(Block));
	#endif

		// placement new to init T
		new (&block->data) T ();

		return &block->data;
	}

	T* alloc () {
		Block* block = _alloc();
		return _init(block);
	}

	T* alloc_threadsafe () {
		Block* block;
		{
			std::lock_guard<std::mutex> lock(m);
			block = _alloc();
		}

		return _init(block);
	}

	// free a ptr (not threadsafe)
	void free (T* ptr) {
		// blocks are never freed for now

		Block* block = (Block*)ptr;

		// 'placement' delete by just calling the destructor
		block->data.~T();

	#if FREELIST_ALLOC_DEBUG_VALUES
		memset(block, 0xdd, sizeof(Block));
	#endif

		// add block to freelist
		block->next = freelist;
		freelist = block;
	}

	void free_threadsafe (T* ptr) {
		std::lock_guard<std::mutex> lock(m);
		free(ptr);
	}

	~FreelistAllocator () {
		while (freelist) {
			Block* block = freelist;
			freelist = block->next;

			_aligned_free(block);
		}
	}

	int count () {
		int count = 0;

		Block* block = freelist;
		while (block) {
			block = block->next;
			count++;
		}

		return count;
	}
};
