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
	Block* _alloc () {
		if (!freelist)
			return nullptr;

		// remove first Block of freelist
		Block* block = freelist;
		freelist = block->next;

		return block;
	}

	T* alloc () {
		Block* block = _alloc();
		if (!block) {
			OPTICK_EVENT("malloc BlockAllocator::alloc");
			// allocate new blocks as needed
			block = (Block*)malloc(sizeof(Block));
		}
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
		Block* block;
		{
			OPTICK_EVENT("mutex BlockAllocator::alloc_threadsafe");

			std::lock_guard<std::mutex> lock(m);
			block = _alloc();
		}
		// Do the malloc outside the block because malloc can take a long time, which can catastrophically block an entire threadpool
		if (!block) {
			OPTICK_EVENT("malloc BlockAllocator::alloc_threadsafe");

			// allocate new blocks as needed
			block = (Block*)malloc(sizeof(Block)); // NOTE: malloc itself mutexes, so there is little point to putting this outside of the lock
		}
		return &block->data;
	}

	void free_threadsafe (T* ptr) {
		OPTICK_EVENT("mutex BlockAllocator::free_threadsafe");

		std::lock_guard<std::mutex> lock(m);
		free(ptr);
	}

	~BlockAllocator () {
		while (freelist) {
			OPTICK_EVENT("free() ~BlockAllocator");

			Block* block = freelist;
			freelist = block->next;

			::free(block);
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
