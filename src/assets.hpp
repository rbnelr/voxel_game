#pragma once
#include "common.hpp"
#include "blocks.hpp"

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

	int calc_tex_index (int face, int variant) const { // anim_frame would be selected in shader
		return sides[face] + variant * anim_frames;
	}
};

// Vertex for rendering chunks via merge instancing
// one vertex is expanded to a quad (6 vert) of block mesh data
struct BlockMeshInstance {
	static constexpr int FIXEDPOINT_FAC = 256;

	int16_t		posx, posy, posz; // pos in chunk
	uint16_t	meshid; // index for merge instancing, this is used to index block meshes
	uint16_t	texid; // texture array id based on block id

	template <typename ATTRIBS>
	static void attributes (ATTRIBS& a) {
		int loc = 0;
		a.init(sizeof(BlockMeshInstance), true);
		a.template addv<AttribMode::SINT2FLT, decltype(posx), 3>(loc++, "pos"   , offsetof(BlockMeshInstance, posx)); // fixed point
		a.template add <AttribMode::UINT,     decltype(meshid) >(loc++, "meshid", offsetof(BlockMeshInstance, meshid));
		a.template add <AttribMode::UINT2FLT, decltype(texid ) >(loc++, "texid" , offsetof(BlockMeshInstance, texid ));
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

	// A mesh slice which is a fixed number of vertices
	struct MeshSlice {
		BlockMeshVertex vertices[MERGE_INSTANCE_FACTOR];
	};

	// A mesh made up of a variable number of slices
	struct Mesh {
		int offset;
		int length; // number of mesh slices
	};

	// mesh (slice) data
	std::vector<MeshSlice>			slices;
	// meshes
	std::vector<Mesh>				meshes;
	// block_id -> mesh lookup
	std::vector<int>				block_meshes; // block mesh index (-1 => normal block, >=0 => index into block_mesh_info)

	void load (json const& blocks_json);
};

struct Assets {
	
	BlockMeshes				block_meshes;
	std::vector<BlockTile>	block_tiles;

	BlockTypes				block_types;

	void load_block_types (json const& blocks_json);
	void load_block_tiles (json const& blocks_json);

	static Assets load () {
		ZoneScoped;
		Assets a;

		json blocks_json = load_json("blocks.json");

		a.load_block_types(blocks_json);
		a.block_meshes.load(blocks_json);
		a.load_block_tiles(blocks_json);

		return a;
	}
};

inline Assets g_assets;
