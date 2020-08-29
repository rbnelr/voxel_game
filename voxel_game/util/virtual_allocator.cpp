#include "virtual_allocator.hpp"
#include "windows.h"

uint32_t get_os_page_size () {
	SYSTEM_INFO info;
	GetSystemInfo(&info);

	return (uint32_t)info.dwPageSize;
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

uint32_t _bsf_nonzero (uint64_t val) {
	unsigned long idx;
	auto ret = _BitScanForward64(&idx, val);
	assert(ret);
	return idx;
}

// returns count*64 if no set bit found
uint32_t _bitscan (uint64_t* bits, uint32_t count, uint32_t start=0) {
	uint32_t x = start;

	while (bits[x] == 0ull) {
		x++;
		if (x == count) {
			return x << 6;
		}
	}

	return (x << 6) + _bsf_nonzero(bits[x]);
}

uint32_t Bitset::clear_first_1 (uint64_t* prev_bits) {
	// get the index to return, which we already know is the lowest set bit
	uint32_t idx = first_set;

	// append to bits if needed
	if (idx == ((uint32_t)bits.size() << 6))
		bits.push_back(0xffffffffffffffffull);

	// clear bit
	assert(bits[idx >> 6] & (1ull << (idx & 0b111111u)));

	if (prev_bits)
		*prev_bits = bits[idx >> 6];
	bits[idx >> 6] ^= 1ull << (idx & 0b111111u);

	// scan for next set bit for next call, can skip all bits before first_set, since we know they are 0
	first_set = _bitscan(bits.data(), (uint32_t)bits.size(), first_set >> 6);

	return idx;
}
void Bitset::set_bit (uint32_t idx, uint64_t* new_bits) {
	// set bit in freeset to 1
	bits[idx >> 6] |= 1ull << (idx & 0b111111u);

	if (new_bits)
		*new_bits = bits[idx >> 6];

	// shrink bits if there are contiguous zero ints at the end
	if ((idx >> 6) == (uint32_t)bits.size()-1) {
		while (bits.back() == 0xffffffffffffffffull)
			bits.pop_back();
	}

	// keep first_set up to date
	first_set = min(first_set, idx);
}
