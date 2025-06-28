#pragma once
#include "kisslib/kissmath.hpp"
#include "kisslib/running_average.hpp"
#include "kisslib/stl_extensions.hpp"
#include "kisslib/containers.hpp"
#include "kisslib/allocator.hpp"
#include "kisslib/string.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include <vector>

namespace ImGui {
	static void HelpMarker (const char* desc) {
		ImGui::TextDisabled("(?)");
		if (ImGui::BeginItemTooltip())
		{
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
			ImGui::TextUnformatted(desc);
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}
}

template <typename T, typename FUNC>
bool imgui_edit_vector (const char* label, std::vector<T>& vec, FUNC item, bool can_resize=true, bool default_open=true, bool subnodes=true) {
	if (!ImGui::TreeNodeEx(label, default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0)) return false;
	
	bool changed = false;

	if (can_resize) {
		int count = (int)vec.size();
		if (ImGui::DragInt("count", &count, 0.1f, 0)) {
			vec.resize(count);
			changed = true;
		}
	}

	if (subnodes) {
		for (int i=0; i<(int)vec.size(); ++i) {
			if (ImGui::TreeNodeEx(&vec[i], 0, "[%d]", i)) {
				changed = item(i, vec[i]) || changed;
				ImGui::TreePop();
			}
		}
	}
	else {
		for (int i=0; i<(int)vec.size(); ++i) {
			ImGui::PushID(i);
			changed = item(i, vec[i]) || changed;
			ImGui::PopID();
		}
	}

	ImGui::TreePop();
	return changed;
}

inline bool imgui_ColorEdit (const char* label, lrgb* col, ImGuiColorEditFlags flags=0) {
	float3 srgbf = float3( to_srgb(col->x), to_srgb(col->y), to_srgb(col->z) );
	bool ret = ImGui::ColorEdit3(label, &srgbf.x, flags);
	*col = float3( to_linear(srgbf.x), to_linear(srgbf.y), to_linear(srgbf.z) );
	return ret;
}
inline bool imgui_ColorEdit (const char* label, lrgba* col, ImGuiColorEditFlags flags=0) {
	float4 srgbaf = float4( to_srgb(col->x), to_srgb(col->y), to_srgb(col->z), col->w ); // alpha is linear
	bool ret = ImGui::ColorEdit4(label, &srgbaf.x, flags);
	*col = float4( to_linear(srgbaf.x), to_linear(srgbaf.y), to_linear(srgbaf.z), srgbaf.w );
	return ret;
}

inline bool imgui_ColorEdit (const char* label, srgb8* col, ImGuiColorEditFlags flags=0) {
	float3 srgbf = (float3)(*col) / 255.0f;
	bool ret = ImGui::ColorEdit3(label, &srgbf.x, flags);
	*col = (srgb8)roundi(srgbf * 255.0f);
	return ret;
}
inline bool imgui_ColorEdit (const char* label, srgba8* col, ImGuiColorEditFlags flags=0) {
	float4 srgbaf = (float4)(*col) / 255.0f;
	bool ret = ImGui::ColorEdit4(label, &srgbaf.x, flags);
	*col = (srgba8)roundi(srgbaf * 255.0f);
	return ret;
}

// needs a PopID()
inline bool imgui_Header (const char* label, bool* is_open=nullptr) {
	bool open = is_open ? *is_open : true;
	auto flags = ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Framed |
	             ImGuiTreeNodeFlags_NoAutoOpenOnLog | (open ? ImGuiTreeNodeFlags_DefaultOpen : 0);
	open = ImGui::TreeNodeEx(label, flags);
	if (open) ImGui::PushID(label);
	if (is_open) *is_open = open;
	return open;
}

enum class GuiConfirm {
	PENDING = 0,
	NO,
	YES,
};
inline GuiConfirm imgui_delete_confirmation (const char* name, bool trigger_open) {
	if (trigger_open)
		ImGui::OpenPopup("POPUP_delete_confirmation");

	GuiConfirm conf = GuiConfirm::PENDING;
	
	if (ImGui::BeginPopup("POPUP_delete_confirmation")) {

		ImGui::Text("Really delete %s?", name);
		
		if (ImGui::Button("Keep")) {
			conf = GuiConfirm::NO;
			ImGui::CloseCurrentPopup();
		}

		ImGui::PushStyleColor(ImGuiCol_Button,        0xFF120CFF);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0xE24E48FF);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive,  0xB8241EFF);
		
		ImGui::SameLine();
		if (ImGui::Button("DELETE")) {
			conf = GuiConfirm::YES;
			ImGui::CloseCurrentPopup();
		}

		ImGui::PopStyleColor(3);

		ImGui::EndPopup();
	}

	return conf;
}

enum LogLevel {
	LOG,
	INFO,
	WARNING,
	ERROR,
};
inline const lrgba LOG_LEVEL_COLS[] = {
	srgba(216, 216, 216, 120),
	srgba(234, 234, 232),
	srgba(255, 220, 80),
	srgba(255, 100, 40),
};

class Logger {
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

inline Logger g_logger;

void vlogf (LogLevel level, char const* format, va_list vl);

inline void log (char const* format, ...) {
	va_list vl;
	va_start(vl, format);

	vlogf(INFO, format, vl);

	va_end(vl);
}
inline void log (LogLevel level, char const* format, ...) {
	va_list vl;
	va_start(vl, format);

	vlogf(level, format, vl);

	va_end(vl);
}

template <typename... Ts>
[[noreturn]] inline void fatal_error (const char* format, Ts... args) {
	log(ERROR, format, args...);
	// throw unhandled exception which dbgbreaks or
	// opens generic abort message box if running normally, so user can read log in (open) console window
	// TODO: hide console window at runtime and log both into file and into imgui/ingame log?
	// show log during fatal_error in message box or somehow unhide console window if hidden?
	throw std::runtime_error(kiss::prints(format, args...));
}

enum class GuiUnsavedConfirm {
	PENDING = 0,
	CANCEL,
	SAVE,
	DISCARD,
};
inline GuiUnsavedConfirm imgui_unsaved_changes_confirmation () {
	ImGui::OpenPopup("POPUP_unsaved_confirmation");

	GuiUnsavedConfirm conf = GuiUnsavedConfirm::PENDING;
	
	auto size = ImGui::GetMainViewport()->Size;
	ImGui::SetNextWindowPos(ImVec2(size.x * 0.5f, size.y * 0.5f), 0, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopup("POPUP_unsaved_confirmation")) {

		ImGui::Text("You have unsaved changes.");
		
		if (ImGui::Button("Cancel")) {
			conf = GuiUnsavedConfirm::CANCEL;
			ImGui::CloseCurrentPopup();
		}
		
		ImGui::SameLine();
		if (ImGui::Button("Save & Close")) {
			conf = GuiUnsavedConfirm::SAVE;
			ImGui::CloseCurrentPopup();
		}

		ImGui::PushStyleColor(ImGuiCol_Button,        0xFF120CFF);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0xE24E48FF);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive,  0xB8241EFF);
		
		ImGui::SameLine();
		if (ImGui::Button("Discard & Close")) {
			conf = GuiUnsavedConfirm::DISCARD;
			ImGui::CloseCurrentPopup();
		}

		ImGui::PopStyleColor(3);

		ImGui::EndPopup();
	}

	return conf;
}

struct Timing_Histogram {
	RunningAverage<float> avg = RunningAverage<float>(64);
	float latest_avg;
	float latest_min, latest_max, latest_std_dev;

	float update_period = .5f; // sec
	float update_timer = 0;

	int imgui_histo_height = 60;
	float y_axis_height = 20;

	// add new timing into circular buffer
	void push_timing (float seconds) {
		avg.push(seconds);
	}

	// compute averages from circular buffer every update_period seconds (only includes pushed timings, not initial 0s)
	// and display them via imgui
	void imgui_display (char const* name, float dt, bool default_open=false) { // dt for updating imgui every update_period (not for actual value being averaged)
		if (update_timer <= 0) {
			latest_avg = avg.calc_avg(&latest_min, &latest_max, &latest_std_dev);
			update_timer += update_period;
		}
		update_timer -= dt;

		if (ImGui::TreeNodeEx(name, default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0,
			"%12s - %6.3fms", name, latest_avg * 1000)) {
			float avg_hz = 1.0f / latest_avg;
			ImGui::Text("avg: %5.1f hz (%6.3f ms  min: %6.3f  max: %6.3f  stddev: %6.3f)",
				avg_hz, latest_avg * 1000, latest_min * 1000, latest_max * 1000, latest_std_dev * 1000);

			ImGui::SetNextItemWidth(-1);
			ImGui::PlotHistogram("##frametimes_histogram", avg.data(), (int)avg.count(), 0, nullptr, 0, y_axis_height / 1000, ImVec2(0, (float)imgui_histo_height));

			if (ImGui::BeginPopupContextItem("##frametimes_histogram popup")) {
				ImGui::SliderInt("imgui_histo_height", &imgui_histo_height, 20, 120);
				ImGui::DragFloat("y_axis_height [ms]", &y_axis_height, 0.1f);

				int cap = (int)avg.capacity();
				if (ImGui::SliderInt("avg_count", &cap, 16, 1024)) {
					avg.resize(cap);
				}

				ImGui::EndPopup();
			}

			ImGui::TreePop();
		}
	}
};


struct ValuePlotter {
	// TODO: use circular buffer with fixed number of items
	circular_buffer<float> values;

	int plot_height = 200;

	ValuePlotter (int count=1000) {
		values = circular_buffer<float>(count);
		resize(count);
	}
	void resize (int count) {
		values.resize(count);
		// init
		while (values.count() < values.capacity())
			values.push(0);
	}

	void push_value (float value) {
		values.push(value);
	}
	void imgui_display (const char* name, float min, float max, bool default_open=true) {
		if (ImGui::TreeNodeEx(name, default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			
			ImGui::SetNextItemWidth(-1);
			ImGui::PlotLines("##speed_plot", values.data(), (int)values.capacity(), 0, nullptr, min, max, ImVec2(0, (float)plot_height), (int)sizeof(float));

			if (ImGui::BeginPopupContextItem("##plot popup")) {
				ImGui::SliderInt("plot_height", &plot_height, 20, 120);

				int count = (int)values.count();
				if (ImGui::SliderInt("count", &count, 16, 1024)) {
					resize(count);
				}

				ImGui::EndPopup();
			}
			ImGui::TreePop();
		}
	}
};

struct DistributionPlotter {
	int num_buckets = 64;
	int plot_height = 200;
	
	template <typename FUNC>
	void plot_distribution (const char* name, int values, FUNC get_value, float xmin, float xmax, bool default_open=true) {
		if (ImGui::TreeNodeEx(name, default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
			
			std::vector<float> buckets(num_buckets, 0.0f);
			
			for (int i=0; i<values; ++i) {
				float val = get_value(i);
				int idx = floori(map(val, xmin, xmax) * (float)num_buckets);
				buckets[clamp(idx, 0, num_buckets-1)] += 1.0f;
			}
			
			float ymin = -0.5f;
			float ymax = +0.5f;
			
			for (auto& val : buckets) {
				ymax = max(ymax, val);
			}

			ImGui::SetNextItemWidth(-1);
			ImGui::PlotHistogram("##histo_plot", buckets.data(), num_buckets, 0, 0, ymin, ymax, float2(0, (float)plot_height));

			if (ImGui::BeginPopupContextItem("##plot popup")) {
				ImGui::SliderInt("plot_height", &plot_height, 20, 120);
				ImGui::SliderInt("num_buckets", &num_buckets, 10, 256);
				ImGui::EndPopup();
			}
			ImGui::TreePop();
		}
	}
};

inline void print_bitset_allocator (AllocatorBitset const& bits, size_t sz=0, size_t pagesz=1, size_t commit_size=-1) {
	constexpr char chars[2] = {'#', '-'};
	ImVec4 colors[2] = { ImVec4(1,1,1,1), ImVec4(0.5f, 0.5f, 0.55f, 1) };

	char str[64+1];

	size_t offs = 0;

	for (size_t b : bits.bits) {

		size_t cur_col = (offs / pagesz) % 2; // get color at start of line
		char* out = str;

		for (int i=0; i < 64 && offs < commit_size; ++i) {
			size_t col = (offs / pagesz) % 2; // alternate col index based on which page we are in

			if (col != cur_col) {
				// color changed, print previous line
				*out = '\0';
				ImGui::TextColored(colors[cur_col], str);
				ImGui::SameLine(0, 0);
				out = str; // restart string

				cur_col = col;
			}

			*out++ = chars[(b >> i) & 1];
			offs += sz;
		}

		*out = '\0';
		ImGui::TextColored(colors[cur_col], str);
	}
}

template <typename T>
inline void print_block_allocator (BlockAllocator<T> const& alloc, char const* name) {
	if (ImGui::TreeNode(name)) {
		print_bitset_allocator(alloc.slots, sizeof(T), os_page_size, alloc.commit_size());
		ImGui::TreePop();
	}
}
