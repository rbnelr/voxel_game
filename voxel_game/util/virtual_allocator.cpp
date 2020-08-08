#include "virtual_allocator.hpp"
#include "windows.h"

uintptr_t get_os_page_size () {
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	return (uintptr_t)info.dwPageSize;
}

void* reserve_address_space (uintptr_t size) {
	void* baseptr = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
	assert(baseptr != nullptr);
	return baseptr;
}

void release_address_space (void* baseptr, uintptr_t size) {
	auto ret = VirtualFree(baseptr, 0, MEM_RELEASE);
	assert(ret != 0);
}

void commit_pages (void* ptr, uintptr_t size) {
	auto ret = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
	assert(ret != NULL);

	TracyAlloc(ptr, size);
}

void decommit_pages (void* ptr, uintptr_t size) {
	TracyFree(ptr);

	auto ret = VirtualFree(ptr, size, MEM_DECOMMIT);
	assert(ret != 0);
}

uint32_t _alloc_first_free (std::vector<uint64_t>& freeset) {
	
	uint32_t i=0;
	while (freeset[i] == 0ull) {
		++i;
	}

	auto& val = freeset[i];

	unsigned long subindex;
	auto ret = _BitScanForward64(&subindex, val);
	assert(ret);

	// set the first 1 bit found to 0
	val ^= 1ull << subindex;

	return i*64 + (uint32_t)subindex;
}
