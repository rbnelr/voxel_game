#include "common.hpp"
#include "window.hpp"

//// vsync and fullscreen mode
void Window::set_vsync (bool on) {
	//ZoneScopedN("glfwSwapInterval");

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

bool Window::switch_fullscreen (bool fullscreen, bool borderless_fullscreen) {
	//ZoneScopedN("switch_fullscreen");

	if (!this->fullscreen) {
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

	this->fullscreen = fullscreen;
	this->borderless_fullscreen = borderless_fullscreen;
	return true;
}

bool Window::toggle_fullscreen () {
	return switch_fullscreen(!fullscreen, borderless_fullscreen);
}

//// gameloop

void frameloop () {
	Renderer r(APPNAME, window.window);

	glfw_input_pre_gameloop(window);

	imgui.init(r);

	window.input.dt = 0; // dt zero on first frame
	uint64_t prev = get_timestamp();

	for (;;) {
		{
			//input.clear_frame_input();
			glfwPollEvents();

			if (glfwWindowShouldClose(window.window))
				break;

			glfw_sample_non_callback_input(window);
		}

		{
			if (!r.frame_begin(window.window))
				continue;

			auto buf = r.render_begin();

			imgui.frame_start();

			ImGui::ShowDemoWindow();

			//game->frame();

			imgui.frame_end(buf);

			r.render_end();

			r.frame_end(window.window, buf);
		}

		{ // Calc dt based on prev frame duration
			auto& i = window.input;

			uint64_t now = get_timestamp();
			i.real_dt = (float)(now - prev) / (float)timestamp_freq;
			i.dt = min(i.real_dt * (i.pause_time ? 0 : i.time_scale), i.max_dt);
			i.unscaled_dt = min(i.real_dt, i.max_dt);
			prev = now;
		}

		window.frame_counter++;
	}

	imgui.destroy(r);
}

void glfw_error (int err, const char* msg) {
	fprintf(stderr, "GLFW Error! [0x%x] '%s'\n", err, msg);
}

int main () {
#ifndef NDEBUG
	glfwSetErrorCallback(glfw_error);
#endif

	if (!glfwInit()) {
		fprintf(stderr, "glfwInit failed!\n");
		return 1;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE); // keep app visible when clicking on second monitor while in fullscreen

	window.window = glfwCreateWindow(1920, 1080, APPNAME, NULL, NULL);
	if (window.window) {

		if (glfwRawMouseMotionSupported())
			glfwSetInputMode(window.window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

		glfw_register_input_callbacks(window);

		frameloop();

		glfwDestroyWindow(window.window);

	} else {
		fprintf(stderr, "glfwCreateWindow failed!\n");
	}

	glfwTerminate();
	return 0;
}
