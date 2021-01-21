#pragma once
#include "common.hpp"
#include "opengl_helper.hpp"

struct GLFWwindow;

namespace gl {
	struct OpenglContext {
		OpenglContext (GLFWwindow* window, char const* app_name);
		~OpenglContext ();

		bool vsync = true;
		int _vsync_on_interval = 1; // handle vsync interval allowing -1 or not depending on extension

		void set_vsync (bool state);

		void imgui_begin ();
		void imgui_draw ();

		static void APIENTRY debug_callback (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* message, void const* userParam);
	};
}
