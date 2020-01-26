#include "dear_imgui.hpp"

#include "dear_imgui/imgui_impl_glfw.h"
#include "dear_imgui/imgui_impl_opengl3.h"

#include "glfw_window.hpp"

void DearImgui::init () {
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init();
}

void DearImgui::frame_start () {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame(enabled && glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED);
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
