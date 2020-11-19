#include "dear_imgui.hpp"
#include "engine/window.hpp"
#include "engine/input.hpp"
#include "vulkan/vulkan.hpp"
#include "kisslib/string.hpp"
#include "kisslib/kissmath_colors.hpp"
using namespace kissmath;

void Logger::imgui () {
	if (!shown) return;

	ZoneScoped;

	if (imgui_uncollapse)
		ImGui::SetNextWindowCollapsed(false);
	imgui_uncollapse = false;

	ImGui::Begin("Logger", &shown);

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

	ImGui::BeginChild("Log lines", ImVec2(0, 0), true);

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

void Logger::add_line (Line const& line) {
	lines.push_back(line);

	added_this_frame++;

	if (line.level == ERROR) {
		imgui_uncollapse = true;
		shown = true;
	}
}

void vlogf (LogLevel level, char const* format, va_list vl) {
	char new_format[1024];
	snprintf(new_format, sizeof(new_format), "[%5d] %s\n", g_window->frame_counter, format);

	char str[4096];
	vsnprintf(str, sizeof(str), new_format, vl);

#if DEBUGLEVEL >= 2 && defined(CONSOLE_SUBSYS)
	// puts() is too slow to use in a game?, seen in profiler often taking >1.5ms to execute, not sure why
	fputs(str, level == ERROR || level == WARNING ? stdout : stderr);
#endif

	Logger::Line l;
	l.level = level;
	l.frame = g_window->frame_counter;

	char* cur = str;
	while (*cur != '\0') {
		char* end = strchr(cur, '\n');
		if (!end) end = cur + strlen(cur);

		size_t len = min(sizeof(l.str)-1, end - cur);
		memcpy(l.str, cur, len);
		l.str[len] = '\0';

		cur += len + (*end == '\0' ? 0 : 1); // skip newline

		g_logger.add_line(l);
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
