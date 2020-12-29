#include "common.hpp"
#include "window.hpp"

#include "GLFW/glfw3.h" // need to include vulkan before glfw because GLFW checks for VK_VERSION_1_0

#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

//// vsync and fullscreen mode
void Window::set_vsync (bool on) {
	//ZoneScopedN("glfwSwapInterval");

	//glfwSwapInterval(on ? _vsync_on_interval : 0);
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

	std_vector<Monitor> monitors;
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

void Window::close () {
	glfwSetWindowShouldClose(window, 1);
}

//// gameloop

void imgui_begin_frame (Window& window) {
	ZoneScoped;

	ImGui_ImplVulkan_NewFrame();
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

	ImGui::PopItemWidth();
	ImGui::End();
}

void glfw_error (int err, const char* msg) {
	fprintf(stderr, "GLFW Error! [0x%x] '%s'\n", err, msg);
}

bool app_init () {
#if DEBUGLEVEL >= 2
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
	{
		ZoneScopedN("glfwCreateWindow");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
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
	{
		ZoneScopedN("glfwDestroyWindow");
		glfwDestroyWindow(window);
	}
	window = nullptr;
}

void Window::switch_renderer () {
	renderer = nullptr;
	close_window();
	open_window();
	g_window.renderer = Renderer::start_renderer(g_window.render_backend, g_window.window);
}

void Window::run () {
	g_assets = Assets::load();

	g_window.game = std::make_unique<Game>();
	g_window.renderer = Renderer::start_renderer(g_window.render_backend, g_window.window);

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

		// Begin frame (aquire image in vk)
		g_window.renderer->frame_begin(g_window.window, changed_files);

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

		g_window.game->imgui(g_window, g_window.input,
			[&] () { },//renderer->graphics_imgui(); },
			[&] () { }//renderer->chunk_renderer.imgui(game->world->chunks); }
		);

		if (g_imgui.show_demo_window)
			ImGui::ShowDemoWindow(&g_imgui.show_demo_window);

		g_window.game->update(g_window, g_window.input);

		// Render
		imgui_end_frame();
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

	g_window.open_window();

	g_window.run();

	g_window.close_window();

	app_shutdown();
	return 0;
}
