#include "dear_imgui.hpp"
#include "../window.hpp"
#include "../input.hpp"
#include "../vulkan/vulkan.hpp"
#include "kisslib/string.hpp"
#include "kisslib/kissmath_colors.hpp"
using namespace kissmath;

void DearImgui::init (vk::Renderer& renderer) {
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	// imgui steals keyboard by default, do not like
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Platform/Renderer bindings
	ImGui_ImplGlfw_InitForVulkan(window.window, true);

	ImGui_ImplVulkan_InitInfo info = {};
	info.Instance			= renderer.instance;
	info.PhysicalDevice		= renderer.physical_device;
	info.Device				= renderer.device;
	info.QueueFamily		= renderer.queues.families.graphics_family;
	info.Queue				= renderer.queues.graphics_queue;
	info.PipelineCache		= VK_NULL_HANDLE;
	info.DescriptorPool		= renderer.descriptor_pool;
	info.MinImageCount		= vk::SWAP_CHAIN_SIZE;
	info.ImageCount			= vk::SWAP_CHAIN_SIZE;
	info.MSAASamples		= renderer.max_msaa_samples;
	info.Allocator			= nullptr;
	info.CheckVkResultFn	= nullptr;
	ImGui_ImplVulkan_Init(&info, renderer.render_pass);

	auto cmd_buf = renderer.begin_one_time_commands();

	ImGui_ImplVulkan_CreateFontsTexture(cmd_buf);

	renderer.end_one_time_commands(cmd_buf);
}

void DearImgui::frame_start () {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame(enabled && window.input.cursor_enabled);

	{
		auto& io = ImGui::GetIO();
		// imgui steals keyboard by default, do not like
		if (io.WantCaptureKeyboard)
			window.input.disable_keyboard();
		if (io.WantCaptureMouse)
			window.input.disable_mouse();
	}

	ImGui::NewFrame();
}
void DearImgui::frame_end (VkCommandBuffer buf) {
	if (show_demo_window)
		ImGui::ShowDemoWindow(&show_demo_window);

	if (enabled) {
		ImGui::Render();

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), buf);
	}
}

void DearImgui::destroy (vk::Renderer& renderer) {
	vkQueueWaitIdle(renderer.queues.graphics_queue);

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

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

void vlogf (LogLevel level, char const* format, va_list vl) {
	char new_format[1024];
	snprintf(new_format, sizeof(new_format), "[%5d] %s\n", window.frame_counter, format);

	char str[4096];
	vsnprintf(str, sizeof(str), new_format, vl);

#if !NDEBUG
	// puts() is too slow to use in a game?, seen in profiler often taking >1.5ms to execute, not sure why
	fputs(str, level == ERROR || level == WARNING ? stdout : stderr);
#endif

	GuiConsole::Line l;
	l.level = level;
	l.frame = window.frame_counter;

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
