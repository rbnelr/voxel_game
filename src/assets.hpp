#pragma once
#include "common.hpp"
#include "blocks.hpp"
#include "engine/renderer.hpp"

inline constexpr int2 TILEMAP_SIZE = int2(16,16);

static constexpr float3 CUBE_CORNERS[6][4] = {
	// -X face
	float3(-1,+1,-1),
	float3(-1,-1,-1),
	float3(-1,+1,+1),
	float3(-1,-1,+1),
	// +X face
	float3(+1,-1,-1),
	float3(+1,+1,-1),
	float3(+1,-1,+1),
	float3(+1,+1,+1),
	// -Y face
	float3(-1,-1,-1),
	float3(+1,-1,-1),
	float3(-1,-1,+1),
	float3(+1,-1,+1),
	// +Y face
	float3(+1,-1,-1),
	float3(-1,-1,-1),
	float3(+1,-1,+1),
	float3(-1,-1,+1),
	// -Z face
	float3(-1,+1,-1),
	float3(+1,+1,-1),
	float3(-1,-1,-1),
	float3(+1,-1,-1),
	// +Z face
	float3(-1,-1,+1),
	float3(+1,-1,+1),
	float3(-1,+1,+1),
	float3(+1,+1,+1),
};
static constexpr float3 CUBE_NORMALS[6] = {
	float3(-1, 0, 0),
	float3(+1, 0, 0),
	float3( 0,-1, 0),
	float3( 0,+1, 0),
	float3( 0, 0,-1),
	float3( 0, 0,+1),
};
constexpr float2 QUAD_UV[] = {
	float2(0,0),
	float2(1,0),
	float2(0,1),
	float2(1,1),
};
constexpr float2 QUAD_CORNERS[] = {
	float2(-0.5f,-0.5f),
	float2(+0.5f,-0.5f),
	float2(-0.5f,+0.5f),
	float2(+0.5f,+0.5f),
};

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
	#define BlockMeshInstance_FIXEDPOINT_FAC 256
	#define BlockMeshInstance_FIXEDPOINT_SHIFT 8

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
		int index;
		int length; // number of mesh slices

		float offs_strength;
	};

	// mesh (slice) data
	std::vector<MeshSlice>			slices;
	// list of block meshes, each mesh is sliced up into 'slices' where each slice is has a fixed number of vertices
	// this is needed to allow for instanced rendering of different meshes in one draw call, called 'merge instancing'
	std::vector<Mesh>				meshes;
	// LUT -> what mesh each block type uses
	// (idx == -1 => no block mesh, ie normal block, idx >= 0 => index into block_mesh_info)
	std::vector<int>				block_meshes;

	void load (json const& blocks_json);
};

struct PlayerAssets {
	float3 tool_euler_angles = float3(deg(103), deg(9), deg(-110));
	float3 tool_offset = float3(-0.485f, -0.095f, -0.2f);
	float tool_scale = 0.8f;

	Animation<AnimPosRot, AIM_LINEAR> animation = {{
		{  0 / 30.0f, float3(0.686f, 1.01f, -1.18f) / 2, AnimRotation::from_euler(deg(50), deg(-5), deg(15)) },
		{  8 / 30.0f, float3(0.624f, 1.30f, -0.94f) / 2, AnimRotation::from_euler(deg(33), deg(-8), deg(16)) },
		{ 13 / 30.0f, float3(0.397f, 1.92f, -1.16f) / 2, AnimRotation::from_euler(deg(22), deg( 1), deg(14)) },
		}};
	float anim_hit_t = 8 / 30.0f;

	float3 arm_size = float3(0.2f, 0.70f, 0.2f);

	float3x4 block_mat = (rotate3_X(deg(-39)) * rotate3_Z(deg(-17))) *
		translate(float3(-0.09f, 0.08f, 0.180f) - 0.15f) * scale(float3(0.3f));
};

struct Assets {
	
	BlockMeshes				block_meshes;
	std::vector<BlockTile>	block_tiles;

	BlockTypes				block_types;

	PlayerAssets			player;

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

// global assets because these are needed in a lot of places
inline Assets g_assets;

struct GenericVertex {
	float3	pos;
	float3	norm;
	float2	uv;
	lrgba	col;

	template <typename ATTRIBS>
	static void attributes (ATTRIBS& a) {
		int loc = 0;
		a.init(sizeof(GenericVertex));
		a.template add<AttribMode::FLOAT, decltype(pos)>(loc++, "pos",  offsetof(GenericVertex, pos ));
		a.template add<AttribMode::FLOAT, decltype(pos)>(loc++, "norm", offsetof(GenericVertex, norm));
		a.template add<AttribMode::FLOAT, decltype(uv )>(loc++, "uv",   offsetof(GenericVertex, uv  ));
		a.template add<AttribMode::FLOAT, decltype(col)>(loc++, "col",  offsetof(GenericVertex, col ));
	}
};
struct GenericVertexData {
	std::vector<GenericVertex>	vertices;
	std::vector<uint16_t>		indices;
};
struct GenericSubmesh {
	size_t		vertex_offs;
	size_t		index_offs;
	uint32_t	vertex_count;
	uint32_t	index_count;
};

struct BlockHighlightSubmeshes {
	GenericSubmesh block_highlight, face_highlight;
};
BlockHighlightSubmeshes load_block_highlight_mesh (GenericVertexData* mesh_buffer);

//std::vector<GenericSubmesh> generate_item_meshes (GenericVertexData* mesh_buffer) {
//	
//}
