#pragma once
#include "common.hpp"
#include "engine/camera.hpp"

struct BlockTileInfo {
	int base_index;

	//int anim_frames = 1;
	//int variations = 1;

	// side is always at base_index
	int top = 0; // base_index + top to get block top tile
	int bottom = 0; // base_index + bottom to get block bottom tile

	int variants = 1;

	float2 uv_pos;
	float2 uv_size;

	//bool random_rotation = false;

	int calc_texture_index (BlockFace face) {
		int index = base_index;
		if (face == BF_POS_Z)
			index += top;
		else if (face == BF_NEG_Z)
			index += bottom;
		return index;
	}
};

struct Graphics {

	BlockTileInfo block_tile_info[BLOCK_IDS_COUNT];

};

struct RenderData {
	Camera_View		view;
	int2			window_size;
};
