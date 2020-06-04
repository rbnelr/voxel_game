#pragma once
#include "../kissmath.hpp"
#include "../blocks.hpp"
#include "../items.hpp"
#include "../util/animation.hpp"
#include "atlas.hpp"
#include "../time_of_day.hpp"
#include "renderer.hpp"
#include "camera.hpp"
#include "input.hpp"

//// Frame uniforms
struct ViewUniforms {
	struct Uniforms {
		float4x4 world_to_cam;
		float4x4 cam_to_world;
		float4x4 cam_to_clip;
		float4x4 clip_to_cam;
		float4x4 world_to_clip;
		float    clip_near;
		float    clip_far;
		float2   viewport_size;

		static constexpr void check_layout (vk::UBOLayoutCheck& c) {
			c.member<decltype(world_to_cam )>(offsetof(Uniforms, world_to_cam ));
			c.member<decltype(cam_to_world )>(offsetof(Uniforms, cam_to_world ));
			c.member<decltype(cam_to_clip  )>(offsetof(Uniforms, cam_to_clip  ));
			c.member<decltype(clip_to_cam  )>(offsetof(Uniforms, clip_to_cam  ));
			c.member<decltype(world_to_clip)>(offsetof(Uniforms, world_to_clip));
			c.member<decltype(clip_near    )>(offsetof(Uniforms, clip_near    ));
			c.member<decltype(clip_far     )>(offsetof(Uniforms, clip_far     ));
			c.member<decltype(viewport_size)>(offsetof(Uniforms, viewport_size));
		}
	};
	
	void set (Camera_View const& view) {
		Uniforms u = {}; // zero padding
		u.world_to_cam = (float4x4)view.world_to_cam;
		u.cam_to_world = (float4x4)view.cam_to_world;
		u.cam_to_clip = view.cam_to_clip;
		u.clip_to_cam = view.clip_to_cam;
		u.world_to_clip = view.cam_to_clip * (float4x4)view.world_to_cam;
		u.clip_near = view.clip_near;
		u.clip_far = view.clip_far;
		u.viewport_size = (float2)input.window_size;
		//uniform_buffer.set(u);
	}
};

struct DebugUniforms {
	struct Uniforms {
		float2 cursor_pos;
		static constexpr void check_layout (vk::UBOLayoutCheck& c) {
			c.member<decltype(cursor_pos)>(offsetof(Uniforms, cursor_pos));
		}
	};

	void set () {
		Uniforms u = {}; // zero padding
		u.cursor_pos = input.cursor_pos;
		//u.wireframe = dbg_wireframe ? 1 | (wireframe_shaded ? 2:0) | (wireframe_colored ? 4:0) : 0;
		//uniform_buffer.set(u);
	}
};

struct FogUniforms {

	float fog_base_coeff = 0.85f; // div by max view dist defined somewhere else maybe dependent on chunk rendering distance
	bool enable = false;

	//SharedUniforms<FogUniforms> fog_uniforms = FOG_UNIFORMS;

	void imgui () {

		ImGui::DragFloat("fog_base_coeff", &fog_base_coeff, 0.05f);

		ImGui::Checkbox("fog_enable", &enable);
	}

	struct Uniforms {
		float3 sky_col;
		float _pad0;
		float3 horiz_col;
		float _pad1;
		float3 ambient_col;

		float coeff;

		static constexpr void check_layout (vk::UBOLayoutCheck& c) {
			c.member<decltype(sky_col    )>(offsetof(Uniforms, sky_col    ));
			c.member<decltype(horiz_col  )>(offsetof(Uniforms, horiz_col  ));
			c.member<decltype(ambient_col)>(offsetof(Uniforms, ambient_col));
			c.member<decltype(coeff      )>(offsetof(Uniforms, coeff      ));
		}
	};
	void set (float max_view_dist, SkyColors colors) {
		Uniforms u;
		u.sky_col = colors.sky_col;
		u.horiz_col = colors.horiz_col;
		u.ambient_col = colors.ambient_col;
		u.coeff = fog_base_coeff;

		if (enable)
			u.coeff /= max_view_dist;
		else
			u.coeff = 0;
		//uniform_buffer.set(u);
	}
};

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
struct TileTextures;

struct BlockVertex {
	float3 pos;
	float3 normal;
	float2 uv;

	BlockVertex () {}
	constexpr BlockVertex (float3 p, float3 norm, float2 uv): pos{p}, normal{norm}, uv{uv} {}
};
#define QUAD(a,b,c,d) \
	b, c, a, \
	a, c, d

static constexpr BlockVertex block_data[6*6] {
	QUAD(
		BlockVertex( float3(-1,+1,-1), float3(-1,0,0), float2(0,0) ),
		BlockVertex( float3(-1,-1,-1), float3(-1,0,0), float2(1,0) ),
		BlockVertex( float3(-1,-1,+1), float3(-1,0,0), float2(1,1) ),
		BlockVertex( float3(-1,+1,+1), float3(-1,0,0), float2(0,1) )
	),								 
	QUAD(							 
		BlockVertex( float3(+1,-1,-1), float3(+1,0,0), float2(0,0) ),
		BlockVertex( float3(+1,+1,-1), float3(+1,0,0), float2(1,0) ),
		BlockVertex( float3(+1,+1,+1), float3(+1,0,0), float2(1,1) ),
		BlockVertex( float3(+1,-1,+1), float3(+1,0,0), float2(0,1) )
	),	
	QUAD(	
		BlockVertex( float3(-1,-1,-1), float3(0,-1,0), float2(0,0) ),
		BlockVertex( float3(+1,-1,-1), float3(0,-1,0), float2(1,0) ),
		BlockVertex( float3(+1,-1,+1), float3(0,-1,0), float2(1,1) ),
		BlockVertex( float3(-1,-1,+1), float3(0,-1,0), float2(0,1) )
	),	
	QUAD(	
		BlockVertex( float3(+1,+1,-1), float3(0,+1,0), float2(0,0) ),
		BlockVertex( float3(-1,+1,-1), float3(0,+1,0), float2(1,0) ),
		BlockVertex( float3(-1,+1,+1), float3(0,+1,0), float2(1,1) ),
		BlockVertex( float3(+1,+1,+1), float3(0,+1,0), float2(0,1) )
	),	
	QUAD(	
		BlockVertex( float3(-1,+1,-1), float3(0,0,-1), float2(0,0) ),
		BlockVertex( float3(+1,+1,-1), float3(0,0,-1), float2(1,0) ),
		BlockVertex( float3(+1,-1,-1), float3(0,0,-1), float2(1,1) ),
		BlockVertex( float3(-1,-1,-1), float3(0,0,-1), float2(0,1) )
	),	
	QUAD(	
		BlockVertex( float3(-1,-1,+1), float3(0,0,+1), float2(0,0) ),
		BlockVertex( float3(+1,-1,+1), float3(0,0,+1), float2(1,0) ),
		BlockVertex( float3(+1,+1,+1), float3(0,0,+1), float2(1,1) ),
		BlockVertex( float3(-1,+1,+1), float3(0,0,+1), float2(0,1) )
	),
};

#undef QUAD

struct GuiGraphics {

	struct Vertex {
		float2	pos_px;
		float2	uv;
		float4	col;

		static void bind (Attributes& a) {
			a.add<decltype(pos_px)>(0, "pos_px", sizeof(Vertex), offsetof(Vertex, pos_px));
			a.add<decltype(uv    )>(1, "uv",     sizeof(Vertex), offsetof(Vertex, uv    ));
			a.add<decltype(col   )>(2, "col",    sizeof(Vertex), offsetof(Vertex, col   ));
		}
	};
	struct ItemsVertex {
		float2	pos_px;
		float2	uv;
		float	tex_indx;

		static void bind (Attributes& a) {
			a.add<decltype(pos_px  )>(0, "pos_px",   sizeof(ItemsVertex), offsetof(ItemsVertex, pos_px  ));
			a.add<decltype(uv      )>(1, "uv",       sizeof(ItemsVertex), offsetof(ItemsVertex, uv      ));
			a.add<decltype(tex_indx)>(2, "tex_indx", sizeof(ItemsVertex), offsetof(ItemsVertex, tex_indx));
		}
	};
	struct BlockItemVertex {
		float3	pos;
		float3	normal;
		float2	uv;
		float	tex_indx;

		static void bind (Attributes& a) {
			a.add<decltype(pos     )>(0, "pos",      sizeof(BlockItemVertex), offsetof(BlockItemVertex, pos     ));
			a.add<decltype(normal  )>(1, "normal",   sizeof(BlockItemVertex), offsetof(BlockItemVertex, normal  ));
			a.add<decltype(uv      )>(2, "uv",       sizeof(BlockItemVertex), offsetof(BlockItemVertex, uv      ));
			a.add<decltype(tex_indx)>(3, "tex_indx", sizeof(BlockItemVertex), offsetof(BlockItemVertex, tex_indx));
		}
	};

	Shader shader = { "gui" };
	Shader shader_item = { "gui_item" };
	Shader shader_item_block = { "gui_item_block" };

	AtlasedTexture crosshair			= { "textures/crosshair.png" };
	AtlasedTexture quickbar				= { "textures/quickbar.png" };
	AtlasedTexture quickbar_selected	= { "textures/quickbar_selected.png" };

	BlockVertex gui_block_mesh[6*6];

	GuiGraphics () {
		float3x3 mat = rotate3_X(deg(45)) * rotate3_Y(deg(45)) * rotate3_X(deg(-90));

		for (int j=0; j<6; ++j) {
			for (int i=0; i<6; ++i) {
				auto p = block_data[j*6+i];
				p.pos = (mat * p.pos) * (0.5f / 1.70710671f);
				p.normal = mat * p.normal;
				gui_block_mesh[j*6+i] = p;
			}
		}
	}

	Texture2D gui_atlas = load_texture_atlas<srgba8>({ &crosshair, &quickbar, &quickbar_selected }, 64, srgba8(0), 1, false);

	std::vector<Vertex> vertices;
	Mesh<Vertex> mesh;

	std::vector<ItemsVertex> items_vertices;
	Mesh<ItemsVertex> items_mesh;

	std::vector<BlockItemVertex> blocks_vertices;
	Mesh<BlockItemVertex> blocks_mesh;

	float gui_scale = 4; // pixel multiplier
	float crosshair_scale = .5f;

	void draw_texture (AtlasedTexture const& tex, float2 pos_px, float2 size_px, lrgba col=1);
	void draw_color_quad (float2 pos_px, float2 size_px, lrgba col);

	void draw_item_tile (item_id id, float2 pos_px, float2 size_px, TileTextures const& tile_textures);

	float2 get_quickbar_slot_center (int slot_index);

	void draw_crosshair ();
	void draw_quickbar_slot (AtlasedTexture tex, int index);
	void draw_quickbar_item (item_id id, int index, TileTextures const& tile_textures);
	void draw_quickbar (Player const& player, TileTextures const& tile_textures);

	void draw (Player const& player, TileTextures const& tile_textures, Sampler const& sampler);
};

struct GenericVertex {
	float3	pos_model;
	float3	normal;
	lrgba	color;

	static void bind (Attributes& a) {
		a.add<decltype(pos_model)>(0, "pos_model", sizeof(GenericVertex), offsetof(GenericVertex, pos_model));
		a.add<decltype(normal   )>(1, "normal",    sizeof(GenericVertex), offsetof(GenericVertex, normal   ));
		a.add<decltype(color    )>(2, "color"    , sizeof(GenericVertex), offsetof(GenericVertex, color    ));
	}
};

struct PlayerGraphics {

	struct BlockVertex {
		float3	pos_model;
		float3	normal;
		float2	uv;
		float	tex_indx;

		static void bind (Attributes& a) {
			a.add<decltype(pos_model)>(0, "pos_model", sizeof(BlockVertex), offsetof(BlockVertex, pos_model));
			a.add<decltype(normal   )>(1, "normal",    sizeof(BlockVertex), offsetof(BlockVertex, normal   ));
			a.add<decltype(uv       )>(2, "uv",        sizeof(BlockVertex), offsetof(BlockVertex, uv       ));
			a.add<decltype(tex_indx )>(3, "tex_indx",  sizeof(BlockVertex), offsetof(BlockVertex, tex_indx ));
		}
	};

	Shader shader = Shader("generic");
	Shader shader_item = Shader("held_item");

	float3 tool_euler_angles = float3(deg(103), deg(9), deg(-110));
	float3 tool_offset = float3(-0.485f, -0.095f, -0.2f);
	float tool_scale = 0.8f;

	void imgui () {
		ImGui::SliderAngle("tool_euler_X", &tool_euler_angles.x, -180, +180);
		ImGui::SliderAngle("tool_euler_Y", &tool_euler_angles.y, -180, +180);
		ImGui::SliderAngle("tool_euler_Z", &tool_euler_angles.z, -180, +180);
		ImGui::DragFloat3("tool_offset", &tool_offset.x, 0.005f);
		ImGui::DragFloat("tool_scale", &tool_scale, 0.005f);
	}

	Animation<AnimPosRot, AIM_LINEAR> animation = {{
		{  0 / 30.0f, float3(0.686f, 1.01f, -1.18f) / 2, AnimRotation::from_euler(deg(50), deg(-5), deg(15)) },
		{  8 / 30.0f, float3(0.624f, 1.30f, -0.94f) / 2, AnimRotation::from_euler(deg(33), deg(-8), deg(16)) },
		{ 13 / 30.0f, float3(0.397f, 1.92f, -1.16f) / 2, AnimRotation::from_euler(deg(22), deg( 1), deg(14)) },
	}};
	float anim_hit_t = 8 / 30.0f;

	float3 arm_size = float3(0.2f, 0.70f, 0.2f);

	Mesh<GenericVertex> fist_mesh;
	Mesh<BlockVertex> block_mesh;

	PlayerGraphics ();

	void draw (Player const& player, TileTextures const& tile_textures, Sampler const& sampler);
};

struct ChunkMesh {
	struct Vertex {
		float3	pos_model;
		float2	uv;
		uint8	tex_indx;
		uint8	block_light;
		uint8	sky_light;
		uint8	hp;

		static void bind (Attributes& a) {
			int cur = 0;
			a.add    <decltype(pos_model  )>(cur++, "pos_model" ,  sizeof(Vertex), offsetof(Vertex, pos_model  ));
			a.add    <decltype(uv         )>(cur++, "uv",          sizeof(Vertex), offsetof(Vertex, uv         ));
			a.add_int<decltype(tex_indx   )>(cur++, "tex_indx",    sizeof(Vertex), offsetof(Vertex, tex_indx   ));
			a.add    <decltype(block_light)>(cur++, "block_light", sizeof(Vertex), offsetof(Vertex, block_light), true);
			a.add    <decltype(sky_light  )>(cur++, "sky_light",   sizeof(Vertex), offsetof(Vertex, sky_light  ), true);
			a.add    <decltype(hp         )>(cur++, "hp",          sizeof(Vertex), offsetof(Vertex, hp         ), true);
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

struct ItemMeshes {

	struct Vertex {
		float3	pos_model;
		float3	normal;
		float2	uv;
		float	tex_indx;

		static void bind (Attributes& a) {
			a.add<decltype(pos_model)>(0, "pos_model", sizeof(Vertex), offsetof(Vertex, pos_model));
			a.add<decltype(normal   )>(1, "normal",    sizeof(Vertex), offsetof(Vertex, normal   ));
			a.add<decltype(uv       )>(2, "uv",        sizeof(Vertex), offsetof(Vertex, uv       ));
			a.add<decltype(tex_indx )>(3, "tex_indx",  sizeof(Vertex), offsetof(Vertex, tex_indx ));
		}
	};

	Mesh<Vertex> meshes;

	struct ItemMesh {
		unsigned offset;
		unsigned size;
	};

	ItemMesh item_meshes[ITEM_IDS_COUNT - MAX_BLOCK_ID];

	void generate (Image<srgba8>* images, int count, int* item_tiles);
};

struct BlockMeshInfo {
	int	offset;
	int	size;
};
struct BlockMeshVertex {
	float3	pos_model;
	float2	uv;
};

// A single texture object that stores all block tiles
// could be implemented as a texture atlas but texture arrays are the better choice here
struct TileTextures {
	Texture2DArray tile_textures;
	Texture2DArray breaking_textures;

	BlockTileInfo block_tile_info[BLOCK_IDS_COUNT];
	int item_tile[ITEM_IDS_COUNT - MAX_BLOCK_ID];

	int2 tile_size;

	std::vector<BlockMeshVertex> block_meshes;
	BlockMeshInfo block_meshes_info[BLOCK_IDS_COUNT];

	ItemMeshes item_meshes;

	int breaking_index = 0;
	int breaking_frames_count = 1;

	float breaking_mutliplier = 1.15f;

	TileTextures ();
	void load_block_meshes ();

	inline int get_tile_base_index (block_id id) {
		return block_tile_info[id].base_index;
	}

	void imgui (const char* name) {
		if (!imgui_push(name, "TileTextures")) return;

		ImGui::SliderFloat("breaking_mutliplier", &breaking_mutliplier, 0, 3);

		imgui_pop();
	}
};

class Chunks;

struct ChunkGraphics {

	Shader shader = Shader("blocks"); // { FOG_UNIFORMS }

	bool alpha_test = true;

	void imgui (Chunks& chunks);

	void draw_chunks (Chunks const& chunks, bool debug_frustrum_culling, uint8 sky_light_reduce, TileTextures const& tile_textures, Sampler const& sampler);
	void draw_chunks_transparent (Chunks const& chunks, TileTextures const& tile_textures, Sampler const& sampler);
};

class World;
struct SelectedBlock;

extern int frame_counter;

#define PIPELINE_SKYBOX { OS_DEPTH_CLAMP, true }, { OS_DEPTH_RANGE, 1.0f, 1.0f }

class Graphics {
public:
	ViewUniforms			view;
	DebugUniforms			debug;
	FogUniforms				fog;

	Graphics () {

	}

	Sampler					sampler;// = Sampler(gl::Enum::NEAREST, gl::Enum::LINEAR_MIPMAP_LINEAR, gl::Enum::REPEAT);

	TileTextures			tile_textures;

	ChunkGraphics			chunk_graphics;
	PlayerGraphics			player;

	BlockHighlightGraphics	block_highlight;

	GuiGraphics				gui;

	//Pipeline				skybox_pipeline		= Pipeline("skybox", { common_uniforms_layout }, { PIPELINE_SKYBOX });
	
	bool debug_frustrum_culling = false;
	bool debug_block_light = false;

	void frustrum_cull_chunks (Chunks& chunks, Camera_View const& view);

	void imgui (Chunks& chunks) {
		
		if (ImGui::CollapsingHeader("Graphics", ImGuiTreeNodeFlags_DefaultOpen)) {
			//shaders->imgui();
			
			//sampler.imgui("sampler");
		
			fog.imgui();
			player.imgui();
			chunk_graphics.imgui(chunks);
			tile_textures.imgui("tile_textures");
		
			ImGui::Checkbox("debug_frustrum_culling", &debug_frustrum_culling);
			ImGui::Checkbox("debug_block_light", &debug_block_light);
		
			ImGui::Separator();
		}
	}

	void draw (World& world, Camera_View const& view, Camera_View const& player_view, bool activate_flycam, SelectedBlock highlighted_block);
};
