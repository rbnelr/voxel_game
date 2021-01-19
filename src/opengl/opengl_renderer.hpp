#pragma once
#include "common.hpp"
#include "renderer.hpp"
#include "opengl_helper.hpp"
#include "opengl_shaders.hpp"
#include "chunk_renderer.hpp"

#include "glad/glad.h"

struct GLFWwindow;

namespace gl {

#define OGL_DEBUG_OUTPUT (DEBUGLEVEL >= 2)
#define OGL_DEBUG_OUTPUT_BREAKPOINT (DEBUGLEVEL >= 4)

#define OGL_DEBUG_LABELS 1

struct OpenglContext {
	OpenglContext (GLFWwindow* window, char const* app_name);
	~OpenglContext ();

	int _vsync_on_interval = 1; // use 1 or -1 for vsync

	void set_vsync (bool state);
	static void APIENTRY debug_callback (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* message, void const* userParam);
};

class OpenglRenderer : public Renderer {
public:
	OpenglContext ctx; // make an 'opengl context' first member so opengl init happens before any other ctors (which might make opengl calls)

	Shaders shaders;

	Shader* test = shaders.compile("chunks");

	virtual void set_vsync (bool state) {
		ctx.set_vsync(state);
	}

	bool wireframe = false;
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
		ImGui::Checkbox("debug_frustrum_culling", &debug_frustrum_culling);
	}

	virtual void chunk_renderer_imgui (Chunks& chunks) {}

	void imgui_draw ();
};

} // namespace gl
