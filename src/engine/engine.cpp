#include "common.hpp"
#include "engine.hpp"
#include "kisslib/clean_windows_h.hpp"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "opengl/opengl_renderer.hpp"
#include "opengl/opengl_helper.hpp"

#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include "GLFW/glfw3native.h"

inline constexpr int2 MIN_WINDOW_SIZE = 128;

inline int _vsync_on_interval = 1;
void Engine::set_vsync (bool vsync) {
	this->vsync = vsync;
	glfwSwapInterval(vsync ? _vsync_on_interval : 0);
}

//// Imgui stuff

inline void imgui_style () {
	auto& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	colors[ImGuiCol_Text]                   = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
	colors[ImGuiCol_TextDisabled]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	colors[ImGuiCol_WindowBg]               = ImVec4(0.09f, 0.09f, 0.11f, 0.83f);
	colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg]                = ImVec4(0.11f, 0.11f, 0.14f, 0.92f);
	colors[ImGuiCol_Border]                 = ImVec4(0.50f, 0.50f, 0.50f, 0.50f);
	colors[ImGuiCol_BorderShadow]           = ImVec4(0.05f, 0.06f, 0.07f, 0.80f);
	colors[ImGuiCol_FrameBg]                = ImVec4(0.43f, 0.43f, 0.43f, 0.39f);
	colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.47f, 0.47f, 0.69f, 0.40f);
	colors[ImGuiCol_FrameBgActive]          = ImVec4(0.42f, 0.41f, 0.64f, 0.69f);
	colors[ImGuiCol_TitleBg]                = ImVec4(0.27f, 0.27f, 0.54f, 0.83f);
	colors[ImGuiCol_TitleBgActive]          = ImVec4(0.32f, 0.32f, 0.63f, 0.87f);
	colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.40f, 0.40f, 0.80f, 0.20f);
	colors[ImGuiCol_MenuBarBg]              = ImVec4(0.40f, 0.40f, 0.55f, 0.80f);
	colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.20f, 0.25f, 0.30f, 0.60f);
	colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.40f, 0.40f, 0.80f, 0.30f);
	colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.40f, 0.40f, 0.80f, 0.40f);
	colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.41f, 0.39f, 0.80f, 0.60f);
	colors[ImGuiCol_CheckMark]              = ImVec4(0.90f, 0.90f, 0.90f, 0.50f);
	colors[ImGuiCol_SliderGrab]             = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
	colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.41f, 0.39f, 0.80f, 0.60f);
	colors[ImGuiCol_Button]                 = ImVec4(0.35f, 0.40f, 0.61f, 0.62f);
	colors[ImGuiCol_ButtonHovered]          = ImVec4(0.40f, 0.48f, 0.71f, 0.79f);
	colors[ImGuiCol_ButtonActive]           = ImVec4(0.46f, 0.54f, 0.80f, 1.00f);
	colors[ImGuiCol_Header]                 = ImVec4(0.40f, 0.40f, 0.90f, 0.45f);
	colors[ImGuiCol_HeaderHovered]          = ImVec4(0.45f, 0.45f, 0.90f, 0.80f);
	colors[ImGuiCol_HeaderActive]           = ImVec4(0.53f, 0.53f, 0.87f, 0.80f);
	colors[ImGuiCol_Separator]              = ImVec4(0.50f, 0.50f, 0.50f, 0.60f);
	colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.60f, 0.60f, 0.70f, 1.00f);
	colors[ImGuiCol_SeparatorActive]        = ImVec4(0.70f, 0.70f, 0.90f, 1.00f);
	colors[ImGuiCol_ResizeGrip]             = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
	colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.78f, 0.82f, 1.00f, 0.60f);
	colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.78f, 0.82f, 1.00f, 0.90f);
	colors[ImGuiCol_Tab]                    = ImVec4(0.34f, 0.34f, 0.68f, 0.79f);
	colors[ImGuiCol_TabHovered]             = ImVec4(0.45f, 0.45f, 0.90f, 0.80f);
	colors[ImGuiCol_TabActive]              = ImVec4(0.40f, 0.40f, 0.73f, 0.84f);
	colors[ImGuiCol_TabUnfocused]           = ImVec4(0.28f, 0.28f, 0.57f, 0.82f);
	colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.35f, 0.35f, 0.65f, 0.84f);
	colors[ImGuiCol_DockingPreview]         = ImVec4(0.40f, 0.40f, 0.90f, 0.31f);
	colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_PlotLines]              = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.27f, 0.27f, 0.38f, 1.00f);
	colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.45f, 1.00f);
	colors[ImGuiCol_TableBorderLight]       = ImVec4(0.26f, 0.26f, 0.28f, 1.00f);
	colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.29f);
	colors[ImGuiCol_TableRowBgAlt]          = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
	colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.00f, 0.00f, 1.00f, 0.35f);
	colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight]           = ImVec4(0.45f, 0.45f, 0.90f, 0.80f);
	colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);


	style.WindowPadding     = ImVec2(5,5);
	style.FramePadding      = ImVec2(6,2);
	style.CellPadding       = ImVec2(4,2);
	style.ItemSpacing       = ImVec2(12,3);
	style.ItemInnerSpacing  = ImVec2(3,3);
	style.IndentSpacing     = 18;
	style.GrabMinSize       = 14;

	style.WindowRounding    = 3;
	style.FrameRounding     = 6;
	style.PopupRounding     = 3;
	style.GrabRounding      = 6;

	style.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
}

void imgui_setup (Engine& eng) {
	ZoneScoped;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	
	imgui_style();

	ImGui_ImplGlfw_InitForOpenGL(eng.window, true);
}
void imgui_shutdown (Engine& eng) {
	ZoneScoped;
	
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}
void imgui_begin_frame (Engine& eng) {
	ZoneScoped;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame(eng.imgui_enabled && eng.input.cursor_enabled);

	auto& io = ImGui::GetIO();
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	if (io.WantCaptureKeyboard)
		eng.input.disable_keyboard();
	if (io.WantCaptureMouse)
		eng.input.disable_mouse();

	ImGui::NewFrame();
}
void imgui_end_frame (Engine& eng) {
	ZoneScoped;
	//OGL_TRACE("imgui_draw");

	ImGui::Render();

	if (GLAD_GL_ARB_framebuffer_sRGB)
		glDisable(GL_FRAMEBUFFER_SRGB);

	if (eng.imgui_enabled)
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	
	if (GLAD_GL_ARB_framebuffer_sRGB)
		glEnable(GL_FRAMEBUFFER_SRGB);
}

void do_imgui (Engine& eng) {
	if (!eng.imgui_enabled)
		return; // This could stop imgui rendering and interaction as long as you don't submit any imgui calls outside of the game imgui function

	ZoneScopedN("imgui");

	// TODO: Docked window has wrong alpha? ie. too little alpha
	// related?: https://github.com/ocornut/imgui/issues/5634
	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
	
	if (ImGui::Begin("Misc")) {
		{
			bool changed = ImGui::Checkbox("fullscreen", &eng.fullscreen);
			ImGui::SameLine();
			changed = ImGui::Checkbox("borderless", &eng.borderless_fullscreen) || changed;

			if (changed)
				eng.set_fullscreen(eng.fullscreen, eng.borderless_fullscreen);
		}

		ImGui::SameLine();
		bool vsync = eng.vsync;
		if (ImGui::Checkbox("Vsync", &vsync)) {
			eng.set_vsync(vsync);
		}
		
		ImGui::SameLine();
		ImGui::Checkbox("Logger", &g_logger.shown);
		g_logger.imgui();

	#if IMGUI_DEMO
		ImGui::SameLine();
		ImGui::Checkbox("ImGui Demo", &eng.imgui_show_demo_window);
	#endif
		
		ImGui::SameLine(0, 20);
		if (ImGui::Button("exit"))
			eng.close();
		
		//if (eng.imgui_show_demo_window)
		//	ImGui::ShowDemoWindow(&eng.imgui_show_demo_window);
		
		eng.trigger_screenshot = ImGui::Button("Screenshot [F8]") || eng.input.buttons[KEY_F8].went_down;
		ImGui::SameLine();
		ImGui::Checkbox("With HUD", &eng.screenshot_hud);

		ImGui::Text("debug.json:");
		ImGui::SameLine();
		if (ImGui::Button("Load [;]") || eng.input.buttons[KEY_SEMICOLON].went_down)
			eng.json_load();
		ImGui::SameLine();
		if (ImGui::Button("Save [']") || eng.input.buttons[KEY_APOSTROPHE].went_down)
			eng.json_save();
		
		ImGui::Separator();
		// Always show control "header" at top even when scrolling down
		if (ImGui::BeginChild("_BodyRegion")) {

			if (imgui_Header("Performance")) {
				eng.fps_display.push_timing(eng.input.real_dt);
				eng.fps_display.imgui_display("framerate", eng.input.real_dt, true);
			
				//ImGui::Text("Chunks drawn %4d / %4d", world->chunks.chunks.count() - world->chunks.count_culled, world->chunks.chunks.count());
				ImGui::PopID();
			}

			eng.input.imgui();

			// Game imgui
			eng.imgui();
		}
		ImGui::EndChild();
	}
	ImGui::End();

	if (eng._should_close == Engine::CLOSE_PENDING) {
		eng._should_close = eng.close_confirmation();
	}
	
#if IMGUI_DEMO
	if (eng.imgui_show_demo_window)
		ImGui::ShowDemoWindow(&eng.imgui_show_demo_window);
#endif
}

//// GLFW & Opengl setup

#if OGL_USE_DEDICATED_GPU
// https://stackoverflow.com/questions/6036292/select-a-graphic-device-in-windows-opengl?noredirect=1&lq=1
// needed to make laptop use dedicated gpu for this app
extern "C" {
	_declspec(dllexport) DWORD NvOptimusEnablement = 1;
	_declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

void create_cursors (Engine& eng);

void window_setup (Engine& eng, char const* window_title) {
	ZoneScoped;
	log("Window setup...");

	{
		ZoneScopedN("glfwInit");
		if (!glfwInit()) {
			fatal_error("glfwInit error!");
		}
	}

	create_cursors(eng);
	
	{
		ZoneScopedN("create window");

		glfwWindowHint(GLFW_RESIZABLE, 1);
		glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);

		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
		//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, 1); // glLineWidth generated GL_INVALID_VALUE with GLFW_OPENGL_FORWARD_COMPAT

		// Need Opengl 4.3 for QOL features, hopefully any modern machine supports this
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

	#if RENDERER_WINDOW_FBO_NO_DEPTH
		glfwWindowHint(GLFW_DEPTH_BITS, 0);
	#endif
		glfwWindowHint(GLFW_STENCIL_BITS, 0);

	#if RENDERER_DEBUG_OUTPUT
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
	#endif
		glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
		//glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
		
		{
			ZoneScopedN("glfwCreateWindow");
			eng.window = glfwCreateWindow(eng.window_size.x, eng.window_size.y, window_title, NULL, NULL);
			if (!eng.window) {
				fatal_error("glfwCreateWindow error!");
			}
		}

		// Set minimum window size to avoid problems in renderer with postprocessing crashing due to too little mip levels (bloom currently)
		glfwSetWindowSizeLimits(eng.window, MIN_WINDOW_SIZE.x, MIN_WINDOW_SIZE.y, GLFW_DONT_CARE, GLFW_DONT_CARE);
	}

	if (glfwRawMouseMotionSupported())
		glfwSetInputMode(eng.window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);

	glfw_register_input_callbacks(eng);
	
	imgui_setup(eng);
}
void window_shutdown (Engine& eng) {
	ZoneScoped;

	imgui_shutdown(eng);
	glfwTerminate();
}

void update_files_changed (Engine& eng) {
	ZoneScopedN("update_files_changed");
	
	// could poll this less frequently if it were to block the main thread significantly
	// but that would just cause stutter every N frames
	// if the OS were that bad I would be forced to run this in a background thread instead
	
	kiss::ChangedFiles changed_files;
	{
		ZoneScopedN("file_changes.poll_changes()");
		changed_files = eng.file_changes.poll_changes();
	}
	
	if (changed_files.any()) {
		ZoneScopedN("file change detected");
		
		//// Repeat reloading of assets because reacting to filechanges often fails because of half-written files
		//int max_retries = 100;
		//int retry_delay = 10; // ms
		//
		//for (int i=0; i<max_retries; ++i) {
		//	Sleep(retry_delay); // start out with a delay in hopes of getting a working file the first time
			
			//bool success = gl::g_shaders.update_recompilation(changed_files);
			bool success = eng.update_files_changed(changed_files);// && success;
			
			/*
			if (changed_files.any_starts_with("textures/", FILE_ADDED|FILE_MODIFIED|FILE_RENAMED_NEW_NAME)) {
				log(INFO, "[OpenglRenderer] Reload textures due to file change");
				try_reloading([&] () { return load_static_data(); });
			}*/
			
		//	if (success)
		//		break; // success
		//}
	}
}

//// fullscreen mode
struct Monitor {
	GLFWmonitor* monitor;
	GLFWvidmode const* vidmode;
	int2	 pos;
	int2	 size;
};

bool select_monitor_from_window_pos (int2 pos, int2 size, Monitor* selected_monior) {
	int count;
	auto** glfw_monitors = glfwGetMonitors(&count);

	std::vector<Monitor> monitors;
	monitors.resize(count);

	auto window_monitor_overlap = [=] (Monitor const& mon) {
		int2 a = clamp(pos, mon.pos, mon.pos + mon.size);
		int2 b = clamp(pos + size, mon.pos, mon.pos + mon.size);

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
bool Engine::set_fullscreen (bool fullscreen, bool borderless_fullscreen) {
	ZoneScoped;

	if (fullscreen) {
		// save window pos
		glfwGetWindowPos(window, &window_pos.x, &window_pos.y);
		glfwGetWindowSize(window, &window_size.x, &window_size.y);
		
		Monitor monitor;
		if (!select_monitor_from_window_pos(window_pos, window_size, &monitor))
			return false; // fail

		if (borderless_fullscreen) {
			glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
			glfwSetWindowMonitor(window, NULL, monitor.pos.x, monitor.pos.y, monitor.size.x, monitor.size.y, GLFW_DONT_CARE);
		} else {
			glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
			glfwSetWindowMonitor(window, monitor.monitor, 0,0, monitor.vidmode->width, monitor.vidmode->height, monitor.vidmode->refreshRate);
		}
	}
	else {
		// restore window pos
		glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
		glfwSetWindowMonitor(window, NULL, window_pos.x, window_pos.y, window_size.x, window_size.y, GLFW_DONT_CARE);
	}

	// reset vsync since it sometimes broke
	set_vsync(vsync);

	this->fullscreen = fullscreen;
	this->borderless_fullscreen = borderless_fullscreen;
	return true;
}
bool Engine::toggle_fullscreen () {
	return set_fullscreen(!fullscreen, borderless_fullscreen);
}

void Engine::close () {
	//glfwSetWindowShouldClose(window, 1);
	_should_close = CLOSE_PENDING;
}

//// Input stuff
void Input::set_cursor_mode (Engine& eng, bool enabled) {
	cursor_enabled = enabled;

	if (enabled)
		glfwSetInputMode(eng.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); // Cursor enabled, can interact with Imgui
	else
		glfwSetInputMode(eng.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // Cursor disabled & Imgui interaction disabled, all controls go to game
}

void create_cursors (Engine& eng) {
	eng._cursors[Engine::CURSOR_NORMAL] = nullptr;
	eng._cursors[Engine::CURSOR_FINGER] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
}
void Engine::set_cursor (CursorMode mode) {
	// Don't set cursor when imgui already sets it (imgui seems to set it every frame like expected)
	// -> we should to set it every frame as well
	if (!ImGui::GetIO().WantCaptureMouse) {
		glfwSetCursor(window, _cursors[mode]);
	}
}

////
void glfw_input_pre_gameloop (Engine& eng) {
	// Get initial mouse position
	glfwGetCursorPos(eng.window, &eng.input._prev_mouse_pos_x, &eng.input._prev_mouse_pos_y);

	// Set initial cursor mode
	eng.input.set_cursor_mode(eng, eng.input.cursor_enabled);
	eng.input._prev_cursor_enabled = eng.input.cursor_enabled;

	eng.input.real_dt = 0; // dt zero on first frame
	eng.input.frame_begin_ts = get_timestamp();
}
void glfw_sample_non_callback_input (Engine& eng) {
	ZoneScoped;

	int2 sz = 0;
	glfwGetFramebufferSize(eng.window, &sz.x, &sz.y);
	eng.input.window_size = max(sz, MIN_WINDOW_SIZE);
	
	double x, y;
	glfwGetCursorPos(eng.window, &x, &y);

	eng.input.cursor_pos = float2((float)x, (float)y);

	//logf("cursor_pos: %f %f\n", input.cursor_pos.x, input.cursor_pos.y);
}

//// Callbacks
void glfw_key_event (GLFWwindow* wnd, int key, int scancode, int action, int mods) {
	assert(action == GLFW_PRESS || action == GLFW_RELEASE || action == GLFW_REPEAT);
	auto& eng = *(Engine*)glfwGetWindowUserPointer(wnd);
	auto& I = eng.input;

	bool went_down =	action == GLFW_PRESS;
	bool went_up =		action == GLFW_RELEASE;

	bool alt =			(mods & GLFW_MOD_ALT) != 0;

	// Toggle fullscreen with F11 or CTRL-ENTER
	if (key == GLFW_KEY_F11 || (alt && key == GLFW_KEY_ENTER)) {
		if (went_down) eng.toggle_fullscreen();
		return;
	}

	// Toggle Imgui visibility with F1
	if (key == GLFW_KEY_F1) {
		if (went_down) eng.imgui_enabled = !eng.imgui_enabled;
		return;
	}

	// Toggle between Imgui interaction and game control
	if (key == GLFW_KEY_F2) {
		if (went_down) I.toggle_cursor_mode(eng);
		return;
	}

	// only process keys after GLFW_KEY_SPACE (32) to allow me to pack mouse buttons into the same array
	if ((went_down || went_up) && key >= GLFW_KEY_SPACE && key <= GLFW_KEY_LAST) {
		I.buttons[key].is_down = went_down;
		I.buttons[key].went_down = went_down;
		I.buttons[key].went_up = went_up;
	}
}
void glfw_char_event (GLFWwindow* wnd, unsigned int codepoint, int mods) {
	auto& I = ((Engine*)glfwGetWindowUserPointer(wnd))->input;

	// for typing input
}
void glfw_mouse_button_event (GLFWwindow* wnd, int button, int action, int mods) {
	assert(action == GLFW_PRESS || action == GLFW_RELEASE);
	auto& I = ((Engine*)glfwGetWindowUserPointer(wnd))->input;

	bool went_down = action == GLFW_PRESS;
	bool went_up =	 action == GLFW_RELEASE;

	if ((went_down || went_up) && button >= GLFW_MOUSE_BUTTON_1 && button <= GLFW_MOUSE_BUTTON_8) {
		// offset mouse button, see input_buttons.hpp
		I.buttons[button+1].is_down = went_down;
		I.buttons[button+1].went_down = went_down;
		I.buttons[button+1].went_up = went_up;
	}
}

// The initial event seems to report the same position as our initial glfwGetCursorPos, so that delta is fine
// But when toggling the cursor from disabled to visible cursor jumps back to the prev position, and an event reports this as delta so we need to discard this 
void glfw_mouse_move_event (GLFWwindow* wnd, double xpos, double ypos) {
	auto& I = ((Engine*)glfwGetWindowUserPointer(wnd))->input;

	float2 delta = float2((float)(xpos - I._prev_mouse_pos_x), (float)(ypos - I._prev_mouse_pos_y));
	delta.y = -delta.y; // convert to bottom up

	I._prev_mouse_pos_x = xpos;
	I._prev_mouse_pos_y = ypos;

	bool cursor_enabled = glfwGetInputMode(wnd, GLFW_CURSOR) != GLFW_CURSOR_DISABLED;
	bool cursor_toggled = cursor_enabled != I._prev_cursor_enabled;
	I._prev_cursor_enabled = cursor_enabled;

	bool discard_delta = cursor_toggled;

	//logf("glfw_mouse_move_event: %7d: %f %f%s\n", frame_counter, delta.x, delta.y, discard_delta ? " (discarded)":"");

	if (!discard_delta)
		I.mouse_delta += delta;
}
void glfw_mouse_scroll (GLFWwindow* wnd, double xoffset, double yoffset) {
	auto& I = ((Engine*)glfwGetWindowUserPointer(wnd))->input;

	// assume int, if glfw_mouse_scroll ever gives us 0.2 for ex. this might break
	// But the gameplay code wants to assume mousewheel moves in "clicks", for item swapping
	// I've personally never seen a mousewheel that does not move in "clicks" anyway
	I.mouse_wheel_delta += (int)ceil(abs(yoffset)) * (int)normalizesafe((float)yoffset); // -1.1f => -2    0 => 0    0.3f => +1
}

// need to make sure window_frame only gets called if the (series of) glfw_window_size_event
// was the result of a glfwPollEvents() call (which blocks until resizing is done, forcing me to do this in the first place)
// glfw_window_size_event can also be emitted by switch_fullscreen for example
// which casues recursive window_frame() calls
// TODO: should switch_fullscreen even happen during resizing? why do we get input callbacks if we don't do glfwPollEvents?
//      -> probably due to windows message queue weirdness (which is why we need this in the first place, blocking glfwPollEvents is just stupid)
//      -> just leave this in since it fixes it perfectly

void window_frame (Engine& eng);

GuiConfirm should_close;

// enable drawing frames when resizing the window
void glfw_window_size_event (GLFWwindow* wnd, int width, int height) {
	auto* eng = (Engine*)glfwGetWindowUserPointer(wnd);
	
	if (!eng->_draw_on_size_events) return;

	window_frame(*eng);
}

void glfw_register_input_callbacks (Engine& eng) {
	glfwSetWindowUserPointer(eng.window, &eng);

	glfwSetKeyCallback        (eng.window, glfw_key_event);
	glfwSetCharModsCallback   (eng.window, glfw_char_event);
	glfwSetCursorPosCallback  (eng.window, glfw_mouse_move_event);
	glfwSetMouseButtonCallback(eng.window, glfw_mouse_button_event);
	glfwSetScrollCallback     (eng.window, glfw_mouse_scroll);
	glfwSetWindowSizeCallback (eng.window, glfw_window_size_event);

	// These are not called initially
	glfwGetWindowSize(eng.window, &eng.window_size.x, &eng.window_size.y);
}

void pause_while_window_not_visible (Engine& eng) {
	for (;;) {
		bool visible = !glfwGetWindowAttrib(eng.window, GLFW_ICONIFIED) && // iconified == minimized
						glfwGetWindowAttrib(eng.window, GLFW_VISIBLE);
		// If window minimized or made invisible don't render
		// achieve this by only returning to frame function if visible
		if (visible)
			return;

		// otherwise loop
		// but dont busy wait and waste cpu, instead go into event driven mode (wait until some event and recheck visibility)
		// we might be woken up by other things than being made visible, but not sure how to avoid this, this shouldn't cause much cpu use though
		
		// I'm unsure how this interacts with glfw_window_size_event (rendering on resize)
		// we would end up calling glfwWaitEvents from inside glfwPollEvents, which is just weird
		// it seems like this works fine though, we don't get woken up until the window is maximized after minimizing,
		// presumably because minimized windows don't get any other events
		glfwWaitEvents();

		eng._was_not_visible_and_paused = true;
	}
}

////
void window_frame (Engine& eng) {
	pause_while_window_not_visible(eng);

	FrameMark;

	glfw_sample_non_callback_input(eng);

	update_files_changed(eng);

	imgui_begin_frame(eng);
	eng._need_to_draw_imgui = true;

	do_imgui(eng);

	eng.frame();

	eng.draw_imgui(); // only draws if still _need_to_draw_imgui

	eng.input.clear_frame_input();

	eng.input.get_time(eng._was_not_visible_and_paused);
	eng._was_not_visible_and_paused = false;

	eng.input.frame_counter++;

	TracyGpuCollect;
}

void Engine::draw_imgui () {
	if (!_need_to_draw_imgui) return;
	imgui_end_frame(*this);
	_need_to_draw_imgui = false;
}

//void clear_window_color (GLFWwindow* window) {
//	glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
//	glClear(GL_COLOR_BUFFER_BIT);
//	glfwSwapBuffers(window);
//}

Engine::Engine (const char* window_title) {
	ZoneScoped;

	log("Engine startup...");

	window_setup(*this, window_title);
	
	//clear_window_color(window); // hide the ugly white rectangle on black background that happens due to loading time of the game

	{ // load window placement
		HWND hwnd = glfwGetWin32Window(window);
		WINDOWPLACEMENT pl;
		if (kiss::load_binary_file("window_placement.bin", &pl, sizeof(pl))) {
			SetWindowPlacement(hwnd, &pl);
		}
	}

	//clear_window_color(window);
}
Engine::~Engine () {
	ZoneScoped;

	log("Engine shutdown...");

	{ // save window placement
		// swtich back to windowed so we don't save bogus fullscreen borderless placement
		if (fullscreen)
			set_fullscreen(false, borderless_fullscreen);

		HWND hwnd = glfwGetWin32Window(window);
		WINDOWPLACEMENT pl = {};
		if (GetWindowPlacement(hwnd, &pl)) {
			kiss::save_binary_file("window_placement.bin", &pl, sizeof(pl));
		}
	}
	
	window_shutdown(*this);
}

int Engine::main_loop () {
	log("Starting main loop...");

	glfw_input_pre_gameloop(*this);
	
	while (_should_close != CLOSE_NOW) {
		{
			ZoneScopedN("glfwPollEvents");
			_draw_on_size_events = true;
			glfwPollEvents();
			_draw_on_size_events = false;
		}

		if (glfwWindowShouldClose(window)) {
			_should_close = CLOSE_PENDING;
			imgui_enabled = true; // need to enable imgui for this to work properly
			glfwSetWindowShouldClose(window, false); // keep state ourselves
		}
		
		window_frame(*this);
	}

	return 0;
}

