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

	std::vector<Vertex> vertex_data;

	Mesh<Vertex> gpu_mesh;
};

// A single texture object that stores all block tiles
// could be implemented as a texture atlas but texture arrays are the better choice here
struct TileTexture {
	//Texture2D texture;

	int tile_base_index[BLOCK_TYPES_COUNT];

	TileTexture ();

	inline int get_tile_base_index (block_type bt) {
		return tile_base_index[bt];
	}
};

struct Chunks;

struct ChunkGraphics {

	Shader shader = Shader("blocks");

	Sampler2D sampler;

	//TileTexture tile_texture;

	Texture2D test = { "textures/earth.png" };

	void imgui () {
		sampler.imgui("Texture 1");
	}

	void draw_chunks (Chunks& chunks);
};
