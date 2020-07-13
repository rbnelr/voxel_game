#pragma once
#include "stdint.h"

// round up the number so that it is a power of two
// 0 -> 0
// 2 -> 2
// 3 -> 4
// 700 -> 1024
// not safe when next power of two is not representable (returns 0, not tested)
inline uint64_t upper_power_of_two (uint64_t v) {
	v--;
	v |= v >> 1ull;
	v |= v >> 2ull;
	v |= v >> 4ull;
	v |= v >> 8ull;
	v |= v >> 16ull;
	v |= v >> 32ull;
	v++;
	return v;
}
