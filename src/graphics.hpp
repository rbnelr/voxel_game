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

inline constexpr int2 TILEMAP_SIZE = int2(16,16);

struct BlockTile {
	int sides[6] = {}; // texture indices of the textures for the 6 faces of the box
	
	int anim_frames = 1;
	int variants = 1;

	int calc_tex_index (BlockFace face, int variant) { // anim_frame would be selected in shader
		return sides[face] + variant * anim_frames;
	}
};

struct Assets {
	
	std::vector<BlockTile> block_tiles;

	inline void load_block_textures (json const& blocks_json) {
		{ // B_NULL
			block_tiles.push_back({});
		}
		block_id id = 1;

		for (auto& kv : blocks_json["blocks"].items()) {
			auto& name = kv.key();
			auto& val = kv.value();

			//
			BlockTile bt;
			
			//int3 size = 16;
			//if (val.contains("size")) val.at("size").get_to(size);

			if (val.contains("variants")) val.at("variants").get_to(bt.variants);

			if (val.contains("tex")) {
				auto& tex = val.at("tex");

				auto get_uv = [] (json const& arr) {
					int2 tile = 0;
					if (arr.is_array() && arr.size() >= 2)
						tile = int2(arr[0].get<int>(), arr[1].get<int>());
					return tile.y * TILEMAP_SIZE.x + tile.x;
				};

				if (tex.is_array()) {
					auto idxs = get_uv(tex);
					for (auto& s : bt.sides) s = idxs;

				} else if (tex.is_object()) {
					for (auto& tex : tex.items()) {
						auto idxs = get_uv(tex.value());

						if      (tex.key() == "top")		bt.sides[BF_TOP] = idxs;
						else if (tex.key() == "bottom")		bt.sides[BF_BOTTOM] = idxs;
						else if (tex.key() == "side")		for (int s=0; s<4; ++s) bt.sides[s] = idxs;
						else if (tex.key() == "top-bottom")	bt.sides[BF_BOTTOM] = bt.sides[BF_TOP] = idxs;
					}
				}
			}

			block_tiles.push_back(bt);
		}
	}
};

struct RenderData {
	Camera_View		view;
	int2			window_size;

	Chunks&			chunks;
	
	WorldGenerator	const& wg;
};
