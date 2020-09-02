#pragma once
#include "stdint.h"
#include "move_only_class.hpp"
#include "assert.h"
#include "tracy.hpp"
#include <vector>
#include <string>
#include "dear_imgui.hpp"

// set uninitialized stuff to 0xcc to either catch out of unitialized bugs or make viewing it in the debugger clearer
#define DBG_MEMSET (!NDEBUG) 
#define DBG_MEMSET_VAL 0xCC

uint32_t get_os_page_size ();

inline const uint32_t os_page_size = get_os_page_size();

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

// Bitset used in allocators to find the lowest free slot in a fast way
struct Bitset {
	std::vector<uint64_t>	bits;
	uint32_t				first_set = 0; // index of first set bit in bits, to possibly speed up scanning

	// finds the first 1 bit and clears it, returns the index of the bit
	uint32_t clear_first_1 (uint64_t* prev_bits=nullptr);

	// set a bit, safe to set bit that's already set
	void set_bit (uint32_t idx, uint64_t* new_bits=nullptr);
};
