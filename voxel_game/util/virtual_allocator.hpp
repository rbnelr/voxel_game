#pragma once
#include "stdint.h"
#include "move_only_class.hpp"
#include "assert.h"
#include "tracy.hpp"
#include <vector>

namespace {
	// round up x to y, assume y is power of two
	inline constexpr uintptr_t round_up_pot (uintptr_t x, uintptr_t y) {
		return (x + y - 1) & ~(y - 1);
	}
}

uintptr_t get_os_page_size ();

inline const uintptr_t os_page_size = get_os_page_size();

void* reserve_address_space (uintptr_t size);
void release_address_space (void* baseptr, uintptr_t size);
void commit_pages (void* ptr, uintptr_t size);
void decommit_pages (void* ptr, uintptr_t size);

/*
	Allocators implemented using OS-level virtual memory
*/

// like std::stack, but can grow up to a max size without needing to copy data (or invalidate pointers)
template <typename T>
class VirtualStackAllocator {
	void*		baseptr; // base address of reserved memory pages
	void*		commitptr; // [baseptr, commitptr) is the commited memory region

	T*			end; // next T to be suballocated (one after end of contiguous allocated Ts)
	uintptr_t	max_count; // max possible number of Ts, a corresponding number of pages will be reserved
public:

	VirtualStackAllocator (uintptr_t max_count): max_count{max_count} {
		baseptr = reserve_address_space(sizeof(T)*max_count);
		commitptr = baseptr;
		end = (T*)baseptr;
	}

	~VirtualStackAllocator () {
		release_address_space(baseptr, sizeof(T)*max_count);
	}

	// Allocate from back
	T* push (uintptr_t count=1) {
		assert(size() < max_count);

		T* ptr = end;
		end += count;

		if ((char*)end > commitptr) {
			// at least one page to be committed

			uintptr_t size_needed = (char*)end - (char*)commitptr;
			size_needed = round_up_pot(size_needed, os_page_size);

			commit_pages(commitptr, size_needed);

			commitptr += size_needed;
		}

		return (T*)ptr;
	}

	// Free from back
	void pop (uintptr_t count=1) {
		assert(size() > 0);

		T* ptr = end;
		end -= count;

		if ((char*)end <= (char*)commitptr - os_page_size) {
			// at least 1 page to be decommitted
			
			char* decommit = (char*)round_up_pot((uintptr_t)end, os_page_size);
			uintptr_t size = (char*)commitptr - decommit; 

			decommit_pages(decommit, size);

			commitptr = decommit;
		}
	}

	inline uintptr_t indexof (T* ptr) const {
		return ptr - (T*)baseptr;
	}
	inline T* operator[] (uintptr_t index) const {
		return (T*)baseptr + index;
	}

	inline uintptr_t size () {
		return end - (T*)baseptr;
	}
};

uint32_t _alloc_first_free (std::vector<uint64_t>& freeset);

// Virtual memory allocator that can accept random order of alloc and free
// sizeof(T) should be multiple of os page size as to not waste memory and also keep bookkeeping simple
// TODO: A version of this that allows sizeof(T) to be anything can be implemented with list of pages and list of elements
// where pages have a refcount of how many elements are allocated out of them (refcount == 0 -> page can be decommitted)
template <typename T>
class SparseAllocator {
	void*					baseptr; // base address of reserved memory pages
	uint32_t				count = 0;
	uint32_t				max_count; // max possible number of Ts, a corresponding number of pages will be reserved
	std::vector<uint64_t>	freeset;

public:

	SparseAllocator (uint32_t max_count): max_count{max_count} {
		assert(sizeof(T) % os_page_size == 0);
		baseptr = reserve_address_space(sizeof(T)*max_count);
	}

	~SparseAllocator () {
		release_address_space(baseptr, sizeof(T)*max_count);
	}

	// Allocate (picks first possible address)
	T* alloc () {
		assert(count < max_count);

		uint32_t bookkeeping_range = (uint32_t)freeset.size() * 64;
		uint32_t free_count = bookkeeping_range - count;

		uint32_t idx;

		if (free_count == 0) {
			// allocate after the end of the contiguous region of allocated elements
			idx = bookkeeping_range;

			freeset.push_back(0xfffffffffffffffeull); // push back all 1s except for the LSB representing the one element we are about to allocate
		} else {
			// bookkeeping region contains free elements allocate first one with a 64 wide scan
			idx = _alloc_first_free(freeset);
		}

		T* ptr = (T*)baseptr + idx;
		count++;

		commit_pages(ptr, sizeof(T));

		return ptr;
	}

	// Free random
	void free (T* ptr) {
		uint32_t idx = indexof(ptr);

		uint32_t freesety = idx / 64;
		uint32_t freesetx = idx % 64;
		assert(freesety < freeset.size() && ((freeset[freesety] >> freesetx) & 1) == 0); // not freed yet
		assert(count > 0);

		// set bit in freeset to 1
		freeset[freesety] |= 1ull << freesetx;

		// shrink freeset if there are contiguous free elements at the end
		if (freesety == freeset.size()-1) {
			while (freeset.back() == 0xffffffffffffffffull) {
				freeset.pop_back();
			}
		}

		count--;

		decommit_pages(ptr, sizeof(T));
	}

	inline uint32_t indexof (T* ptr) const {
		return (uint32_t)(ptr - (T*)baseptr);
	}
	inline T* operator[] (uint32_t index) const {
		return (T*)baseptr + index;
	}

	inline uint32_t size () {
		return count;
	}
	inline uint32_t freeset_size () {
		return (uint32_t)freeset.size() * 64;
	}

	inline bool is_allocated (uint32_t i) {
		return (freeset[i / 64] >> (i % 64)) == 0;
	}
};
