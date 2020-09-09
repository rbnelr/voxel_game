#pragma once
#include "stdafx.hpp"
#include "svo.hpp"
#include "voxel_mesher.hpp"
#include "threading.hpp"

// Backend system resposible for handling storage, changes to, and displaying of the voxel world
class Voxels {
public:
	SVO svo;

	void imgui () {
		if (!imgui_push("Voxels")) return;
		svo.imgui();
		imgui_pop();
	}

	block_id query_block (int3 pos, bool phys_read=false) {
		auto res = svo.octree_read(pos.x, pos.y, pos.z, 0);
		if (phys_read && res.lod != 0) {
			return B_NULL;
		}
		assert(res.vox.type == svo::BLOCK_ID);
		return (block_id)res.vox.value;
	}
	void set_block (int3 pos, block_id bid) {
		svo.octree_write(pos.x, pos.y, pos.z, 0, { svo::BLOCK_ID, bid });
	}
};
