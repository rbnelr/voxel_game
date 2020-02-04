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
