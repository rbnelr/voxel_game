#pragma once
#include "stdafx.hpp"
#include "util/allocator.hpp"
#include "threading.hpp"
#include "graphics/graphics.hpp"

namespace svo {
	struct Chunk;
	struct SVO;
}

struct VoxelVertex {
	float3		pos;
	float3		normal;
	float2		uv;
};

struct VoxelInstance {
	int8_t		posx;
	int8_t		posy;
	int8_t		posz;
	uint8_t		scale_face; // [4bit scale][4bit face_id]
	int			tex_indx;
};

void remesh_chunk (svo::Chunk* chunk, svo::SVO& svo, Graphics const& g, uint64_t world_seed,
	std::vector<VoxelInstance>& opaque_mesh,
	std::vector<VoxelInstance>& transparent_mesh);
