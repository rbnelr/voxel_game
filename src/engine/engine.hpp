#pragma once
#include "kisslib/kissmath.hpp"
#include "kisslib/read_directory.hpp"
#include "input.hpp"
using namespace kiss;

struct GLFWwindow;
struct GLFWcursor;

// For update_files_changed:
// Repeat reloading of assets because reacting to filechanges often fails because of half-written files
template <typename FUNC>
bool try_reloading (FUNC loadfunc) {
	int max_retries = 100;
	int retry_delay = 10; // ms

	for (int i=0; i<max_retries; ++i) {
		sleep_msec(retry_delay); // start out with a delay in hopes of getting a working file the first time

		if (loadfunc())
			return true; // success
	}
	return false; // fail
}


class Engine {
public:

	enum ShouldClose {
		CLOSE_CANCEL=0,
		CLOSE_PENDING,
		CLOSE_NOW,
	};

	Engine (const char* window_title);
	virtual ~Engine ();

	GLFWwindow* window;
	Input input = {};
	
	
	bool _draw_on_size_events = false;
	bool _was_not_visible_and_paused = false;

	int _vsync_on_interval = 1;

	ShouldClose _should_close = Engine::CLOSE_CANCEL;

	Logger logger;

	DirectoyChangeNotifier file_changes = DirectoyChangeNotifier("./", true);
	
	// close down game after current frame
	void close ();

	
	// called during imgui execution when app wants to close
	// return CLOSE_PENDING when you want to open a popup until the user decides if they really want to close
	//   -> close_confirmation will be called again the next frame
	// return CLOSE_CANCEL = when the user wanted to cancel closing
	// return CLOSE_NOW = when the user wants to close
	// don't implement this to get normal immediate closing behavior
	virtual ShouldClose close_confirmation () {
		return CLOSE_NOW;
	}

	int2 window_pos = INT_MIN;
	int2 window_size = int2(1280, 720);

	bool fullscreen = false;
	bool borderless_fullscreen = true;

	bool set_fullscreen (bool fullscreen, bool borderless_fullscreen);
	bool toggle_fullscreen ();

	Timing_Histogram fps_display;

	bool vsync = true;
	void set_vsync (bool vsync);

	// Avoid imgui asserts due to missing draw_imgui when not calling renderer for whatever reason
	bool _need_to_draw_imgui = false;
	void draw_imgui ();
	
	bool imgui_enabled = true;
#if IMGUI_DEMO
	bool imgui_show_demo_window = false;
#endif

	bool screenshot_hud = false;
	bool trigger_screenshot = false;

	enum CursorMode {
		CURSOR_NORMAL=0,
		CURSOR_FINGER,

		_CURSORS_COUNT,
	};
	GLFWcursor* _cursors[_CURSORS_COUNT] = {};

	void set_cursor (CursorMode mode);

	virtual void imgui () = 0;
	virtual void frame () = 0;

	virtual void json_load () = 0;
	virtual void json_save () = 0;

	virtual bool update_files_changed (kiss::ChangedFiles& changed_files) { return true; }

	int main_loop ();
};
