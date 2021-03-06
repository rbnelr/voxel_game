#pragma once
#include "common.hpp"
#include "engine/renderer.hpp"
#include "opengl_context.hpp"
#include "opengl_helper.hpp"
#include "opengl_shaders.hpp"
#include "gl_chunk_renderer.hpp"
#include "engine/input.hpp"

namespace gl {

// mesh has to be bound
inline void draw_submesh (IndexedMesh& mesh, GenericSubmesh submesh) {
	glDrawElementsBaseVertex(GL_TRIANGLES,
		submesh.index_count, // vertex draw count is how many indicies there are
		GL_UNSIGNED_SHORT,
		(void*)(submesh.index_offs * sizeof(uint16_t)), // index offset
		(GLint)submesh.vertex_offs); // basevertex is the vertex offset
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

	void set (Camera_View const& view, int2 viewport_size) {
		memset(this, 0, sizeof(*this)); // zero padding

		world_to_clip = view.cam_to_clip * (float4x4)view.world_to_cam;
		world_to_cam = (float4x4)view.world_to_cam;
		cam_to_clip = view.cam_to_clip;

		clip_to_world = (float4x4)view.cam_to_world * view.clip_to_cam;
		clip_to_cam = view.clip_to_cam;
		cam_to_world = (float4x4)view.cam_to_world;

		clip_near = view.clip_near;
		clip_far = view.clip_far;
		this->viewport_size = (float2)viewport_size;
	}
};
struct CommonUniforms {
	ViewUniforms view;

	CommonUniforms (Input& I, Game& game, int2 viewport_size) {
		view.set(game.view, viewport_size);
	}
};

class OpenglRenderer;

struct glDebugDraw {
	VertexBuffer vbo_lines	= vertex_buffer<DebugDraw::LineVertex>("DebugDraw.vbo_lines");
	VertexBuffer vbo_tris	= vertex_buffer<DebugDraw::TriVertex> ("DebugDraw.vbo_tris");

	IndexedInstancedBuffer vbo_wire_cube = indexed_instanced_buffer<DebugDraw::PosVertex, DebugDraw::Instance>("DebugDraw.vbo_wire_cube");

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
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo_wire_cube.mesh_ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(DebugDraw::_wire_indices), DebugDraw::_wire_indices, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	void draw (OpenglRenderer& r);
};

struct BlockHighlight {
	Shader*			shad;
	IndexedMesh		mesh;
	GenericSubmesh	block_highl, face_highl;

	BlockHighlight (Shaders& shaders) {
		shad = shaders.compile("block_highlight");

		auto bh = load_block_highlight_mesh();
		mesh = upload_mesh("block_highlight",
			bh.data.vertices.data(), bh.data.vertices.size(), bh.data.indices.data(), bh.data.indices.size());

		block_highl = bh.block_highlight;
		face_highl = bh.face_highlight;
	}
	void draw (OpenglRenderer& r, SelectedBlock& block);
};

struct GUIRenderer {
	Shader*			shad;

	struct GUIVertex {
		float2 pos;
		float3 uv; // z is tile texture index, -1 means gui texture
		
		template <typename ATTRIBS>
		static void attributes (ATTRIBS& a) {
			int loc = 0;
			a.init(sizeof(GUIVertex));
			a.template add<AttribMode::FLOAT, decltype(pos       )>(loc++, "pos"       , offsetof(GUIVertex, pos       ));
			a.template add<AttribMode::FLOAT, decltype(uv        )>(loc++, "uv"        , offsetof(GUIVertex, uv        ));
		}
	};
	VertexBuffer	gui_vbo	= vertex_buffer<GUIVertex>("GUIRenderer.gui_vbo");

	std::vector<GUIVertex> vertex_data;

	struct AtlasUVs {
		float2 pos;
		float2 size;
	};
	static constexpr AtlasUVs crosshair_uv				= { {   0, 0}, {32,32} };
	static constexpr AtlasUVs toolbar_uv				= { {32*1, 0}, {32,32} };
	static constexpr AtlasUVs toolbar_highl_uv			= { {32*2, 0}, {32,32} };
	static constexpr AtlasUVs toolbar_selected_uv		= { {32*3, 0}, {32,32} };

	int gui_scale = 4;
	bool crosshair = false;

	void push_quad (GUIVertex* arr) {
		size_t idx = vertex_data.size();
		vertex_data.resize(idx + 6);
		auto* out = &vertex_data[idx];

		out[0] = arr[1];
		out[1] = arr[3];
		out[2] = arr[0];
		out[3] = arr[0];
		out[4] = arr[3];
		out[5] = arr[2];
	}

	// render quad with pixel coords
	void draw_gui_quad (float2 const& pos, float2 const& size, AtlasUVs const& uv);
	void draw_item_quad (float2 const& pos, float2 const& size, item_id item);

	GUIRenderer (Shaders& shaders) {
		shad = shaders.compile("gui");
	}
	void draw (OpenglRenderer& r, Input& I, Game& game);

	void draw_gui (Input& I, Game& game);
};

class OpenglRenderer : public Renderer {
public:
	OpenglContext	ctx; // make an 'opengl context' first member so opengl init happens before any other ctors (which might make opengl calls)

	StateManager	state;
	Shaders			shaders;

	Framebuffer		framebuffer;
	bool			trigger_screenshot = false;
	bool			screenshot_hud = false;

	Ubo				common_uniforms = {"common_ubo"};

	Sampler			tile_sampler = {"tile_sampler"};
	Sampler			gui_sampler = {"gui_sampler"};
	Sampler			normal_sampler = {"normal_sampler"};

	Ssbo			block_meshes_ssbo = {"block_meshes_ssbo"};
	Ssbo			block_tiles_ssbo = {"block_tiles_ssbo"};

	enum TextureUnit : GLint {
		TILE_TEXTURES=0,
		GUI_ATLAS,
		HEAT_GRADIENT,
	};

	Texture2DArray	tile_textures	= {"tile_textures"};
	Texture2D		gui_atlas		= {"gui_atlas"};
	Texture2D		heat_gradient	= {"heat_gradient"};

	ChunkRenderer	chunk_renderer	= ChunkRenderer(shaders);
	Raytracer		raytracer		= Raytracer(shaders);

	BlockHighlight	block_highl		= BlockHighlight(shaders);
	GUIRenderer		gui_renderer	= GUIRenderer(shaders);

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

	bool load_textures (); // can be reloaded
	void load_static_data ();

	OpenglRenderer (GLFWwindow* window, char const* app_name): ctx{window, app_name} {
		load_static_data();

		float max_aniso = 1.0f;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_aniso);

		glSamplerParameteri(tile_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glSamplerParameteri(tile_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameteri(tile_sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glSamplerParameteri(tile_sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glSamplerParameterf(tile_sampler, GL_TEXTURE_MAX_ANISOTROPY, max_aniso);

		glSamplerParameteri(gui_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glSamplerParameteri(gui_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glSamplerParameteri(gui_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(gui_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glSamplerParameteri(normal_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glSamplerParameteri(normal_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glSamplerParameteri(normal_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glSamplerParameteri(normal_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glSamplerParameterf(normal_sampler, GL_TEXTURE_MAX_ANISOTROPY, max_aniso);
	}
	virtual ~OpenglRenderer () {}

	virtual void frame_begin (GLFWwindow* window, Input& I, kiss::ChangedFiles& changed_files);
	virtual void render_frame (GLFWwindow* window, Input& I, Game& game);

	virtual void screenshot_imgui (Input& I) {
		trigger_screenshot = ImGui::Button("Screenshot [F8]") || I.buttons[KEY_F8].went_down;
		ImGui::SameLine();
		ImGui::Checkbox("With HUD", &screenshot_hud);
	}
	virtual void graphics_imgui (Input& I) {
		framebuffer.imgui();

		ImGui::Checkbox("wireframe", &wireframe);
		ImGui::SameLine();
		ImGui::Checkbox("backfaces", &wireframe_backfaces);

		debug_draw.imgui();
		ImGui::SliderFloat("line_width", &line_width, 1.0f, 8.0f);

		ImGui::Separator();

		ImGui::Checkbox("draw_chunks", &chunk_renderer._draw_chunks);
		ImGui::Checkbox("crosshair", &gui_renderer.crosshair);

		ImGui::SliderInt("gui_scale", &gui_renderer.gui_scale, 1, 16);

		raytracer.imgui();
	}

	virtual void chunk_renderer_imgui (Chunks& chunks) {
		chunk_renderer.imgui(chunks);
	}
};

} // namespace gl
