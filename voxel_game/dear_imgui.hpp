#pragma once
#include "imgui.h"

#include "misc/cpp/imgui_stdlib.h"
#include "dear_imgui/imgui_impl_glfw.h"
#include "dear_imgui/imgui_impl_opengl3.h"

#include "util/circular_buffer.hpp"
#include "kissmath.hpp"

#include <string>

class DearImgui {

public:
	bool enabled = true;

	bool show_demo_window = false;

	void init ();

	void frame_start ();
	void frame_end ();

	void destroy ();
};

extern DearImgui imgui;

extern int tree_depth;

// root level is closed by default, sublevels are open by default
//  for printing inherited baseclasses in their derived classes
inline bool imgui_push (const char* class_name, const char* instance_name=nullptr, bool inner_default_open=true) {
	bool ret;
	if (instance_name)
		ret = ImGui::TreeNodeEx(instance_name, tree_depth != 0 && inner_default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s <%s>", instance_name, class_name);
	else
		ret = ImGui::TreeNodeEx(class_name, tree_depth != 0 && inner_default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0, "<%s>", class_name);

	if (ret) tree_depth++;
	return ret;
}
inline void imgui_pop () {
	ImGui::TreePop();
	tree_depth--;
}

// Imgui is not srgb correct, ColorEdit3 assume srgb (since color pickers are usually in srgb and should display srgb values as ints because that is more convinient than floats)
//  but the values its displays are the same as are passed to the shader, which assumes linear values, so it's impossible to both have the color picker be in srgb and the color be correct on screen
//  solution -> fix the shader to manually convert srgb to linear??
inline bool imgui_ColorEdit3 (const char* label, float col[3], ImGuiColorEditFlags flags) {
	float3 srgbf = float3( to_srgb(col[0]), to_srgb(col[1]), to_srgb(col[2]) );
	bool ret = ImGui::ColorEdit3(label, &srgbf.x, flags);
	col[0] = to_linear(srgbf.x);
	col[1] = to_linear(srgbf.y);
	col[2] = to_linear(srgbf.z);
	return ret;
}

enum LogLevel {
	INFO	=0,
	WARNING	=10,
	ERROR	=15,
};

class GuiConsole {
	bool imgui_uncollapse = false;
public:
	struct Line {
		char str[128 - sizeof(int)*3];
		LogLevel level;
		int frame;
		int counter;
	};

	circular_buffer<Line> unimportant_lines = circular_buffer<Line>(256);
	circular_buffer<Line> important_lines = circular_buffer<Line>(256);
	bool shown = true;

	bool show_info = false;
	bool show_warn = true;
	bool show_err = true;

	int added_this_frame = 0;
	int counter = 0;

	void imgui ();

	void add_line (Line line);
};

// Global GuiConsole
extern GuiConsole gui_console;

void logf (char const* format, ...);
void logf (LogLevel level, char const* format, ...);
