#pragma once
#include "stdint.h"
#include "move_only_class.hpp"
#include "assert.h"
#include "tracy.hpp"
#include <vector>
#include <string>

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

template <typename T, bool DOCOMMIT=true> // DOCOMMIT=false allows manual memory sparseness with sizeof(T) > os_page_size
class SparseAllocator {
	void*					baseptr; // base address of reserved memory pages
	uint32_t				count = 0;
	uint32_t				max_count; // max possible number of Ts, a corresponding number of pages will be reserved
	Bitset					free_slots;
	uint32_t				paging_shift_mask;
	uint32_t				paging_mask;

	static_assert(is_pot(sizeof(T)), "SparseAllocator<T>: sizeof(T) needs to be power of two!");

public:

	SparseAllocator (uint32_t max_count): max_count{max_count} {
		if (DOCOMMIT) {
			uint32_t slots_per_page = (uint32_t)(os_page_size / sizeof(T));
			assert(slots_per_page <= 64); // cannot support more than 64 slots per os page, would require multiple bitset reads to check commit status
			
			uint32_t tmp = max(slots_per_page, 1); // prevent 0 problems
			paging_shift_mask = 0b111111u ^ (tmp - 1); // indx in 64 bitset block, then round down to slots_per_page
			paging_mask = (1u << tmp) - 1; // simply slots_per_page 1 bits
		}

		baseptr = reserve_address_space(sizeof(T)*max_count);
	}

	~SparseAllocator () {
		release_address_space(baseptr, sizeof(T)*max_count);
	}

	// Allocate first possible slot
	T* alloc () {
		assert(count < max_count);

		uint64_t prev_bits;
		uint32_t idx = free_slots.clear_first_1(&prev_bits);
		count++;

		T* ptr = (T*)baseptr + idx;
		
		if (DOCOMMIT) {
			// get bits representing prev alloc status of all slots in affected page
			prev_bits >>= idx & paging_shift_mask;
		
			// commit page if all page slots were free before
			if (((uint32_t)prev_bits & paging_mask) == paging_mask) {
				commit_pages(ptr, sizeof(T));

			#if DBG_MEMSET
				memset(ptr, DBG_MEMSET_VAL, sizeof(T));
			#endif
			}
		}

		return ptr;
	}

	// Free random slot
	void free (T* ptr) {
		uint32_t idx = indexof(ptr);

		uint64_t new_bits;
		free_slots.set_bit(idx, &new_bits);
		count--;

		if (DOCOMMIT) {
			// get bits representing new alloc status of all slots in affected page
			new_bits >>= idx & paging_shift_mask;

			// decommit page if all page slots are free now
			if (((uint32_t)new_bits & paging_mask) == paging_mask) {
				decommit_pages(ptr, sizeof(T));
			}
		}
	}

	uint32_t size () {
		return count;
	}
	uint32_t indexof (T* ptr) const {
		return (uint32_t)(ptr - (T*)baseptr);
	}
	T* operator[] (uint32_t index) const {
		return (T*)baseptr + index;
	}

	std::string dbg_string_free_slots () {
		std::string str = "";
		for (int i=0; i<free_slots.bits.size(); ++i) {
			char bits[64];
			for (int j=0; j<64; ++j)
				bits[j] = (free_slots.bits[i] & (1ull << j)) ? '1':'0';
			str.append(bits, 64);
			str.append("\n");
		}
		return str;
	}
};

// Threadsafe wrappers
#include <mutex>

// Need to wrap locks for tracy
#define MUTEX				TracyLockableN(std::mutex,	m, "ThreadsafeQueue mutex")
#define LOCK_GUARD			std::lock_guard<LockableBase(std::mutex)> lock(m)

template <typename T>
class ThreadsafeSparseAllocator : public SparseAllocator<T> {
	MUTEX;

public:
	ThreadsafeSparseAllocator (uint32_t max_count): SparseAllocator<T>{max_count} {}

	T* alloc_threadsafe () {
		LOCK_GUARD;
		return this->alloc();
	}
	void free_threadsafe (T* ptr) {
		LOCK_GUARD;
		this->free(ptr);
	}

	uint32_t size_threadsafe () {
		LOCK_GUARD;
		return this->count;
	}
};

#undef MUTEX			
#undef LOCK_GUARD		
