#include "common.hpp"
#include "imgui/dear_imgui.hpp"
#include "game.hpp"
#include "string.h"

void Logger::imgui () {
	if (!shown) return;

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
	int frame_count = g ? (int)g->input.frame_counter : 0; // handle very first print

	const char* color_code_format = "%s";
	if      (level == WARNING) color_code_format = "\x1B[33m%s\033[0m";
	else if (level == ERROR  ) color_code_format = "\x1B[31m%s\033[0m";

	char new_format[1024];
	snprintf(new_format, sizeof(new_format), "%5d| %s\n", frame_count, format);

	char str[4096];
	vsnprintf(str, sizeof(str), new_format, vl);

#if defined(CONSOLE_SUBSYS)
	fprintf(level == ERROR || level == WARNING ? stdout : stderr, color_code_format, str);
#endif

	Logger::Line l;
	l.level = level;
	l.frame = frame_count;

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
