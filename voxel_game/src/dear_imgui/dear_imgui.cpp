#include "stdafx.hpp"

#include "dear_imgui.hpp"
#include "glfw_window.hpp"
#include "util/string.hpp"
#include "input.hpp"
#include "kissmath_colors.hpp"
using namespace kissmath;

void DearImgui::init () {
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	// imgui steals keyboard by default, do not like
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();
}

void DearImgui::frame_start () {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame(enabled && glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED);

	{
		auto& io = ImGui::GetIO();
		// imgui steals keyboard by default, do not like
		if (io.WantCaptureKeyboard)
			input.disable_keyboard();
		if (io.WantCaptureMouse)
			input.disable_mouse();
	}

	ImGui::NewFrame();
}
void DearImgui::frame_end () {
	if (show_demo_window)
		ImGui::ShowDemoWindow(&show_demo_window);

	if (enabled) {
		ImGui::Render();

		int display_w, display_h;
		glfwGetFramebufferSize(window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	} else {
		ImGui::EndFrame();
	}
}

void DearImgui::destroy () {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

DearImgui imgui;

int tree_depth = 0;


////
GuiConsole gui_console;

void GuiConsole::imgui () {
	if (!gui_console.shown) return;

	if (imgui_uncollapse)
		ImGui::SetNextWindowCollapsed(false);
	imgui_uncollapse = false;

	ImGui::Begin("Console", &gui_console.shown);

	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.3f);
	
	ImGui::DragInt("display lines", &max_display_lines, 0.5f, 1, 0, ".6d", ImGuiSliderFlags_Logarithmic);
	ImGui::SameLine();
	ImGui::Text("%6.2f KB", (float)lines.size() * sizeof(Line) / 1024);
	ImGui::SameLine();
	if (ImGui::Button("Clear"))
		lines = std::vector<Line>();
	ImGui::SameLine();

	ImGui::SameLine();
	ImGui::Checkbox("LOG", &show_levels[LOG]);
	ImGui::SameLine();
	ImGui::Checkbox("INFO", &show_levels[INFO]);
	ImGui::SameLine();
	ImGui::Checkbox("WARN", &show_levels[WARNING]);
	ImGui::SameLine();
	ImGui::Checkbox("ERR", &show_levels[ERROR]);

	ImGui::BeginChild("Console lines", ImVec2(0, 0), true);

	bool autoscroll = ImGui::GetScrollMaxY() == ImGui::GetScrollY();

	int first_line = max((int)lines.size() - max_display_lines, 0);

	for (int i=first_line; i<(int)lines.size(); ++i) {
		if (!show_levels[lines[i].level]) continue;

		lrgba col = LOG_LEVEL_COLS[lines[i].level];
		ImGui::TextColored(ImVec4(col.x, col.y, col.z, col.w), lines[i].str);
	}

	if (autoscroll) // keep scroll set to end of console buffer if it was at the end previously
		ImGui::SetScrollHereY();
	else
		ImGui::SetScrollY( ImGui::GetScrollY() - ImGui::GetTextLineHeightWithSpacing() * (float)added_this_frame );

	ImGui::EndChild();

	ImGui::End();

	added_this_frame = 0;
}

void GuiConsole::add_line (Line const& line) {
	lines.push_back(line);

	added_this_frame++;

	if (line.level == ERROR) {
		imgui_uncollapse = true;
		shown = true;
	}
}

extern int frame_counter;

void vlogf (LogLevel level, char const* format, va_list vl) {
	char new_format[1024];
	snprintf(new_format, sizeof(new_format), "[%5d] %s\n", frame_counter, format);

	char str[4096];
	vsnprintf(str, sizeof(str), new_format, vl);

#if !NDEBUG
	// puts() is too slow to use in a game?, seen in profiler often taking >1.5ms to execute, not sure why
	fputs(str, level == ERROR || level == WARNING ? stdout : stderr);
#endif

	GuiConsole::Line l;
	l.level = level;
	l.frame = frame_counter;

	char* cur = str;
	while (*cur != '\0') {
		char* end = strchr(cur, '\n');
		if (!end) end = cur + strlen(cur);

		size_t len = min(sizeof(l.str)-1, end - cur);
		memcpy(l.str, cur, len);
		l.str[len] = '\0';

		cur += len + (*end == '\0' ? 0 : 1); // skip newline

		gui_console.add_line(l);
	}
}
void clog (char const* format, ...) {
	va_list vl;
	va_start(vl, format);

	vlogf(INFO, format, vl);

	va_end(vl);
}
void clog (LogLevel level, char const* format, ...) {
	va_list vl;
	va_start(vl, format);

	vlogf(level, format, vl);

	va_end(vl);
}