#pragma once
#include "blocks.hpp"
#include "kissmath.hpp"

static constexpr int CHUNK_SIZE = 64;
static constexpr int CHUNK_SCALE = 6;

// Calc 3d index into flattened [CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE] array because compiler sometimes does too much math
static constexpr inline uintptr_t CHUNK_3D_INDEX (int x, int y, int z) {
	return   ((uintptr_t)z << CHUNK_SCALE*2) 
			+((uintptr_t)y << CHUNK_SCALE  )
			+((uintptr_t)x                 );
}
// i=1 -> + int3(1,0,0)  i=6 -> + int3(1,1,0) offs in 3d array etc.
static constexpr inline uintptr_t CHUNK_3D_CHILD_OFFSET (int i) {
	return   ((uintptr_t)((i&4) >> 2) << CHUNK_SCALE*2)
			+((uintptr_t)((i&2) >> 1) << CHUNK_SCALE  )
			+((uintptr_t)((i&1)     )                 );
}

namespace worldgen {
	typedef void (* generate_chunk_dll_fp)(block_id* blocks, int3 chunk_pos, int chunk_lod, uint64_t chunk_seed);
}

extern "C" {
	__declspec(dllexport) void generate_chunk_dll (block_id* blocks, int3 chunk_pos, int chunk_lod, uint64_t chunk_seed);
}
