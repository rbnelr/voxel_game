#pragma once
#include "common.hpp"
#include "engine/renderer.hpp"
#include "opengl_context.hpp"
#include "opengl_helper.hpp"
#include "opengl_shaders.hpp"
#include "gl_chunk_renderer.hpp"

namespace gl {

struct ViewUniforms {
	float4x4 world_to_cam;
	float4x4 cam_to_world;
	float4x4 cam_to_clip;
	float4x4 clip_to_cam;
	float4x4 world_to_clip;
	float    clip_near;
	float    clip_far;
	float2   viewport_size;

	void set (Camera_View const& view, int2 viewport_size) {
		memset(this, 0, sizeof(*this)); // zero padding

		world_to_cam = (float4x4)view.world_to_cam;
		cam_to_world = (float4x4)view.cam_to_world;
		cam_to_clip = view.cam_to_clip;
		clip_to_cam = view.clip_to_cam;
		world_to_clip = view.cam_to_clip * (float4x4)view.world_to_cam;
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

	Shader* shad_lines;
	Shader* shad_lines_occluded;
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
	}

	void draw (OpenglRenderer& r);
};

Mesh block_highlight ();

class OpenglRenderer : public Renderer {
public:
	OpenglContext ctx; // make an 'opengl context' first member so opengl init happens before any other ctors (which might make opengl calls)

	StateManager state;
	Shaders shaders;

	Ubo common_uniforms = {"common_ubo"};

	Sampler tile_sampler = {"tile_sampler"};
	Ubo block_meshes_ubo = {"block_meshes_ubo"};
	Texture2DArray tile_textures = {"tile_textures"};

	ChunkRenderer chunk_renderer = ChunkRenderer(shaders);

	Shader*			block_highl_shad = shaders.compile("block_highlight");
	Mesh			block_highl_mesh = block_highlight();
	void draw_block_highlight (OpenglRenderer& r, SelectedBlock& block);

	virtual bool get_vsync () {
		return ctx.vsync;
	}
	virtual void set_vsync (bool state) {
		ctx.set_vsync(state);
	}

	bool wireframe = false;
	bool wireframe_backfaces = true;
	float line_width = 1.0f;

	glDebugDraw debug_draw = glDebugDraw(shaders);

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
	}
	virtual ~OpenglRenderer () {}

	virtual void frame_begin (GLFWwindow* window, kiss::ChangedFiles& changed_files);
	virtual void render_frame (GLFWwindow* window, Input& I, Game& game);

	virtual void graphics_imgui () {
		//screenshot.imgui();

		//if (imgui_push("Renderscale")) {
		//	ImGui::Text("res: %4d x %4d px (%5.2f Mpx)", renderscale_size.x, renderscale_size.y, (float)(renderscale_size.x * renderscale_size.y) / 1000 / 1000);
		//	ImGui::SliderFloat("renderscale", &renderscale, 0.02f, 2.0f);
		//
		//	renderscale_nearest_changed = ImGui::Checkbox("renderscale nearest", &renderscale_nearest);
		//
		//	imgui_pop();
		//}

		ImGui::Checkbox("wireframe", &wireframe);
		ImGui::SameLine();
		ImGui::Checkbox("backfaces", &wireframe_backfaces);

		debug_draw.imgui();
		ImGui::SliderFloat("line_width", &line_width, 1.0f, 8.0f);
	}

	virtual void chunk_renderer_imgui (Chunks& chunks) {
		chunk_renderer.imgui(chunks);
	}
};

} // namespace gl
