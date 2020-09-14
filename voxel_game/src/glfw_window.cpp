#include "stdafx.hpp"
//#include "glad/glad_wgl.h"

#include "glfw_window.hpp"
#include "graphics/glshader.hpp"
#include "graphics/debug_graphics.hpp"

#include "input.hpp"
#include "game.hpp"
#include "graphics/camera.hpp" // for use_reverse_depth

GLFWwindow*	window = nullptr;
int			frame_counter = 0;

#define WINDOW_RES int2(1920, 1080)

constexpr int GL_VERSION_MAJOR = 3;
constexpr int GL_VERSION_MINOR = 3;
constexpr bool VAO_REQUIRED = (GL_VERSION_MAJOR * 100 + GL_VERSION_MINOR) >= 303;

//// vsync and fullscreen mode

// handle vsync interval allowing -1 or not depending on extension
int		_vsync_on_interval = 1;
// keep latest vsync for toggle_fullscreen
bool	vsync;

struct Rect {
	int2	 pos;
	int2	 dim;
};

bool	fullscreen = false;
bool	borderless_fullscreen = true; // use borderless fullscreen as long as the cpu usage (gpu driver?) bug happens on my dev desktop
Rect	window_positioning;

bool get_vsync () {
	return vsync;
}
void set_vsync (bool on) {
	ZoneScopedN("glfwSwapInterval");
	
	glfwSwapInterval(on ? _vsync_on_interval : 0);
	vsync = on;
}

struct Monitor {
	GLFWmonitor* monitor;
	GLFWvidmode const* vidmode;
	int2	 pos;
	int2	 size;
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
	ZoneScopedN("switch_fullscreen");

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
		glfwSetWindowMonitor(window, NULL, window_positioning.pos.x, window_positioning.pos.y,
			window_positioning.dim.x, window_positioning.dim.y, GLFW_DONT_CARE);
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

	shaders = std::make_unique<ShaderManager>();
	debug_graphics = std::make_unique<DebugGraphics>();

	{
		ZoneScopedN("imgui.init()");
		imgui.init();
	}

	auto game = std::make_unique<Game>();

	input.dt = 0; // dt zero on first frame
	uint64_t prev_frame_end = get_timestamp();

	glfw_input_pre_gameloop(window);

	for (;;) {
		{
			ZoneScopedN("get input");

			input.clear_frame_input();
			glfwPollEvents();

			if (glfwWindowShouldClose(window))
				break;

			glfw_sample_non_callback_input(window);

			input = input;
		}

		{
			imgui.frame_start();

			game->frame();

			imgui.frame_end();
		}

		{
			ZoneScopedN("glfwSwapBuffers");
			glfwSwapBuffers(window);
		}

		input = input;

		// Calc dt based on prev frame duration
		uint64_t now = get_timestamp();
		input.real_dt = (float)(now - prev_frame_end) / (float)timestamp_freq;
		input.dt = min(input.real_dt * (input.pause_time ? 0 : input.time_scale), input.max_dt);
		input.unscaled_dt = min(input.real_dt, input.max_dt);
		prev_frame_end = now;

		frame_counter++;

		TracyGpuCollect;

		FrameMark;
	}

	game = nullptr;

	imgui.destroy();

	debug_graphics = nullptr;
	shaders = nullptr;
}

void glfw_error (int err, const char* msg) {
	clog(ERROR, "GLFW Error! [0x%x] '%s'\n", err, msg);
}

void APIENTRY ogl_debug (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* message, void const* userParam) {
	//if (source == GL_DEBUG_SOURCE_SHADER_COMPILER_ARB) return;
	if (source == GL_DEBUG_SOURCE_APPLICATION) return;

	// hiding irrelevant infos/warnings
	switch (id) {
		case 131185: // Buffer detailed info (where the memory lives which is supposed to depend on the usage hint)
					 //case 1282: // using shader that was not compiled successfully
					 //
					 //case 2: // API_ID_RECOMPILE_FRAGMENT_SHADER performance warning has been generated. Fragment shader recompiled due to state change.
		case 131218: // Program/shader state performance warning: Fragment shader in program 3 is being recompiled based on GL state.
		case 131186: // Buffer performance warning: Buffer object 10 (bound to NONE, usage hint is GL_STATIC_DRAW) is being copied/moved from VIDEO memory to HOST memory.
					 //			 //case 131154: // Pixel transfer sync with rendering warning
					 //
					 //			 //case 1282: // Wierd error on notebook when trying to do texture streaming
					 //			 //case 131222: // warning with unused shadow samplers ? (Program undefined behavior warning: Sampler object 0 is bound to non-depth texture 0, yet it is used with a program that uses a shadow sampler . This is undefined behavior.), This might just be unused shadow samplers, which should not be a problem
					 //			 //case 131218: // performance warning, because of shader recompiling based on some 'key'
			return;
	}

	const char* src_str = "<unknown>";
	switch (source) {
		case GL_DEBUG_SOURCE_API_ARB:				src_str = "GL_DEBUG_SOURCE_API_ARB";				break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:		src_str = "GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB";		break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:	src_str = "GL_DEBUG_SOURCE_SHADER_COMPILER_ARB";	break;
		case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:		src_str = "GL_DEBUG_SOURCE_THIRD_PARTY_ARB";		break;
		case GL_DEBUG_SOURCE_APPLICATION_ARB:		src_str = "GL_DEBUG_SOURCE_APPLICATION_ARB";		break;
		case GL_DEBUG_SOURCE_OTHER_ARB:				src_str = "GL_DEBUG_SOURCE_OTHER_ARB";				break;
	}

	const char* type_str = "<unknown>";
	switch (source) {
		case GL_DEBUG_TYPE_ERROR_ARB:				type_str = "GL_DEBUG_TYPE_ERROR_ARB";				break;
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:	type_str = "GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB";	break;
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:	type_str = "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB";	break;
		case GL_DEBUG_TYPE_PORTABILITY_ARB:			type_str = "GL_DEBUG_TYPE_PORTABILITY_ARB";			break;
		case GL_DEBUG_TYPE_PERFORMANCE_ARB:			type_str = "GL_DEBUG_TYPE_PERFORMANCE_ARB";			break;
		case GL_DEBUG_TYPE_OTHER_ARB:				type_str = "GL_DEBUG_TYPE_OTHER_ARB";				break;
	}

	const char* severity_str = "<unknown>";
	switch (severity) {
		case GL_DEBUG_SEVERITY_HIGH_ARB:			severity_str = "GL_DEBUG_SEVERITY_HIGH_ARB";		break;
		case GL_DEBUG_SEVERITY_MEDIUM_ARB:			severity_str = "GL_DEBUG_SEVERITY_MEDIUM_ARB";		break;
		case GL_DEBUG_SEVERITY_LOW_ARB:				severity_str = "GL_DEBUG_SEVERITY_LOW_ARB";			break;
	}

	clog(severity == GL_DEBUG_SEVERITY_HIGH_ARB ? ERROR : WARNING,
		"OpenGL debug message: severity: %s src: %s type: %s id: %d  %s\n", severity_str, src_str, type_str, id, message);

	__debugbreak();
}

#if _DEBUG || 1
#define OPENGL_DEBUG
#define GLFW_DEBUG
#endif

void glfw_init_gl () {
	ZoneScopedN("glfw_init_gl");

	glfwMakeContextCurrent(window);

	{
		gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	}

	push_debug_names = push_debug_names && glfwExtensionSupported("GL_KHR_debug");

#ifdef OPENGL_DEBUG
	if (glfwExtensionSupported("GL_ARB_debug_output")) {
		glDebugMessageCallbackARB(ogl_debug, 0);
	#if _DEBUG // when displaying opengl debug output in release mode, don't do it syncronously as to (theoretically) not hurt performance
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB); // this exists -> if ogl_debuproc needs to be thread safe
	#endif
	}
#endif

	if (glfwExtensionSupported("WGL_EXT_swap_control_tear"))
		_vsync_on_interval = -1;

	set_vsync(true);

	// srgb enabled by default if supported
	// TODO: should I use glfwExtensionSupported or GLAD_GL_ARB_framebuffer_sRGB? does it make a difference?
	if (glfwExtensionSupported("GL_ARB_framebuffer_sRGB"))
		glEnable(GL_FRAMEBUFFER_SRGB);
	else
		clog(ERROR, "No sRGB supported! Shading will be non-linear!\n");

	if (glfwExtensionSupported("GL_ARB_clip_control"))
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
	else
		clog(ERROR, "GL_ARB_clip_control not supported, depth won't be correct!\n");

	if (	!glfwExtensionSupported("GL_NV_gpu_shader5") ||
			!glfwExtensionSupported("GL_NV_shader_buffer_load")) {
		clog(ERROR, "GL_NV_gpu_shader5 or GL_NV_shader_buffer_load not supported!\n");
	}

	TracyGpuContext;
}

int main () {
	{
	#ifdef GLFW_DEBUG
		glfwSetErrorCallback(glfw_error);
	#endif

		{
			ZoneScopedN("glfwInit");

			if (!glfwInit()) {
				fprintf(stderr, "glfwInit failed!\n");
				return 1;
			}
		}

		ZoneScopedN("glfwCreateWindow");

	#ifdef OPENGL_DEBUG
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
	#endif
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
		glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE); // keep app visible when clicking on second monitor while in fullscreen
		
		window = glfwCreateWindow(WINDOW_RES.x, WINDOW_RES.y, "Voxel Game", NULL, NULL);
		if (!window) {
			fprintf(stderr, "glfwCreateWindow failed!\n");
			glfwTerminate();
			return 1;
		}

		if (glfwRawMouseMotionSupported())
			glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

		glfw_register_input_callbacks(window);
	}

	glfw_init_gl();

	glfw_gameloop();

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}
