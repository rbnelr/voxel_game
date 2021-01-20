#pragma once
#include "common.hpp"
#include "opengl_helper.hpp"

struct GLFWwindow;

namespace gl {
	struct OpenglContext {
		OpenglContext (GLFWwindow* window, char const* app_name);
		~OpenglContext ();

		int _vsync_on_interval = 1; // use 1 or -1 for vsync

		void set_vsync (bool state);

		void imgui_begin ();
		void imgui_draw ();

		static void APIENTRY debug_callback (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* message, void const* userParam);
	};
}
