#pragma once
#include "../kissmath.hpp"
#include "glshader.hpp"
#include "texture.hpp"
#include "debug_graphics.hpp"
#include "../blocks.hpp"
#include "gl.hpp"

// rotate from facing up to facing in a block face direction
static inline constexpr float3x3 face_rotation[] = {
	// BF_NEG_X  = rotate3_Z(-deg(90)) * rotate3_X(deg(90)),
	float3x3( 0, 0,-1,
	         -1, 0, 0,
			  0, 1, 0),
	// BF_POS_X  = rotate3_Z(+deg(90)) * rotate3_X(deg(90)),
	float3x3( 0, 0, 1,
			  1, 0, 0,
			  0, 1, 0),
	// BF_NEG_Y  = rotate3_Z(-deg(0)) * rotate3_X(deg(90)),
	float3x3( 1, 0, 0,
		      0, 0,-1,
		      0, 1, 0),
	// BF_POS_Y  = rotate3_Z(+deg(180)) * rotate3_X(deg(90)),
	float3x3(-1, 0, 0,
	          0, 0, 1,
	          0, 1, 0),
	// BF_NEG_Z  = rotate3_X(deg(180)),
	float3x3( 1, 0, 0,
	          0,-1, 0,
	          0, 0,-1),
	// BF_POS_Z  = float3x3::identity()
	float3x3( 1, 0, 0,
		      0, 1, 0,
		      0, 0, 1),
};

struct SkyboxGraphics {

	struct Vertex {
		float3 world_dir;

		static void bind (Attributes& a) {
			a.add<decltype(world_dir)>(0, "world_dir", sizeof(Vertex), offsetof(Vertex, world_dir));
		}
	};

	Shader shader = { "skybox" };

	Mesh<Vertex> mesh; // a inward facing cube of size 1

	SkyboxGraphics ();

	void draw ();
};

struct BlockHighlightGraphics {

	struct Vertex {
		float3	pos_model;
		lrgba	color;

		static void bind (Attributes& a) {
			a.add<decltype(pos_model)>(0, "pos_model", sizeof(Vertex), offsetof(Vertex, pos_model));
			a.add<decltype(color    )>(1, "color"    , sizeof(Vertex), offsetof(Vertex, color    ));
		}
	};

	Shader shader = { "block_highlight" };

	Mesh<Vertex> mesh;

	BlockHighlightGraphics ();

	void draw (float3 pos, BlockFace face);
};

struct ChunkMesh {
	struct Vertex {
		float3	pos_model;
		float	brightness;
		float4	uv_indx_hp; // xy: [0,1] face uv; z: texture index, w: hp_ratio [0,1]
		lrgba	color;

		static void bind (Attributes& a) {
			a.add<decltype(pos_model )>(0, "pos_model" , sizeof(Vertex), offsetof(Vertex, pos_model ));
			a.add<decltype(brightness)>(1, "brightness", sizeof(Vertex), offsetof(Vertex, brightness));
			a.add<decltype(uv_indx_hp)>(2, "uv_indx_hp", sizeof(Vertex), offsetof(Vertex, uv_indx_hp));
			a.add<decltype(color     )>(3, "color"     , sizeof(Vertex), offsetof(Vertex, color     ));
		}
	};

	std::vector<Vertex> opaque_faces;
	std::vector<Vertex> transparent_faces;

	Mesh<Vertex> opaque_mesh;
	Mesh<Vertex> transparent_mesh;
};

struct BlockTileInfo {
	int base_index;

	//int anim_frames = 1;
	//int variations = 1;

	// side is always at base_index
	int top = 0; // base_index + top to get block top tile
	int bottom = 0; // base_index + bottom to get block bottom tile

	//bool random_rotation = false;
};

// A single texture object that stores all block tiles
// could be implemented as a texture atlas but texture arrays are the better choice here
struct TileTextures {
	Texture2DArray tile_textures;
	Texture2DArray breaking_textures;

	BlockTileInfo block_tile_info[BLOCK_TYPES_COUNT];

	int2 tile_size;

	int breaking_index = 0;
	int breaking_frames_count = 1;

	float breaking_mutliplier = 1.15f;

	TileTextures ();

	inline int get_tile_base_index (block_type bt) {
		return block_tile_info[bt].base_index;
	}

	void imgui (const char* name) {
		if (!imgui_push(name, "TileTextures")) return;

		ImGui::SliderFloat("breaking_mutliplier", &breaking_mutliplier, 0, 3);

		imgui_pop();
	}
};

struct Chunks;

struct ChunkGraphics {

	Shader shader = Shader("blocks");

	Sampler2D sampler;

	TileTextures tile_textures;

	bool alpha_test = true;

	void imgui (Chunks& chunks);

	void draw_chunks (Chunks& chunks);
	void draw_chunks_transparent (Chunks& chunks);
};
