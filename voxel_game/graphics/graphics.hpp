#pragma once
#include "../kissmath.hpp"
#include "../blocks.hpp"
#include "common.hpp"
#include "glshader.hpp"
#include "texture.hpp"
#include "debug_graphics.hpp"
#include "gl.hpp"
#include "../util/animation.hpp"

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

struct CrosshairGraphics {

	struct Vertex {
		float4	pos_clip;
		float2	uv;

		Vertex (float4 p, float2 uv): pos_clip{p}, uv{uv} {}

		static void bind (Attributes& a) {
			a.add<decltype(pos_clip)>(0, "pos_clip", sizeof(Vertex), offsetof(Vertex, pos_clip));
			a.add<decltype(uv      )>(1, "",         sizeof(Vertex), offsetof(Vertex, uv      ));
		}
	};

	Shader shader = { "crosshair" };
	Texture2D texture = { "textures/crosshair.png", false };

	Sampler2D sampler;

	int2 prev_window_size = -1;
	Mesh<Vertex> mesh;

	int crosshair_size = 2;

	void draw ();
};

struct GenericVertex {
	float3	pos_model;
	lrgba	color;

	static void bind (Attributes& a) {
		a.add<decltype(pos_model)>(0, "pos_model", sizeof(GenericVertex), offsetof(GenericVertex, pos_model));
		a.add<decltype(color    )>(1, "color"    , sizeof(GenericVertex), offsetof(GenericVertex, color    ));
	}
};

class Player;

struct PlayerGraphics {

	Shader shader = { "generic" };

	Animation<AnimPosRot, AIM_LINEAR> animation = {{
		{  0 / 30.0f, float3(0.686f, 1.01f, -1.18f) / 2, AnimRotation::from_euler(deg(50), deg(-5), deg(15)) },
		{  8 / 30.0f, float3(0.624f, 1.30f, -0.94f) / 2, AnimRotation::from_euler(deg(33), deg(-8), deg(16)) },
		{ 13 / 30.0f, float3(0.397f, 1.92f, -1.16f) / 2, AnimRotation::from_euler(deg(22), deg(1), deg(14)) },
	}};
	float anim_hit_t = 8 / 30.0f;

	float3 arm_size = float3(0.2f, 0.70f, 0.2f);

	Mesh<GenericVertex> fist_mesh;

	PlayerGraphics ();

	void draw (Player const& player);
};

struct ChunkMesh {
	struct Vertex {
		uint8v3	pos_model;
		uint8	brightness;
		uint8v2	uv;
		uint8	tex_indx;
		uint8	hp_ratio;

		static void bind (Attributes& a) {
			a.add    <decltype(pos_model )>(0, "pos_model" , sizeof(Vertex), offsetof(Vertex, pos_model ));
			a.add_int<decltype(brightness)>(1, "brightness", sizeof(Vertex), offsetof(Vertex, brightness));
			a.add    <decltype(uv        )>(2, "uv",         sizeof(Vertex), offsetof(Vertex, uv        ));
			a.add    <decltype(tex_indx  )>(3, "tex_indx",   sizeof(Vertex), offsetof(Vertex, tex_indx  ));
			a.add    <decltype(hp_ratio  )>(4, "hp_ratio",   sizeof(Vertex), offsetof(Vertex, hp_ratio  ), true);
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

extern bool _use_potatomode;
class Chunks;

struct ChunkGraphics {

	Shader shader = Shader("blocks");

	Sampler2D sampler;

	TileTextures tile_textures;

	bool alpha_test = !_use_potatomode;

	void imgui (Chunks& chunks);

	void draw_chunks (Chunks const& chunks);
	void draw_chunks_transparent (Chunks const& chunks);
};

class World;
struct SelectedBlock;

class Graphics {
public:
	CommonUniforms			common_uniforms;

	ChunkGraphics			chunk_graphics;
	PlayerGraphics			player;

	BlockHighlightGraphics	block_highlight;

	CrosshairGraphics		crosshair;
	SkyboxGraphics			skybox;

	void imgui (Chunks& chunks) {
		if (ImGui::CollapsingHeader("Graphics")) {
			common_uniforms.imgui();

			chunk_graphics.imgui(chunks);

			ImGui::Separator();
		}
	}

	void draw (World const& world, Camera_View const& view, Camera_View const& player_view, bool activate_flycam, SelectedBlock highlighted_block);
};
