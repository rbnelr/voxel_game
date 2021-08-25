#include "common.hpp"
#include "window.hpp"

#include "imgui/imgui_impl_glfw.h"
#include "GLFW/glfw3.h"

//// fullscreen mode
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
	ZoneScoped;

	if (!this->fullscreen) {
		// store windowed window placement
		glfwGetWindowPos(window, &window_pos.pos.x, &window_pos.pos.y);
		glfwGetWindowSize(window, &window_pos.dim.x, &window_pos.dim.y);
	}

	if (fullscreen) {
		Monitor monitor;
		if (!select_monitor_from_window_pos(window_pos, &monitor))
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
		glfwSetWindowMonitor(window, NULL, window_pos.pos.x, window_pos.pos.y,
			window_pos.dim.x, window_pos.dim.y, GLFW_DONT_CARE);
	}

	// reset vsync to make sure 
	renderer->set_vsync(renderer->get_vsync());

	this->fullscreen = fullscreen;
	this->borderless_fullscreen = borderless_fullscreen;
	return true;
}

bool Window::toggle_fullscreen () {
	return switch_fullscreen(!fullscreen, borderless_fullscreen);
}

void Window::close () {
	glfwSetWindowShouldClose(window, 1);
}

//// gameloop

void imgui_begin_frame (Window& window) {
	ZoneScoped;

	ImGui_ImplGlfw_NewFrame(g_imgui.enabled && window.input.cursor_enabled);

	{
		auto& io = ImGui::GetIO();
		io.ConfigWindowsMoveFromTitleBarOnly = true;
		if (io.WantCaptureKeyboard)
			window.input.disable_keyboard();
		if (io.WantCaptureMouse)
			window.input.disable_mouse();
	}

	ImGui::NewFrame();
}
void imgui_end_frame () {
	g_logger.imgui();
}

void glfw_error (int err, const char* msg) {
	fprintf(stderr, "GLFW Error! [0x%x] '%s'\n", err, msg);
}

bool app_init () {
#if RENDERER_DEBUG_OUTPUT
	glfwSetErrorCallback(glfw_error);
#endif

	{
		ZoneScopedN("glfwInit");
		if (!glfwInit()) {
			fprintf(stderr, "glfwInit failed!\n");
			return false;
		}
	}

	return true;
}

void app_shutdown () {

	{
		ZoneScopedN("glfwTerminate");
		glfwTerminate();
	}
}

void Window::open_window () {
	ZoneScoped;

	{
		ZoneScopedN("glfwCreateWindow");

		switch (g_window.render_backend) {
			case RenderBackend::OPENGL: {

				glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
			#if RENDERER_DEBUG_OUTPUT
				glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
			#endif
				glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
				glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
				glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
				glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

			} break;
		}

		glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE); // keep app visible when clicking on second monitor while in fullscreen

		window = glfwCreateWindow(1920, 1080, APPNAME, NULL, NULL);
	}

	if (!window) {
		fprintf(stderr, "glfwCreateWindow failed!\n");
		return;
	}

	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	glfw_register_input_callbacks(*this);
}
void Window::close_window () {
	ZoneScoped;

	glfwGetWindowPos(window, &window_pos.pos.x, &window_pos.pos.y);
	glfwGetWindowSize(window, &window_pos.dim.x, &window_pos.dim.y);

	glfwDestroyWindow(window);
	window = nullptr;
}

void Window::switch_renderer () {
	renderer = nullptr;
	close_window();
	game->chunks.renderer_switch();
	
	open_window();

	glfwSetWindowMonitor(window, NULL, window_pos.pos.x, window_pos.pos.y,
		window_pos.dim.x, window_pos.dim.y, GLFW_DONT_CARE);

	g_window.renderer = start_renderer(g_window.render_backend, g_window.window);
}

void Window::run () {
	open_window(); // open window first to get feedback when clicking exe

	g_assets = Assets::load(); // load assets before renderer so renderer can rely on assets in ctor
	g_window.renderer = start_renderer(g_window.render_backend, g_window.window);

	g_window.game = std::make_unique<Game>();

	glfw_input_pre_gameloop(g_window);

	g_window.input.dt = 0; // dt zero on first frame
	uint64_t prev = get_timestamp();

	//// Frameloop
	for (;;) {
		FrameMark;

		kiss::ChangedFiles changed_files;
		{
			ZoneScopedN("file_changes.poll_changes()");
			changed_files = g_window.file_changes.poll_changes();
		}

		if (switch_render_backend) {
			switch_renderer();
			switch_render_backend = false;
		}

		// Begin frame (aquire image in vk)
		if (g_window.renderer)
			g_window.renderer->frame_begin(g_window.window, g_window.input, changed_files);

		{ // Input sampling
			ZoneScopedN("sample_input");

			g_window.input.clear_frame_input();
			glfwPollEvents();

			if (glfwWindowShouldClose(g_window.window))
				break;

			glfw_sample_non_callback_input(g_window);
		}

		imgui_begin_frame(g_window);

		// Update

		g_window.game->imgui(g_window, g_window.input, g_window.renderer.get());

		if (g_imgui.show_demo_window)
			ImGui::ShowDemoWindow(&g_imgui.show_demo_window);

		g_window.game->update(g_window, g_window.input);

		// Render
		imgui_end_frame();
		if (g_window.renderer)
			g_window.renderer->render_frame(g_window.window, g_window.input, *g_window.game);

		{ // Calc next frame dt based on this frame duration
			auto& i = g_window.input;

			uint64_t now = get_timestamp();
			i.real_dt = (float)(now - prev) / (float)timestamp_freq;
			i.dt = min(i.real_dt * (i.pause_time ? 0 : i.time_scale), i.max_dt);
			i.unscaled_dt = min(i.real_dt, i.max_dt);
			prev = now;
		}

		g_window.frame_counter++;
	}

	renderer = nullptr;
	close_window();
	g_window.game = nullptr;
}

#ifdef WIN32
	#ifdef CONSOLE_SUBSYS
int main ()
	#else
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
	#endif
#endif
{
	if (!app_init())
		return 1;

	g_window.run();

	app_shutdown();
	return 0;
}
