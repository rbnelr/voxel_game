#pragma once
#include "common.hpp"
#include "input.hpp"
#include "game.hpp"
#include "renderer.hpp"

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
	Rect	window_pos;
	
	int		frame_counter = 0;

	std::unique_ptr<Game>			game;

	RenderBackend					render_backend = RenderBackend::VULKAN;
	bool							switch_render_backend = false;
	std::unique_ptr<Renderer>		renderer;

	DirectoyChangeNotifier			file_changes = DirectoyChangeNotifier("./", true);

	void set_vsync (bool on) {
		renderer->set_vsync(on);
		vsync = on;
	}

	bool switch_fullscreen (bool fullscreen, bool borderless_fullscreen);
	bool toggle_fullscreen ();

	// close down game after current frame
	void close ();

	void open_window ();
	void close_window ();

	void switch_renderer ();

	void run ();
};

inline Window g_window; // global window, needed to allow Logger to be global, which needs frame_counter, prefer to pass window along if possible
