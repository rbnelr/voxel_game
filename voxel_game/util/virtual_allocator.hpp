#include "stdint.h"
#include "move_only_class.hpp"
#include "assert.h"

extern const uintptr_t os_page_size;

void* reserve_address_space (uintptr_t size);
void release_address_space (void* baseptr, uintptr_t size);
void* commit_pages (void* baseptr, void* neededptr, void* commitptr);
void* decommit_pages (void* baseptr, void* neededptr, void* commitptr);

// Allocator implemented using OS-level virtual memory
// like std::vector, but can grow up to a max size without needing to copy data (and invalidating pointers)
// You need to specify a max possible number of allocated 'pages'
// the memory space is then reserved from the OS, but not committed yet, so no ram will actually be used yet
// you can then use push_back and pop_back like on a std::vector to allocate pages at the end, only then will they be committed from ram
template <typename T>
class VirtualAllocator {
	void*		baseptr; // base address of reserved memory pages
	void*		commitptr; // first page that is not yet committed

	T*			end; // next T to be suballocated (one after end of contiguous allocated Ts)
	uintptr_t	max_count; // max possible number of Ts, a corresponding number of pages will be reserved
public:

	VirtualAllocator (uintptr_t max_count): max_count{max_count} {
		baseptr = reserve_address_space(sizeof(T)*max_count);
		commitptr = baseptr;
		end = (T*)baseptr;
	}

	~VirtualAllocator () {
		release_address_space(baseptr, sizeof(T)*max_count);
	}

	// Allocate from back
	T* push_back () {
		assert(size() < max_count);
		//if (size() >= max_count) {
		//	return nullptr;
		//}

		T* ptr = end++;

		if ((char*)end > commitptr) {
			// at least one page to be committed
			commitptr = commit_pages(baseptr, end, commitptr);
		}

		return (T*)ptr;
	}

	// Free from back
	void pop_back () {
		assert(size() > 0);

		T* ptr = end--;

		if ((char*)end <= (char*)commitptr - os_page_size) {
			// at least 1 page to be decommitted
			commitptr = decommit_pages(baseptr, end, commitptr);
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
