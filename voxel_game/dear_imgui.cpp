#include "dear_imgui.hpp"
#include "glfw_window.hpp"
#include "util/string.hpp"
#include "input.hpp"
#include "kissmath_colors.hpp"
using namespace kissmath;

#include "optick.h"

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
	int size = (int)unimportant_lines.capacity();
	if (ImGui::DragInt("buffer size", &size, 0.5f, 1, 10000)) {
		unimportant_lines.resize(size);
		important_lines.resize(size);
	}

	ImGui::SameLine();
	ImGui::Checkbox("INFO", &show_info);
	ImGui::SameLine();
	ImGui::Checkbox("WARN", &show_warn);
	ImGui::SameLine();
	ImGui::Checkbox("ERR", &show_err);

	ImGui::BeginChild("Console lines", ImVec2(0, 0), true);

	bool autoscroll = ImGui::GetScrollMaxY() == ImGui::GetScrollY();

	int ai=0, bi=0;
	auto get_line = [&] () -> Line* {
		auto* a = ai < unimportant_lines.count() ? &unimportant_lines.get_oldest(ai) : nullptr;
		auto* b = bi <   important_lines.count() ? &  important_lines.get_oldest(bi) : nullptr;

		if (!a && !b) return nullptr;

		if (!a) { ++bi; return b; }
		if (!b) { ++ai; return a; }

		if (a->counter < b->counter) { ++ai; return a; }
		else {						   ++bi; return b; }
	};

	Line* line;
	while ((line = get_line())) {
		if (line->level == INFO    && !show_info) continue;
		if (line->level == WARNING && !show_warn) continue;
		if (line->level == ERROR   && !show_err ) continue;

		lrgba col = srgba(250);
		if (line->level == WARNING)		col = srgba(255, 220, 80);
		else if (line->level == ERROR)	col = srgba(255, 100, 40);

		ImGui::TextColored(ImVec4(col.x, col.y, col.z, col.w), line->str);
	}

	if (autoscroll) // keep scroll set to end of console buffer if it was at the end previously
		ImGui::SetScrollHereY();
	else
		ImGui::SetScrollY( ImGui::GetScrollY() - ImGui::GetTextLineHeightWithSpacing() * (float)added_this_frame );

	ImGui::EndChild();

	ImGui::End();

	added_this_frame = 0;
}

void GuiConsole::add_line (Line line) {
	//OPTICK_EVENT();

	auto* ls = line.level == INFO ? &unimportant_lines : &important_lines;
	
	ls->push(std::move(line));

	added_this_frame++;
	counter++;

	if (line.level == ERROR) {
		imgui_uncollapse = true;
		shown = true;
	}
}

extern int frame_counter;

void vlogf (LogLevel level, char const* format, va_list vl) {
	OPTICK_EVENT("vlogf");

	char new_format[128];
	snprintf(new_format, sizeof(new_format), "[%5d] %s\n", frame_counter, format);
	
	GuiConsole::Line l;
	vsnprintf(l.str, sizeof(l.str), new_format, vl);

	/* puts is too slow to use in a game!!, often taking >1.5ms to execute
	{
		OPTICK_EVENT("vlogf fputs");
		fputs(line.c_str(), level == ERROR || level == WARNING ? stdout : stderr);
	}*/

	l.level = level;
	l.frame = frame_counter;
	gui_console.add_line(std::move(l));
}
void logf (char const* format, ...) {
	va_list vl;
	va_start(vl, format);

	vlogf(INFO, format, vl);

	va_end(vl);
}
void logf (LogLevel level, char const* format, ...) {
	va_list vl;
	va_start(vl, format);

	vlogf(level, format, vl);

	va_end(vl);
}
