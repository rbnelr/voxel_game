#pragma once
#include "common.hpp"
#include "engine/camera.hpp"
#include "chunks.hpp"

struct ChunkVertex {
	float3	pos_model;
	float2	uv;
	uint8	tex_indx;
	uint8	block_light;
	uint8	sky_light;
	uint8	hp;

	//static void bind (Attributes& a) {
	//	int cur = 0;
	//	a.add    <decltype(pos_model  )>(cur++, "pos_model" ,  sizeof(Vertex), offsetof(Vertex, pos_model  ));
	//	a.add    <decltype(uv         )>(cur++, "uv",          sizeof(Vertex), offsetof(Vertex, uv         ));
	//	a.add_int<decltype(tex_indx   )>(cur++, "tex_indx",    sizeof(Vertex), offsetof(Vertex, tex_indx   ));
	//	a.add    <decltype(block_light)>(cur++, "block_light", sizeof(Vertex), offsetof(Vertex, block_light), true);
	//	a.add    <decltype(sky_light  )>(cur++, "sky_light",   sizeof(Vertex), offsetof(Vertex, sky_light  ), true);
	//	a.add    <decltype(hp         )>(cur++, "hp",          sizeof(Vertex), offsetof(Vertex, hp         ), true);
	//}
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
	BlockTileInfo block_tile_info[BLOCK_IDS_COUNT];
};

struct RenderData {
	Camera_View		view;
	int2			window_size;

	Chunks&			chunks;

	Assets			const& assets;
	WorldGenerator	const& wg;
};
