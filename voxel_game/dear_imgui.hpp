#pragma once
#include "imgui.h"
#include "dear_imgui/imgui_impl_glfw.h"
#include "dear_imgui/imgui_impl_opengl3.h"

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
inline bool imgui_treepush (char const* label) {
	bool ret = ImGui::TreeNodeEx(label, tree_depth == 0 ? 0 : ImGuiTreeNodeFlags_DefaultOpen);
	if (ret) tree_depth++;
	return ret;
}
inline void imgui_treepop () {
	ImGui::TreePop();
	tree_depth--;
}
