#pragma once
#include "common.hpp"
#include "input.hpp"

static constexpr const char* APPNAME = "Voxel Game";

struct Rect {
	int2	 pos;
	int2	 dim;
};

struct GLFWwindow;

struct Window {
	GLFWwindow* window = nullptr;

	Input	input;

	bool	vsync; // readonly
	int		_vsync_on_interval = 1; // handle vsync interval allowing -1 or not depending on extension

	bool	fullscreen = false; // readonly
	bool	borderless_fullscreen = true; // readonly, use borderless fullscreen as long as the cpu usage (gpu driver?) bug happens on my dev desktop
	Rect	window_positioning;
	
	int		frame_counter = 0;

	DirectoyChangeNotifier	file_changes = DirectoyChangeNotifier("./", true);

	void set_vsync (bool on);

	bool switch_fullscreen (bool fullscreen, bool borderless_fullscreen);
	bool toggle_fullscreen ();

	void close ();
};

inline Window* g_window; // global window, needed to allow Logger to be global, which needs frame_counter, prefer to pass window along if possible
