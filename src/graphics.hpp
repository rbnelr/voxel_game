#pragma once
#include "common.hpp"
#include "engine/camera.hpp"
#include "chunks.hpp"

enum class AttribMode {
	FLOAT,		// simply pass float to shader
	SINT,		// simply pass sint to shader
	UINT,		// simply pass uint to shader
	SINT2FLT,	// convert sint to float 
	UINT2FLT,	// convert uint to float
	SNORM,		// sint turns into [-1, 1] float (ex. from [-127, +127], note that -127 instead of -128)
	UNORM,		// uint turns into [0, 1] float (ex. from [0, 255])
};

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

struct Assets {
	BlockTileInfo block_tile_info[50];

	void load_block_textures () {
		
	}
};

struct RenderData {
	Camera_View		view;
	int2			window_size;

	Chunks&			chunks;

	Assets			const& assets;
	WorldGenerator	const& wg;
};
