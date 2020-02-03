#pragma once
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "dear_imgui/imgui_impl_glfw.h"
#include "dear_imgui/imgui_impl_opengl3.h"
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
inline bool imgui_push (std::string const& instance_name, char const* class_name, bool inner_default_open=true) {
	//ImGui::id
	bool ret = ImGui::TreeNodeEx(instance_name.c_str(), tree_depth != 0 && inner_default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s <%s>", instance_name.c_str(), class_name);
	if (ret) tree_depth++;
	return ret;
}
inline void imgui_pop () {
	ImGui::TreePop();
	tree_depth--;
}
