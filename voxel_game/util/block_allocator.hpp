#pragma once
#include "stdlib.h"
#include "optick.h"

// Custom memory allocator that allocates in fixed blocks using a freelist
// used to avoid malloc and free overhead
template <typename T>
class BlockAllocator {
	union Block {
		Block*	next; // link to next block in linked list of free blocks
		T		data; // data if allocated
	};

	Block* freelist = nullptr;

	mutable std::mutex m;

public:
	// allocate a T (not threadsafe)
	T* alloc () {
		if (!freelist) {
			OPTICK_EVENT();

			// allocate new blocks as needed
			freelist = (Block*)malloc(sizeof(Block));
			freelist->next = nullptr;
		}

		// remove first Block of freelist
		Block* block = freelist;
		freelist = block->next;

		return &block->data;
	}

	// free a ptr (not threadsafe)
	void free (T* ptr) {
		// blocks are never freed for now

		Block* block = (Block*)ptr;

		// add block to freelist
		block->next = freelist;
		freelist = block;
	}

	T* alloc_threadsafe () {
		std::lock_guard<std::mutex> lock(m);
		return alloc();
	}

	void free_threadsafe (T* ptr) {
		std::lock_guard<std::mutex> lock(m);
		free(ptr);
	}
};
