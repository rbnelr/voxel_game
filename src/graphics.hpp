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

// Vertex for rendering chunks via merge instancing
// one vertex is expanded to a set (6 tris) of block mesh data
struct BlockMeshInstance {
	uint8v3		pos; // pos in chunk
	uint8_t		meshid; // index for merge instancing, this is used to index block meshes
	uint16_t	texid; // texture array id based on block id

	template <typename ATTRIBS>
	static void attributes (ATTRIBS& a) {
		int loc = 0;
		a.init(sizeof(BlockMeshInstance), true);
		a.template add<AttribMode::UINT2FLT, decltype(pos   )>(loc++, "pos"   , offsetof(BlockMeshInstance, pos   ));
		a.template add<AttribMode::UINT,     decltype(meshid)>(loc++, "meshid", offsetof(BlockMeshInstance, meshid));
		a.template add<AttribMode::UINT2FLT, decltype(texid )>(loc++, "texid" , offsetof(BlockMeshInstance, texid ));
	}
};
// Vertex for block meshes which are used when rendering chunks via merge instancing
struct BlockMeshVertex {
	// all as float4 to avoid std140 layout problems
	float4		pos;
	float4		normal;
	float4		uv;

	//template <typename ATTRIBS>
	//static void attributes (ATTRIBS& a) {
	//	int loc = 0;
	//	a.init(sizeof(BlockMeshVertex));
	//	a.template add<AttribMode::FLOAT, decltype(pos   )>(loc++, "pos"   , offsetof(BlockMeshVertex, pos   ));
	//	a.template add<AttribMode::FLOAT, decltype(normal)>(loc++, "normal", offsetof(BlockMeshVertex, normal));
	//	a.template add<AttribMode::FLOAT, decltype(uv    )>(loc++, "uv"    , offsetof(BlockMeshVertex, uv    ));
	//}
};

struct BlockMeshes {
	static constexpr int MERGE_INSTANCE_FACTOR = 6; // how many vertices are emitted per input vertex (how big is one slice of block mesh)

	struct Mesh {
		int offset;
		int length; // number of mesh slices
	};
	struct MeshSlice {
		BlockMeshVertex vertices[MERGE_INSTANCE_FACTOR];
	};

	std::vector<MeshSlice>	slices;
};

struct Assets {
	
	std::vector<BlockMeshes::Mesh> block_meshes;

	BlockMeshes generate_block_meshes (json const& blocks_json) {
		BlockMeshes bm;

		static constexpr float3 pos[6][4] = {
			{ {0,1,0}, {0,0,0}, {0,0,1}, {0,1,1} },
			{ {1,0,0}, {1,1,0}, {1,1,1}, {1,0,1} },
			{ {0,0,0}, {1,0,0}, {1,0,1}, {0,0,1} },
			{ {1,1,0}, {0,1,0}, {0,1,1}, {1,1,1} },
			{ {0,1,0}, {1,1,0}, {1,0,0}, {0,0,0} },
			{ {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1} } };
		static constexpr float3 normals[6] = { {-1,0,0}, {+1,0,0}, {0,-1,0}, {0,+1,0}, {0,0,-1}, {0,0,+1} };
		static constexpr float2 uv[4] = { {0,1}, {1,1}, {1,0}, {0,0} };

		static constexpr int indices[6] = {
			0,1,3, 3,1,2,
			// 1,2,0, 0,2,3,
		};

		bm.slices.resize(6);

		for (int face=0; face<6; ++face) {

			for (int vert=0; vert<6; ++vert) {
				int idx = indices[vert];

				auto& v = bm.slices[face].vertices[vert];
				v.pos		= float4(pos[face][idx], 1.0f);
				v.normal	= float4(normals[face], 1.0f);
				v.uv		= float4(uv[idx], 0.0f, 1.0f);
			}

			block_meshes.push_back({ face, 1 });
		}

		return bm;
	}

	std::vector<BlockTile> block_tiles;

	void load_block_textures (json const& blocks_json) {
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
