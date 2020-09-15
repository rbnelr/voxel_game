#pragma once
#include "stdafx.hpp"
#include "../blocks.hpp"
#include "../items.hpp"
#include "../util/animation.hpp"
#include "atlas.hpp"
#include "../time_of_day.hpp"
#include "common.hpp"
#include "raytracer.hpp"

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

//struct ChunkMesh {
//
//	//std::vector<Vertex> opaque_faces;
//	//std::vector<Vertex> transparent_faces;
//
//	Mesh<Vertex> opaque_mesh;
//	Mesh<Vertex> transparent_mesh;
//};

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

class Voxels;

struct VoxelGraphics {

	// empty vao because I don't use vertex arrays here
	gl::Vao vao;
	
	Shader shader = Shader("blocks", { FOG_UNIFORMS });

	void draw (Voxels& voxels, bool debug_frustrum_culling, TileTextures const& tile_textures, Sampler const& sampler);
	void draw_transparent (Voxels& voxels, TileTextures const& tile_textures, Sampler const& sampler);
};

class World;
struct SelectedBlock;

struct FogUniforms {
	float3 sky_col;
	float _pad0;
	float3 horiz_col;
	float _pad1;
	float3 ambient_col;

	float coeff;

	static constexpr void check_layout (SharedUniformsLayoutChecker& c) {
		c.member<decltype(sky_col    )>(offsetof(FogUniforms, sky_col    ));
		c.member<decltype(horiz_col  )>(offsetof(FogUniforms, horiz_col  ));
		c.member<decltype(ambient_col)>(offsetof(FogUniforms, ambient_col));
		c.member<decltype(coeff      )>(offsetof(FogUniforms, coeff      ));
	}
};
struct Fog {
	float fog_base_coeff = 0.85f; // div by max view dist defined somewhere else maybe dependent on chunk rendering distance
	bool enable = false;

	SharedUniforms<FogUniforms> fog_uniforms = FOG_UNIFORMS;

	void imgui () {

		ImGui::DragFloat("fog_base_coeff", &fog_base_coeff, 0.05f);

		ImGui::Checkbox("fog_enable", &enable);
	}

	void set (float max_view_dist, SkyColors colors) {
		FogUniforms f;
		f.sky_col = colors.sky_col;
		f.horiz_col = colors.horiz_col;
		f.ambient_col = colors.ambient_col;
		f.coeff = fog_base_coeff;

		if (enable)
			f.coeff /= max_view_dist;
		else
			f.coeff = 0;
		fog_uniforms.set(f);
	}
};

// framebuffer for rendering at different resolution and to make sure we get float depth buffer
struct Framebuffer {
	// https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/

	GLuint color	= 0;
	GLuint depth	= 0;
	GLuint fbo		= 0;

	int2 size = 0;
	float renderscale = 1.0f;

	bool nearest = false;

	void imgui () {
		if (!imgui_push("Framebuffer")) return;

		ImGui::SliderFloat("renderscale", &renderscale, 0.02f, 2.0f);

		ImGui::SameLine();
		ImGui::Text("= %4d x %4d px", size.x, size.y);

		ImGui::Checkbox("blit nearest", &nearest);

		imgui_pop();
	}

	void update () {
		auto old_size = size;
		size = max(1, roundi((float2)input.window_size * renderscale));

		if (!equal(old_size, size)) {
			// delete old
			glDeleteTextures(1, &color);
			glDeleteTextures(1, &depth);
			glDeleteFramebuffers(1, &fbo);

			// create new (textures created with glTexStorage2D cannot be resized), shouldn't cause noticable lag i think
			glGenTextures(1, &color);
			glBindTexture(GL_TEXTURE_2D, color);
			glTexStorage2D(GL_TEXTURE_2D, 1, GL_SRGB8_ALPHA8, size.x, size.y);
			glBindTexture(GL_TEXTURE_2D, 0);

			glGenTextures(1, &depth);
			glBindTexture(GL_TEXTURE_2D, depth);
			glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT32F, size.x, size.y);
			glBindTexture(GL_TEXTURE_2D, 0);

			glGenFramebuffers(1, &fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);

			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE) {
				fprintf(stderr, "glCheckFramebufferStatus: %x\n", status);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
	}

	void blit () {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // default FBO

		glBlitFramebuffer(
			0, 0, size.x, size.y,
			0, 0, input.window_size.x, input.window_size.y,
			GL_COLOR_BUFFER_BIT, nearest ? GL_NEAREST : GL_LINEAR);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
};

extern int frame_counter;

class Graphics {
public:
	Framebuffer				framebuffer;

	CommonUniforms			common_uniforms;
	Sampler					sampler = Sampler(gl::Enum::NEAREST, gl::Enum::LINEAR_MIPMAP_LINEAR, gl::Enum::REPEAT);

	TileTextures			tile_textures;

	VoxelGraphics			voxel_graphics;
	PlayerGraphics			player;

	BlockHighlightGraphics	block_highlight;

	GuiGraphics				gui;
	SkyboxGraphics			skybox;

	Fog						fog;

	Raytracer				raytracer;

	bool debug_frustrum_culling = false;
	bool debug_block_light = false;

	//void frustrum_cull_chunks (Chunks& chunks, Camera_View const& view);

	void imgui () {
		if (frame_counter == 30) {
			//raytracer.regen_data(chunks);
		}

		if (ImGui::CollapsingHeader("Graphics", ImGuiTreeNodeFlags_DefaultOpen)) {
			framebuffer.imgui();

			shaders->imgui();

			common_uniforms.imgui();
			sampler.imgui("sampler");

			fog.imgui();
			player.imgui();
			tile_textures.imgui("tile_textures");

			ImGui::Checkbox("debug_frustrum_culling", &debug_frustrum_culling);
			ImGui::Checkbox("debug_block_light", &debug_block_light);

			ImGui::Separator();

			raytracer.imgui();
		}
	}

	void draw (World& world, Camera_View const& view, Camera_View const& player_view, bool activate_flycam, bool creative_mode, SelectedBlock highlighted_block);
};
