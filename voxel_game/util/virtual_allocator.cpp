#include "virtual_allocator.hpp"
#include "windows.h"

uintptr_t get_os_page_size () {
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	return (uintptr_t)info.dwPageSize;
}

const uintptr_t os_page_size = get_os_page_size();

// round up x to y, assume y is power of two
uintptr_t round_up_pot (uintptr_t x, uintptr_t y) {
	return (x + y - 1) & ~(y - 1);
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

void* commit_pages (void* baseptr, void* neededptr, void* commitptr) {

	uintptr_t size_needed = (char*)neededptr - (char*)commitptr;

	size_needed = round_up_pot(size_needed, os_page_size);

	auto ret = VirtualAlloc(commitptr, size_needed, MEM_COMMIT, PAGE_READWRITE);
	assert(ret != NULL);

	return (char*)commitptr + size_needed;
}

void* decommit_pages (void* baseptr, void* neededptr, void* commitptr) {
	char* decommit = (char*)round_up_pot((uintptr_t)neededptr, os_page_size);
	uintptr_t size = (char*)commitptr - decommit; 

	auto ret = VirtualFree(decommit, size, MEM_DECOMMIT);
	assert(ret != 0);

	return decommit;
}
