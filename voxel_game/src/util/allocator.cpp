#include "allocator.hpp"
#include "clean_windows_h.hpp"

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

#if DBG_MEMSET
	memset(ptr, DBG_MEMSET_VAL, size);
#endif
}

void decommit_pages (void* ptr, uintptr_t size) {
	auto ret = VirtualFree(ptr, size, MEM_DECOMMIT);
	assert(ret != 0);
}

#define ONES (~0ull) //0xffffffffffffffffull

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

int Bitset::bitscan_forward_1 (int start) {
	int count = (int)bits.size();
	int i = start;
	while (i < count) {
		if (bits[i] != 0ull)
			return (i << 6) + _bsf_1(bits[i]);
		++i;
	}
	return count << 6;
}
int Bitset::bitscan_reverse_0 () {
	int count = (int)bits.size();
	int i = count -1;
	while (i >= 0) {
		if (bits[i] != ONES)
			return (i << 6) + _bsr_0(bits[i]);
		--i;
	}
	return -1;
}

int Bitset::clear_first_1 () {
	// get the index to return, which we already know is the lowest set bit
	int idx = first_set;

	// append to bits if needed
	if (idx == ((int)bits.size() << 6))
		bits.push_back(ONES);

	// clear bit
	assert(bits[idx >> 6] & (1ull << (idx & 0b111111u)));
	bits[idx >> 6] &= ~(1ull << (idx & 0b111111u));

	// scan for next set bit for next call, can skip all bits before first_set, since we know they are 0
	first_set = bitscan_forward_1(first_set >> 6);

	return idx;
}
void Bitset::set_bit (int idx) {
	// set bit in freeset to 1
	bits[idx >> 6] |= 1ull << (idx & 0b111111u);

	// shrink bits if there are contiguous zero ints at the end
	if ((idx >> 6) == (int)bits.size()-1) {
		while (bits.back() == ONES)
			bits.pop_back();
	}

	// keep first_set up to date
	first_set = std::min(first_set, idx);
}
