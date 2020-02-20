//#include "glad/glad_wgl.h"

#include "glfw_window.hpp"
#include "kissmath/int2.hpp"
#include "kissmath/float2.hpp"
#include "graphics/glshader.hpp"
#include "graphics/debug_graphics.hpp"
using namespace kissmath;

#include "util/timer.hpp"
#include "input.hpp"
#include "game.hpp"
#include "dear_imgui.hpp"
using namespace kiss;

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
bool			borderless_fullscreen = false;
Rect			window_positioning;

Input			_input = {};

void set_cursor_mode (bool enabled) {
	if (enabled)
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // Cursor enabled, can interact with Imgui
	else
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Cursor disabled & Imgui interaction disabled, all controls go to game
}
void toggle_cursor_mode () {
	bool cursor_enabled = glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED;
	cursor_enabled = !cursor_enabled;
	set_cursor_mode(cursor_enabled);
}

//// input
void glfw_key_event (GLFWwindow* window, int key, int scancode, int action, int mods) {
	assert(action == GLFW_PRESS || action == GLFW_RELEASE || action == GLFW_REPEAT);

	bool went_down =	action == GLFW_PRESS;
	bool went_up =		action == GLFW_RELEASE;

	bool alt =			(mods & GLFW_MOD_ALT) != 0;

	// Toggle fullscreen with F11 or CTRL-ENTER
	if (key == GLFW_KEY_F11 || (alt && key == GLFW_KEY_ENTER)) {
		if (went_down) toggle_fullscreen();
		return;
	}

	// Toggle Imgui visibility with F1
	if (key == GLFW_KEY_F1) {
		if (went_down) imgui.enabled = !imgui.enabled;
		return;
	}

	// Toggle between Imgui interaction and game control
	if (key == GLFW_KEY_F2) {
		if (went_down) toggle_cursor_mode();
		return;
	}

	if ((went_down || went_up) && key >= GLFW_KEY_SPACE && key <= GLFW_KEY_LAST) {
		_input.buttons[key].is_down = went_down;
		_input.buttons[key].went_down = went_down;
		_input.buttons[key].went_up = went_up;
	}
}
void glfw_char_event (GLFWwindow* window, unsigned int codepoint, int mods) {
	// for typing input
}
void glfw_mouse_button_event (GLFWwindow* window, int button, int action, int mods) {
	assert(action == GLFW_PRESS || action == GLFW_RELEASE);

	bool went_down = action == GLFW_PRESS;
	bool went_up =	 action == GLFW_RELEASE;

	if ((went_down || went_up) && button >= GLFW_MOUSE_BUTTON_1 && button <= GLFW_MOUSE_BUTTON_8) {
		_input.buttons[button].is_down = went_down;
		_input.buttons[button].went_down = went_down;
		_input.buttons[button].went_up = went_up;
	}
}

int frame_counter = 0;
double _prev_mouse_pos_x, _prev_mouse_pos_y;
bool _prev_cursor_enabled;

// The initial event seems to report the same position as our initial glfwGetCursorPos, so that delta is fine
// But when toggling the cursor from disabled to visible cursor jumps back to the prev position, and an event reports this as delta so we need to discard this 
void glfw_mouse_move_event (GLFWwindow* window, double xpos, double ypos) {
	float2 delta = float2((float)(xpos - _prev_mouse_pos_x), (float)(ypos - _prev_mouse_pos_y));
	delta.y = -delta.y; // convert to bottom up

	_prev_mouse_pos_x = xpos;
	_prev_mouse_pos_y = ypos;

	bool cursor_enabled = glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED;
	bool cursor_toggled = cursor_enabled != _prev_cursor_enabled;
	_prev_cursor_enabled = cursor_enabled;

	bool discard_delta = cursor_toggled;

	//logf("glfw_mouse_move_event: %7d: %f %f%s\n", frame_counter, delta.x, delta.y, discard_delta ? " (discarded)":"");
	
	if (!discard_delta)
		_input.mouse_delta += delta;
}
void glfw_mouse_scroll (GLFWwindow* window, double xoffset, double yoffset) {
	// assume int, if glfw_mouse_scroll ever gives us 0.2 for ex. this might break
	// But the gameplay code wants to assume mousewheel moves in "clicks", for item swapping
	// I've personally never seen a mousewheel that does not move in "clicks" anyway
	_input.mouse_wheel_delta += (int)ceil(abs(yoffset)) * (int)normalizesafe((float)yoffset); // -1.1f => -2    0 => 0    0.3f => +1
}

void glfw_register_callbacks (GLFWwindow* window) {
	glfwSetKeyCallback(window,			glfw_key_event);
	glfwSetCharModsCallback(window,		glfw_char_event);
	glfwSetCursorPosCallback(window,	glfw_mouse_move_event);
	glfwSetMouseButtonCallback(window,	glfw_mouse_button_event);
	glfwSetScrollCallback(window,		glfw_mouse_scroll);
}

void glfw_get_non_callback_input (GLFWwindow* window) {
	glfwGetFramebufferSize(window, &_input.window_size.x, &_input.window_size.y);

	double x, y;
	glfwGetCursorPos(window, &x, &y);

	_input.cursor_pos = float2((float)x, (float)y);
	_input.cursor_pos.y = _input.window_size.y - 1 - _input.cursor_pos.y;

	//logf("cursor_pos: %f %f\n", input.cursor_pos.x, input.cursor_pos.y);
}

//// vsync and fullscreen mode

// handle vsync interval allowing -1 or not depending on extension
int _vsync_on_interval = 1;
// keep latest vsync for toggle_fullscreen
bool vsync;

bool get_vsync () {
	return vsync;
}
void set_vsync (bool on) {
	glfwSwapInterval(on ? _vsync_on_interval : 0);
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
#if 0 // actually using VAOs now
	GLuint vao;
	if (VAO_REQUIRED) {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
	}
#endif

	shaders = std::make_unique<ShaderManager>();
	debug_graphics = std::make_unique<DebugGraphics>();

	imgui.init();

	auto game = std::make_unique<Game>();

	_input.dt = 0; // dt zero on first frame
	uint64_t prev = get_timestamp();

	// Get initial mouse position
	glfwGetCursorPos(window, &_prev_mouse_pos_x, &_prev_mouse_pos_y);

	// Get initial cursor mode
	_prev_cursor_enabled = glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED;

	for (;;) {
		_input.clear_frame_input();

		glfwPollEvents();

		if (glfwWindowShouldClose(window))
			break;

		glfw_get_non_callback_input(window);

		input = _input;

		imgui.frame_start();

		game->frame();

		imgui.frame_end();

		glfwSwapBuffers(window);

		// Calc dt based on prev frame duration
		uint64_t now = get_timestamp();
		_input.real_dt = (float)(now - prev) / (float)timestamp_freq;
		_input.dt = min(_input.real_dt * (_input.pause_time ? 0 : _input.time_scale), _input.max_dt);
		_input.unscaled_dt = min(_input.real_dt, _input.max_dt);
		prev = now;

		frame_counter++;
	}

	imgui.destroy();

	debug_graphics = nullptr;
	shaders = nullptr;

#if 0
	if (VAO_REQUIRED) {
		glDeleteVertexArrays(1, &vao);
	}
#endif
}

void glfw_error (int err, const char* msg) {
	logf(ERROR, "GLFW Error! [0x%x] '%s'\n", err, msg);
}

void APIENTRY ogl_debug (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* message, void const* userParam) {
	//if (source == GL_DEBUG_SOURCE_SHADER_COMPILER_ARB) return;

	// hiding irrelevant infos/warnings
	switch (id) {
	case 131185: // Buffer detailed info (where the memory lives which is supposed to depend on the usage hint)
	//case 1282: // using shader that was not compiled successfully
	//
	//case 2: // API_ID_RECOMPILE_FRAGMENT_SHADER performance warning has been generated. Fragment shader recompiled due to state change.
	case 131218: // Program/shader state performance warning: Fragment shader in program 3 is being recompiled based on GL state.
	
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

	logf(ERROR, "OpenGL debug message: severity: %s src: %s type: %s id: %d  %s\n", severity_str, src_str, type_str, id, message);
}

#if _DEBUG || 1
	#define OPENGL_DEBUG
	#define GLFW_DEBUG
#endif

void glfw_init_gl () {
	glfwMakeContextCurrent(window);

	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

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
		logf(ERROR, "No sRGB supported! Shading will be non-linear!\n");
}

int main () {

#ifdef GLFW_DEBUG
	glfwSetErrorCallback(glfw_error);
#endif

	if (!glfwInit()) {
		logf(ERROR, "glfwInit failed!\n");
		return 1;
	}

#ifdef OPENGL_DEBUG
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
	glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE); // keep app visible when clicking on second monitor while in fullscreen

	window = glfwCreateWindow(WINDOW_RES.x, WINDOW_RES.y, "Voxel Game", NULL, NULL);
	if (!window) {
		logf(ERROR, "glfwCreateWindow failed!\n");
		glfwTerminate();
		return 1;
	}
	
	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	glfw_register_callbacks(window);

	glfw_init_gl();

	glfw_gameloop();

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
