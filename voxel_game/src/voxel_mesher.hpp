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
	float3		pos_model;
	float2		uv;
	int			tex_indx;

	static void bind (Attributes& a) {
		int cur = 0;
		a.add<decltype(pos_model  )>(cur++, "pos_model" ,  sizeof(VoxelVertex), offsetof(VoxelVertex, pos_model  ));
		a.add<decltype(uv         )>(cur++, "uv",          sizeof(VoxelVertex), offsetof(VoxelVertex, uv         ));
		a.add<decltype(tex_indx   )>(cur++, "tex_indx",    sizeof(VoxelVertex), offsetof(VoxelVertex, tex_indx   ));
	}
};

void remesh_chunk (svo::Chunk* chunk, svo::SVO& svo, Graphics const& g, uint64_t world_seed,
	std::vector<VoxelVertex>& opaque_mesh, std::vector<VoxelVertex>& transparent_mesh);
