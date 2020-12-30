#pragma once
#include "common.hpp"
#include "renderer.hpp"

#include "glad/glad.h"

struct GLFWwindow;

namespace gl {

#define OGL_DEBUG_OUTPUT (DEBUGLEVEL >= 2)
#define OGL_DEBUG_OUTPUT_BREAKPOINT (DEBUGLEVEL >= 4)

#define OGL_DEBUG_LABELS 1

class OpenglRenderer : public Renderer {
public:

	int _vsync_on_interval = 1; // use 1 or -1 for vsync

	virtual void set_vsync (bool state);

	OpenglRenderer (GLFWwindow* window, char const* app_name);
	virtual ~OpenglRenderer ();

	virtual void frame_begin (GLFWwindow* window, kiss::ChangedFiles& changed_files);
	virtual void render_frame (GLFWwindow* window, Input& I, Game& game);

	virtual void graphics_imgui () {}
	virtual void chunk_renderer_imgui (Chunks& chunks) {}

	void imgui_draw ();

	static void APIENTRY debug_callback (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* message, void const* userParam);
};

} // namespace gl
