#include "glfw_window.hpp"
#include "graphics/renderer.hpp"
#include "kissmath/int2.hpp"
#include "kissmath/float2.hpp"
#include "graphics/debug_graphics.hpp"
#include "util/timer.hpp"
#include "input.hpp"
#include "game.hpp"
#include "dear_imgui.hpp"
using namespace kissmath;
using namespace kiss;

#include "optick.h"

#include <memory>
#include "stdio.h"
#include "assert.h"

#define WINDOW_RES int2(1920, 1080)

constexpr int GL_VERSION_MAJOR = 3;
constexpr int GL_VERSION_MINOR = 3;
constexpr bool VAO_REQUIRED = (GL_VERSION_MAJOR * 100 + GL_VERSION_MINOR) >= 303;

struct Rect {
	int2 pos;
	int2 dim;
};

GLFWwindow*		window;

bool			fullscreen = false;
bool			borderless_fullscreen = true; // use borderless fullscreen as long as the cpu usage (gpu driver?) bug happens on my dev desktop
Rect			window_positioning;

//// vsync and fullscreen mode

// handle vsync interval allowing -1 or not depending on extension
int _vsync_on_interval = 1;
// keep latest vsync for toggle_fullscreen
bool vsync;

bool get_vsync () {
	return vsync;
}
void set_vsync (bool on) {
	OPTICK_EVENT();

	//glfwSwapInterval(on ? _vsync_on_interval : 0);
	vsync = on;
}

struct Monitor {
	GLFWmonitor* monitor;
	GLFWvidmode const* vidmode;
	int2 pos;
	int2 size;
};

bool select_monitor_from_window_pos (Rect window_positioning, Monitor* selected_monior) {
	int count;
	auto** glfw_monitors = glfwGetMonitors(&count);

	std::vector<Monitor> monitors;
	monitors.resize(count);

	auto window_monitor_overlap = [=] (Monitor const& mon) {
		int2 a = clamp(window_positioning.pos, mon.pos, mon.pos + mon.size);
		int2 b = clamp(window_positioning.pos + window_positioning.dim, mon.pos, mon.pos + mon.size);

		int2 size = b - a;
		float overlap_area = (float)(size.x * size.y);
		return overlap_area;
	};

	float max_overlap = -INF;
	Monitor* max_overlap_monitor = nullptr;

	for (int i=0; i<count; ++i) {
		auto& m = monitors[i];

		m.monitor = glfw_monitors[i];
		m.vidmode = glfwGetVideoMode(m.monitor);
		glfwGetMonitorPos(m.monitor, &m.pos.x, &m.pos.y);

		m.size.x = m.vidmode->width;
		m.size.y = m.vidmode->height;

		float overlap = window_monitor_overlap(m);
		if (overlap > max_overlap) {
			max_overlap = overlap;
			max_overlap_monitor = &m;
		}
	}

	if (!max_overlap_monitor)
		return false; // fail, glfw returned no monitors

	*selected_monior = *max_overlap_monitor;
	return true;
}

bool get_fullscreen (bool* borderless_fullscreen) {
	if (borderless_fullscreen) *borderless_fullscreen = ::borderless_fullscreen;
	return fullscreen;
}
bool switch_fullscreen (bool fullscreen, bool borderless_fullscreen) {
	OPTICK_EVENT();

	if (!::fullscreen) {
		// store windowed window placement
		glfwGetWindowPos(window, &window_positioning.pos.x, &window_positioning.pos.y);
		glfwGetWindowSize(window, &window_positioning.dim.x, &window_positioning.dim.y);
	}

	if (fullscreen) {
		Monitor monitor;
		if (!select_monitor_from_window_pos(window_positioning, &monitor))
			return false; // fail

		if (borderless_fullscreen) {
			glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
			glfwSetWindowMonitor(window, NULL, monitor.pos.x, monitor.pos.y, monitor.size.x, monitor.size.y, GLFW_DONT_CARE);
		} else {
			glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
			glfwSetWindowMonitor(window, monitor.monitor, 0,0, monitor.vidmode->width, monitor.vidmode->height, monitor.vidmode->refreshRate);
		}
	} else {
		// restore windowed window placement
		glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
		glfwSetWindowMonitor(window, NULL, window_positioning.pos.x, window_positioning.pos.y, window_positioning.dim.x, window_positioning.dim.y, GLFW_DONT_CARE);
	}

	// reset vsync to make sure 
	set_vsync(vsync);

	::fullscreen = fullscreen;
	::borderless_fullscreen = borderless_fullscreen;
	return true;
}

bool toggle_fullscreen () {
	return switch_fullscreen(!fullscreen, borderless_fullscreen);
}

//// gameloop
void glfw_gameloop () {

	debug_graphics = std::make_unique<DebugGraphics>();

	{
		OPTICK_FRAME("imgui.init");
		imgui.init();
	}

	auto game = std::make_unique<Game>();

	input.dt = 0; // dt zero on first frame
	uint64_t prev = get_timestamp();

	glfw_input_pre_gameloop(window);

	for (;;) {
		OPTICK_FRAME("MainThread");

		{
			OPTICK_EVENT("get input");

			input.clear_frame_input();
			glfwPollEvents();

			if (glfwWindowShouldClose(window))
				break;

			glfw_sample_non_callback_input(window);

			input = input;
		}

		if (vulkan->frame_start()) {

			imgui.frame_start();

			game->frame();

			imgui.frame_end();

			vulkan->frame_end();
		}

		//{
		//	OPTICK_EVENT("glfwSwapBuffers");
		//	glfwSwapBuffers(window);
		//}

		input = input;

		// Calc dt based on prev frame duration
		uint64_t now = get_timestamp();
		input.real_dt = (float)(now - prev) / (float)timestamp_freq;
		input.dt = min(input.real_dt * (input.pause_time ? 0 : input.time_scale), input.max_dt);
		input.unscaled_dt = min(input.real_dt, input.max_dt);
		prev = now;

		frame_counter++;
	}

	imgui.destroy();

	debug_graphics = nullptr;

#if 0
	if (VAO_REQUIRED) {
		glDeleteVertexArrays(1, &vao);
	}
#endif
}

void glfw_error (int err, const char* msg) {
	clog(ERROR, "GLFW Error! [0x%x] '%s'\n", err, msg);
}

static constexpr char const* APP_NAME = "Voxel Game";

int main () {
	//OPTICK_START_CAPTURE();

	{
		OPTICK_EVENT("glfw init");

	#ifdef GLFW_DEBUG
		glfwSetErrorCallback(glfw_error);
	#endif

		if (!glfwInit()) {
			fprintf(stderr, "glfwInit failed!\n");
			return 1;
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE); // keep app visible when clicking on second monitor while in fullscreen

		window = glfwCreateWindow(WINDOW_RES.x, WINDOW_RES.y, APP_NAME, NULL, NULL);
		if (!window) {
			fprintf(stderr, "glfwCreateWindow failed!\n");
			glfwTerminate();
			return 1;
		}
	
		if (glfwRawMouseMotionSupported())
			glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

		glfw_register_input_callbacks(window);
	}

	vulkan = std::make_unique<Vulkan>(window, APP_NAME);

	glfw_gameloop();

	vulkan = nullptr;

	glfwDestroyWindow(window);
	glfwTerminate();

	//OPTICK_STOP_CAPTURE();
	//OPTICK_SAVE_CAPTURE("capture.opt");
	OPTICK_SHUTDOWN();
	return 0;
}
