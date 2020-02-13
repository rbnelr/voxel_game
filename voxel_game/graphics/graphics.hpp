#pragma once
#include "../kissmath.hpp"
#include "../blocks.hpp"
#include "common.hpp"
#include "shaders.hpp"
#include "texture.hpp"
#include "debug_graphics.hpp"
#include "gl.hpp"
#include "../util/animation.hpp"
#include "atlas.hpp"

constexpr SharedUniformsInfo FOG_UNIFORMS = { "Fog", 2 };

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

	Shader shader = Shader("skybox", { FOG_UNIFORMS });

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

class Player;

struct GuiGraphics {

	struct Vertex {
		float4	pos_clip;
		float2	uv;
		float4	col;

		static void bind (Attributes& a) {
			a.add<decltype(pos_clip)>(0, "pos_clip", sizeof(Vertex), offsetof(Vertex, pos_clip));
			a.add<decltype(uv      )>(1, "uv",       sizeof(Vertex), offsetof(Vertex, uv      ));
			a.add<decltype(col     )>(2, "col",      sizeof(Vertex), offsetof(Vertex, col     ));
		}
	};

	Shader shader = { "gui" };

	AtlasedTexture crosshair			= { "textures/crosshair.png" };
	AtlasedTexture quickbar				= { "textures/quickbar.png" };
	AtlasedTexture quickbar_selected	= { "textures/quickbar_selected.png" };

	Texture2D gui_atlas = load_texture_atlas<srgba8>({ &crosshair, &quickbar, &quickbar_selected }, 64, srgba8(0), 1, false);

	Sampler2D sampler;

	std::vector<Vertex> vertices;
	Mesh<Vertex> mesh;

	float gui_scale = 4; // pixel multiplier
	float crosshair_scale = .5f;

	void draw_texture (AtlasedTexture const& tex, float2 pos_px, float2 size_px, lrgba col=1);
	void draw_color_quad (float2 pos_px, float2 size_px, lrgba col);

	void draw_crosshair ();
	void draw_quickbar_slot (AtlasedTexture tex, int index);
	void draw_quickbar (Player const& player);

	void draw (Player const& player);
};

struct GenericVertex {
	float3	pos_model;
	lrgba	color;

	static void bind (Attributes& a) {
		a.add<decltype(pos_model)>(0, "pos_model", sizeof(GenericVertex), offsetof(GenericVertex, pos_model));
		a.add<decltype(color    )>(1, "color"    , sizeof(GenericVertex), offsetof(GenericVertex, color    ));
	}
};

struct PlayerGraphics {

	Shader shader = Shader("generic", { FOG_UNIFORMS });

	Animation<AnimPosRot, AIM_LINEAR> animation = {{
		{  0 / 30.0f, float3(0.686f, 1.01f, -1.18f) / 2, AnimRotation::from_euler(deg(50), deg(-5), deg(15)) },
		{  8 / 30.0f, float3(0.624f, 1.30f, -0.94f) / 2, AnimRotation::from_euler(deg(33), deg(-8), deg(16)) },
		{ 13 / 30.0f, float3(0.397f, 1.92f, -1.16f) / 2, AnimRotation::from_euler(deg(22), deg( 1), deg(14)) },
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

	//std::vector<Vertex> opaque_faces;
	//std::vector<Vertex> transparent_faces;

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

	Shader shader = Shader("blocks", { FOG_UNIFORMS });

	Sampler2D sampler;

	TileTextures tile_textures;

	bool alpha_test = !_use_potatomode;

	void imgui (Chunks& chunks);

	void draw_chunks (Chunks const& chunks, bool debug_frustrum_culling, bool debug_lod);
	void draw_chunks_transparent (Chunks const& chunks);
};

class World;
struct SelectedBlock;

struct FogUniforms {
	float3 sky_col =	srgb(121,192,255);
	float _pad0;
	float3 horiz_col =	srgb(199,211,219);
	float _pad1;
	float3 down_col =	srgb(41,49,52);

	float coeff =	0.85f; // div by max view dist defined somewhere else maybe dependent on chunk rendering distance

	static constexpr void check_layout (SharedUniformsLayoutChecker& c) {
		c.member<decltype(sky_col  )>(offsetof(FogUniforms, sky_col  ));
		c.member<decltype(horiz_col)>(offsetof(FogUniforms, horiz_col));
		c.member<decltype(down_col )>(offsetof(FogUniforms, down_col ));
		c.member<decltype(coeff    )>(offsetof(FogUniforms, coeff    ));
	}
};
struct Fog {
	FogUniforms f;
	bool enable = false;

	SharedUniforms<FogUniforms> fog_uniforms = FOG_UNIFORMS;

	void imgui () {

		imgui_ColorEdit3("sky_col", &f.sky_col.x, ImGuiColorEditFlags_DisplayHSV);
		imgui_ColorEdit3("horiz_col", &f.horiz_col.x, ImGuiColorEditFlags_DisplayHSV);
		imgui_ColorEdit3("down_col", &f.down_col.x, ImGuiColorEditFlags_DisplayHSV);

		ImGui::DragFloat("fog_base_coeff", &f.coeff, 0.05f);

		ImGui::Checkbox("fog_enable", &enable);
	}

	void set (float max_view_dist) {
		FogUniforms u = f;
		if (enable)
			u.coeff /= max_view_dist;
		else
			u.coeff = 0;
		fog_uniforms.set(u);
	}
};

class Graphics {
public:
	CommonUniforms			common_uniforms;

	ChunkGraphics			chunk_graphics;
	PlayerGraphics			player;

	BlockHighlightGraphics	block_highlight;

	GuiGraphics				gui;
	SkyboxGraphics			skybox;

	Fog						fog;

	bool debug_frustrum_culling = false;
	bool debug_lod = false;

	void frustrum_cull_chunks (Chunks& chunks, Camera_View const& view);

	void imgui (Chunks& chunks) {
		if (ImGui::CollapsingHeader("Graphics")) {
			common_uniforms.imgui();
			fog.imgui();
			chunk_graphics.imgui(chunks);

			ImGui::Checkbox("debug_frustrum_culling", &debug_frustrum_culling);
			ImGui::Checkbox("debug_lod", &debug_lod);

			ImGui::Separator();
		}
	}

	void draw (World& world, Camera_View const& view, Camera_View const& player_view, bool activate_flycam, SelectedBlock highlighted_block);
};
