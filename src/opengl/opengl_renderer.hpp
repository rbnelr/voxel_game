#pragma once
#include "common.hpp"
#include "engine/renderer.hpp"
#include "opengl_context.hpp"
#include "opengl_helper.hpp"
#include "opengl_shaders.hpp"
#include "chunk_renderer.hpp"

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

class OpenglRenderer : public Renderer {
public:
	OpenglContext ctx; // make an 'opengl context' first member so opengl init happens before any other ctors (which might make opengl calls)

	Shaders shaders;

	UniformBuffer<CommonUniforms> common_uniforms = { "Common", 0 };
	
	Shader* test = shaders.compile("test");
	Vao dummy_vao = {"dummy_vao"};

	virtual bool get_vsync () {
		return ctx.vsync;
	}
	virtual void set_vsync (bool state) {
		ctx.set_vsync(state);
	}

	bool wireframe = false;
	bool wireframe_backfaces = true;
	float line_width = 2.0f;

	bool debug_frustrum_culling = false;

	OpenglRenderer (GLFWwindow* window, char const* app_name): ctx{window, app_name} {}
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
		ImGui::SliderFloat("line_width", &line_width, 1.0f, 8.0f);

		ImGui::Checkbox("debug_frustrum_culling", &debug_frustrum_culling);
	}

	virtual void chunk_renderer_imgui (Chunks& chunks) {}
};

} // namespace gl
