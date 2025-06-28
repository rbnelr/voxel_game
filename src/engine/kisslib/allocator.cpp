#include "allocator.hpp"
#include "clean_windows_h.hpp"

////// Platform specific code

uint32_t get_os_page_size () {
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	return (uint32_t)info.dwPageSize;
}

//// VirtualAlloc
void* reserve_address_space (size_t size) {
	ALLOCATOR_PROFILE_SCOPED("reserve_address_space");

	void* baseptr = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
	assert(baseptr != nullptr);
	return baseptr;
}

void release_address_space (void* baseptr, size_t size) {
	ALLOCATOR_PROFILE_SCOPED("release_address_space");

	auto ret = VirtualFree(baseptr, 0, MEM_RELEASE);
	assert(ret != 0);
}

void commit_pages (void* ptr, size_t size) {
	ALLOCATOR_PROFILE_SCOPED("commit_pages");

	auto ret = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
	assert(ret != NULL);
}

void decommit_pages (void* ptr, size_t size) {
	ALLOCATOR_PROFILE_SCOPED("decommit_pages");

	auto ret = VirtualFree(ptr, size, MEM_DECOMMIT);
	assert(ret != 0);
}

//// AllocatorBitset
uint32_t _bsf_1 (uint64_t val) {
	unsigned long idx;
	auto ret = _BitScanForward64(&idx, val);
	assert(ret);
	return idx;
}
uint32_t _bsr_0 (uint64_t val) {
	unsigned long idx;
	auto ret = _BitScanReverse64(&idx, ~val);
	assert(ret);
	return idx;
}

