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

	int calc_tex_index (int face, int variant) { // anim_frame would be selected in shader
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
	
	std::vector<BlockMeshes::Mesh> block_mesh_info;
	std::vector<int> block_meshes; // block mesh index (-1 => normal block, >=0 => index into block_mesh_info)

	BlockMeshes generate_block_meshes (json const& blocks_json);

	std::vector<BlockTile> block_tiles;

	void load_block_textures (json const& blocks_json);
};

struct RenderData {
	Camera_View		view;
	int2			window_size;

	Chunks&			chunks;
	
	WorldGenerator	const& wg;
};
