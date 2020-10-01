#pragma once
#include "imgui.h"

#include "misc/cpp/imgui_stdlib.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "../../kisslib/kissmath.hpp"

#include <string>
#include <vector>

namespace vk { struct Renderer; }

struct DearImgui {
	bool enabled = true;

	bool show_demo_window = false;

	int tree_depth = 0;

	void init (vk::Renderer& renderer);

	void frame_start ();
	void frame_end (VkCommandBuffer buf);

	void destroy (vk::Renderer& renderer);
};

inline DearImgui imgui;

// root level is closed by default, sublevels are open by default
//  for printing inherited baseclasses in their derived classes
inline bool imgui_push (const char* class_name, const char* instance_name=nullptr, bool inner_default_open=true) {
	bool ret;
	if (instance_name)
		ret = ImGui::TreeNodeEx(instance_name, imgui.tree_depth != 0 && inner_default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s <%s>", instance_name, class_name);
	else
		ret = ImGui::TreeNodeEx(class_name, imgui.tree_depth != 0 && inner_default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0, "<%s>", class_name);

	if (ret) imgui.tree_depth++;
	return ret;
}
inline void imgui_pop () {
	ImGui::TreePop();
	imgui.tree_depth--;
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
	LOG,
	INFO,
	WARNING,
	ERROR,
};
static inline const lrgba LOG_LEVEL_COLS[] = {
	srgba(216, 216, 216, 120),
	srgba(234, 234, 232),
	srgba(255, 220, 80),
	srgba(255, 100, 40),
};

class GuiConsole {
	bool imgui_uncollapse = false;
public:
	struct Line {
		char str[128 - sizeof(int)*2];
		LogLevel level;
		int frame;
	};

	std::vector<Line> lines;
	bool shown = true;

	bool show_levels[4] = { 0,1,1,1 };
	int max_display_lines = 5000;

	int added_this_frame = 0;
	int counter = 0;

	void imgui ();

	void add_line (Line const& line);
};

// Global GuiConsole
extern GuiConsole gui_console;

// log() function name is already taken by <cmath>

// log to console
void clog (char const* format, ...);

// log to console
void clog (LogLevel level, char const* format, ...);
