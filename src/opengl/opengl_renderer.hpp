#pragma once
#include "common.hpp"
#include "engine/renderer.hpp"
#include "opengl_context.hpp"
#include "opengl_helper.hpp"
#include "opengl_shaders.hpp"
#include "gl_chunk_renderer.hpp"
#include "bloom.hpp"
#include "engine/input.hpp"

namespace gl {

// mesh has to be bound
inline void draw_submesh (GenericSubmesh submesh) {
	glDrawElementsBaseVertex(GL_TRIANGLES,
		submesh.index_count, // vertex draw count is how many indicies there are
		GL_UNSIGNED_SHORT,
		(void*)(submesh.index_offs * sizeof(uint16_t)), // index offset
		(GLint)submesh.base_vertex);
}

struct ViewUniforms {
	// forward
	float4x4 world_to_clip;
	float4x4 world_to_cam;
	float4x4 cam_to_clip;
	// inverse
	float4x4 clip_to_world;
	float4x4 clip_to_cam;
	float4x4 cam_to_world;

	float    clip_near;
	float    clip_far;
	float2   viewport_size;

	void set (Camera_View const& view) {
		memset(this, 0, sizeof(*this)); // zero padding

		world_to_clip = view.cam_to_clip * (float4x4)view.world_to_cam;
		world_to_cam = (float4x4)view.world_to_cam;
		cam_to_clip = view.cam_to_clip;

		clip_to_world = (float4x4)view.cam_to_world * view.clip_to_cam;
		clip_to_cam = view.clip_to_cam;
		cam_to_world = (float4x4)view.cam_to_world;

		clip_near = view.clip_near;
		clip_far = view.clip_far;
	}
};
struct CommonUniforms {
	ViewUniforms view;
};

class OpenglRenderer;

struct glDebugDraw {
	SERIALIZE(glDebugDraw, draw_occluded, occluded_alpha)

	VertexBuffer vbo_lines	= vertex_buffer<DebugDraw::LineVertex>("DebugDraw.vbo_lines");
	VertexBuffer vbo_tris	= vertex_buffer<DebugDraw::TriVertex> ("DebugDraw.vbo_tris");

	IndexedInstancedBuffer vbo_wire_cube = indexed_instanced_buffer<DebugDraw::PosVertex, DebugDraw::Instance>("DebugDraw.vbo_wire_cube");

	struct IndirectLineVertex {
		float4 pos; // vec4 for std430 alignment
		float4 col;

		template <typename ATTRIBS>
		static void attributes (ATTRIBS& a) {
			int loc = 0;
			a.init(sizeof(IndirectLineVertex));
			a.template add<AttribMode::FLOAT, decltype(pos)>(loc++, "pos", offsetof(IndirectLineVertex, pos));
			a.template add<AttribMode::FLOAT, decltype(col)>(loc++, "col", offsetof(IndirectLineVertex, col));
		}
	};
	struct IndirectWireCubeInstace {
		float4 pos; // vec4 for std430 alignment
		float4 size; // vec4 for std430 alignment
		float4 col;

		template <typename ATTRIBS>
		static void attributes (ATTRIBS& a) {
			a.init(sizeof(IndirectWireCubeInstace), true);
			a.template add<AttribMode::FLOAT, decltype(pos)>(1, "instance_pos", offsetof(IndirectWireCubeInstace, pos));
			a.template add<AttribMode::FLOAT, decltype(size)>(2, "instance_size", offsetof(IndirectWireCubeInstace, size));
			a.template add<AttribMode::FLOAT, decltype(col)>(3, "instance_col", offsetof(IndirectWireCubeInstace, col));
		}
	};
	struct IndirectBuffer {
		struct Lines {
			glDrawArraysIndirectCommand cmd;
			IndirectLineVertex vertices[4096];
		} lines;

		struct WireCubes {
			glDrawElementsIndirectCommand cmd;
			uint32_t _pad[3]; // padding for std430 alignment
			IndirectWireCubeInstace vertices[4096];
		} wire_cubes;
	};

	Vbo indirect_vbo = {"DebugDraw.indirect_draw"};
	Vao indirect_lines_vao     = setup_vao<IndirectLineVertex>("DebugDraw.indirect_lines", { indirect_vbo, offsetof(IndirectBuffer, lines.vertices[0]) });
	Vao indirect_wire_cube_vao = setup_instanced_vao<DebugDraw::PosVertex, IndirectWireCubeInstace>("DebugDraw.indirect_wire_cubes", 
											{ indirect_vbo, offsetof(IndirectBuffer, wire_cubes.vertices[0]) },
											vbo_wire_cube.mesh_vbo, vbo_wire_cube.mesh_ebo);

	void clear_indirect () {
		glBindBuffer(GL_ARRAY_BUFFER, indirect_vbo);

		{
			glDrawArraysIndirectCommand cmd = {};
			cmd.instanceCount = 1;
			glBufferSubData(GL_ARRAY_BUFFER, offsetof(IndirectBuffer, lines.cmd), sizeof(cmd), &cmd);
		}
		{
			glDrawElementsIndirectCommand cmd = {};
			cmd.count = ARRLEN(DebugDraw::_wire_indices);
			glBufferSubData(GL_ARRAY_BUFFER, offsetof(IndirectBuffer, wire_cubes.cmd), sizeof(cmd), &cmd);
		}
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	Shader* shad_lines;
	Shader* shad_lines_occluded;
	Shader* shad_wire_cube;

	Shader* shad_tris;

	bool draw_occluded = false;
	float occluded_alpha = 0.3f;

	void imgui () {
		ImGui::Checkbox("draw_occluded", &draw_occluded);
		ImGui::SliderFloat("occluded_alpha", &occluded_alpha, 0,1);
	}

	glDebugDraw (Shaders& shaders) {
		shad_lines			= shaders.compile("debug_lines", {{"DRAW_OCCLUDED", "0"}});
		shad_lines_occluded	= shaders.compile("debug_lines", {{"DRAW_OCCLUDED", "1"}});
		shad_tris			= shaders.compile("debug_tris");

		shad_wire_cube		= shaders.compile("debug_wire_cube", {{"DRAW_OCCLUDED", "0"}});

		glBindBuffer(GL_ARRAY_BUFFER, vbo_wire_cube.mesh_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(DebugDraw::_wire_vertices), DebugDraw::_wire_vertices, GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_wire_cube.mesh_ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(DebugDraw::_wire_indices), DebugDraw::_wire_indices, GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, indirect_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(IndirectBuffer), nullptr, GL_STREAM_DRAW);
		clear_indirect();

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void draw (OpenglRenderer& r);
};

struct BlockHighlight {
	Shader*					shad;
	BlockHighlightSubmeshes	block_highl;

	BlockHighlight (Shaders& shaders) {
		shad = shaders.compile("block_highlight");
	}
	void draw (OpenglRenderer& r, SelectedBlock& block);
};

struct GuiRenderer {
	Shader*			gui_shad;
	Sampler			gui_sampler = {"gui_sampler"};

	struct GUIVertex {
		float2 pos; // 2d because we display 2d gui
		float3 normal; // but still 3d normals for a bit of lighting on the pseudo 3d cubes for blocks
		float3 uvi; // z is tile texture index, -1 means gui texture
		
		template <typename ATTRIBS>
		static void attributes (ATTRIBS& a) {
			int loc = 0;
			a.init(sizeof(GUIVertex));
			a.template add<AttribMode::FLOAT, decltype(pos   )>(loc++, "pos"   , offsetof(GUIVertex, pos   ));
			a.template add<AttribMode::FLOAT, decltype(normal)>(loc++, "normal", offsetof(GUIVertex, normal));
			a.template add<AttribMode::FLOAT, decltype(uvi   )>(loc++, "uvi"   , offsetof(GUIVertex, uvi   ));
		}
	};

	IndexedBuffer	gui_ib	= indexed_buffer<GUIVertex>("GUIRenderer.gui_vbo");

	std::vector<GUIVertex> gui_vertex_data;
	std::vector<uint16_t> gui_index_data;

	// allocate vertices for N quads (4 each) and already fill indices for them
	GUIVertex* push_quads (int count) {
		size_t base_idx = gui_vertex_data.size();
		gui_vertex_data.resize(base_idx + 4*count);
		auto* verts = &gui_vertex_data[base_idx];

		size_t idx_offs = gui_index_data.size();
		gui_index_data.resize(idx_offs + 6*count);
		auto* indices = &gui_index_data[idx_offs];

		for (int j=0; j<count; ++j) {
			for (int i=0; i<6; ++i) {
				*indices++ = (uint16_t)(base_idx + j*4 + QUAD_INDICES[i]);
			}
		}

		return verts;
	}

	struct AtlasUVs {
		float2 pos;
		float2 size;
	};
	static constexpr AtlasUVs crosshair_uv			= { float2(   0,    0)   ,    32    };
	static constexpr AtlasUVs frame_uv				= { float2(32*1,    0) +6,    16 +4 };
	static constexpr AtlasUVs frame_highl_uv		= { float2(32*2,    0) +6,    16 +4 };
	static constexpr AtlasUVs frame_grabbed_uv		= { float2(32*2, 32*1) +6,    16 +4 };
	static constexpr AtlasUVs frame_selected_uv		= { float2(32*3,    0) +4,    16 +8 };

	int gui_scale = 4;
	bool crosshair = false;

	// render quad with pixel coords
	void draw_gui_quad (float2 const& pos, float2 const& size, AtlasUVs const& uv);
	void draw_item_quad (float2 const& pos, float2 const& size, item_id item);

	GuiRenderer (Shaders& shaders) {
		gui_shad = shaders.compile("gui");

		glSamplerParameteri(gui_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glSamplerParameteri(gui_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameteri(gui_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(gui_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glSamplerParameterf(gui_sampler, GL_TEXTURE_MAX_ANISOTROPY, 1);
	}
	void draw_gui (OpenglRenderer& r, Input& I, Game& game);

	void update_gui (Input& I, Game& game);
};

// Drawing of meshes that are displayed in first and third person for now
struct PlayerRenderer {
	Shader*			held_block_shad;
	Shader*			held_item_shad;

	Vao				dummy_vao = {"dummy_vao"};

	PlayerRenderer (Shaders& shaders) {
		held_block_shad    = shaders.compile("held_block");
		held_item_shad     = shaders.compile("held_item");
	}
	void draw (OpenglRenderer& r, Game& game);

};

class OpenglRenderer : public Renderer {
public:
	SERIALIZE(OpenglRenderer, chunk_renderer, raytracer, bloom_renderer, line_width, debug_draw, imopen)

	struct ImguiOpen {
		SERIALIZE(ImguiOpen, framebuffer, debugdraw, gui)
		bool framebuffer=true, debugdraw=true, gui=true;
	};

	virtual void deserialize (nlohmann::ordered_json const& j) { j.get_to(*this); }
	virtual void serialize (nlohmann::ordered_json& j) { j = *this; }

	OpenglContext	ctx; // make an 'opengl context' first member so opengl init happens before any other ctors (which might make opengl calls)

	StateManager	state;
	Shaders			shaders;

	CommonUniforms	common_uniforms;

	Framebuffer		framebuffer;
	bool			trigger_screenshot = false;
	bool			screenshot_hud = false;

	ChunkRenderer	chunk_renderer	= ChunkRenderer(shaders);
	PlayerRenderer	player_rederer	= PlayerRenderer(shaders);
	Raytracer		raytracer		= Raytracer(shaders);

	BlockHighlight	block_highl		= BlockHighlight(shaders);
	GuiRenderer		gui_renderer	= GuiRenderer(shaders);

	BloomRenderer	bloom_renderer	= BloomRenderer(shaders);

	Shader*			post_shad = shaders.compile("postprocess");

	Ubo				common_uniforms_ubo = {"common_ubo"};

	Sampler			tile_sampler = {"tile_sampler"};
	Sampler			normal_sampler = {"normal_sampler"};
	Sampler			bloom_sampler = {"bloom_sampler"};
	Sampler			post_sampler = {"post_sampler"};
	Sampler			normal_sampler_wrap = {"normal_sampler_wrap"};

	Ssbo			block_meshes_ssbo = {"block_meshes_ssbo"};
	Ssbo			block_tiles_ssbo = {"block_tiles_ssbo"};

	struct BreakingTile {
		int first;
		int count;
	};
	BreakingTile	damage_tiles = { 15*16 + 6, 10 };

	IndexedBuffer				mesh_data = indexed_buffer<GenericVertex>("mesh_data");
	std::vector<GenericSubmesh>	item_meshes;
	
	Texture2DArray	tile_textures	= {"tile_textures"};
	Texture2D		gui_atlas		= {"gui_atlas"};
	Texture2D		gradient		= {"gradient"};
	Texture2D		water_N_A		= {"water_N_A"};
	Texture2D		water_N_B		= {"water_N_B"};
	
	Texture2DArray	textures_A		= {"textures_A"}; // albedos
	Texture2DArray	textures_N		= {"textures_N"}; // normals, occlusion, displacement, glossy

	Texture2DArray	textures2_A		= {"textures2_A"}; // albedos
	Texture2DArray	textures2_N		= {"textures2_N"}; // normals, occlusion, displacement, glossy

	bool			wireframe = false;
	bool			wireframe_backfaces = true;
	float			line_width = 1.0f;
	glDebugDraw		debug_draw = glDebugDraw(shaders);

	virtual bool get_vsync () {
		return ctx.vsync;
	}
	virtual void set_vsync (bool state) {
		ctx.set_vsync(state);
	}

	bool load_textures (GenericVertexData& mesh_data); // can be reloaded
	bool load_static_data ();

	OpenglRenderer (GLFWwindow* window, char const* app_name): ctx{window, app_name} {

		// I never align my pixel rows
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);

		load_static_data();

		float max_aniso = 1.0f;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_aniso);

		glSamplerParameteri(tile_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glSamplerParameteri(tile_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameteri(tile_sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glSamplerParameteri(tile_sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glSamplerParameterf(tile_sampler, GL_TEXTURE_MAX_ANISOTROPY, max_aniso);

		glSamplerParameteri(post_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glSamplerParameteri(post_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(post_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(post_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glSamplerParameterf(post_sampler, GL_TEXTURE_MAX_ANISOTROPY, 1);

		glSamplerParameteri(bloom_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glSamplerParameteri(bloom_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(bloom_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(bloom_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glSamplerParameterf(bloom_sampler, GL_TEXTURE_MAX_ANISOTROPY, max_aniso); // 

		glSamplerParameteri(normal_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glSamplerParameteri(normal_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(normal_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(normal_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glSamplerParameterf(normal_sampler, GL_TEXTURE_MAX_ANISOTROPY, max_aniso);

		glSamplerParameteri(normal_sampler_wrap, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glSamplerParameteri(normal_sampler_wrap, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(normal_sampler_wrap, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glSamplerParameteri(normal_sampler_wrap, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glSamplerParameterf(normal_sampler_wrap, GL_TEXTURE_MAX_ANISOTROPY, max_aniso);
	}
	virtual ~OpenglRenderer () {}

	virtual void frame_begin (GLFWwindow* window, Input& I, kiss::ChangedFiles& changed_files);
	virtual void render_frame (GLFWwindow* window, Input& I, Game& game);

	ImguiOpen imopen;

	virtual void screenshot_imgui (Input& I) {
		trigger_screenshot = ImGui::Button("Screenshot [F8]") || I.buttons[KEY_F8].went_down;
		ImGui::SameLine();
		ImGui::Checkbox("With HUD", &screenshot_hud);
	}
	virtual void graphics_imgui (Input& I) {
		if (imgui_treenode("Framebuffer", &imopen.framebuffer)) {
			framebuffer.imgui();

			ImGui::TreePop();
		}

		if (imgui_treenode("Debug Draw", &imopen.debugdraw)) {
			ImGui::Checkbox("wireframe", &wireframe);
			ImGui::SameLine();
			ImGui::Checkbox("backfaces", &wireframe_backfaces);

			debug_draw.imgui();
			ImGui::SliderFloat("line_width", &line_width, 1.0f, 8.0f);

			ImGui::TreePop();
		}

		ImGui::Checkbox("draw_chunks", &chunk_renderer._draw_chunks);

		if (imgui_treenode("GUI", &imopen.gui)) {
			ImGui::Checkbox("crosshair", &gui_renderer.crosshair);
			ImGui::SliderInt("gui_scale", &gui_renderer.gui_scale, 1, 16);

			ImGui::TreePop();
		}

		raytracer.imgui();
		bloom_renderer.imgui();
	}

	virtual void chunk_renderer_imgui (Chunks& chunks) {
		chunk_renderer.imgui(chunks);
	}
};

} // namespace gl
