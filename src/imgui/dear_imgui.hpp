#pragma once
#include "imgui.h"

#include "misc/cpp/imgui_stdlib.h"

#include "kisslib/kissmath.hpp"
#include "kisslib/running_average.hpp"
#include "kisslib/allocator.hpp"

#include <string>
#include <vector>

#include "imgui.h"

struct DearImgui {
	bool enabled = true;

	bool show_demo_window = false;

	int tree_depth = 0;

	DearImgui () {
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();

		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	}
	~DearImgui () {
		ImGui::DestroyContext();
	}
};

inline DearImgui g_imgui;

// root level is closed by default, sublevels are open by default
//  for printing inherited baseclasses in their derived classes
inline bool imgui_push (const char* class_name, const char* instance_name=nullptr, bool inner_default_open=true) {
	bool ret;
	if (instance_name)
		ret = ImGui::TreeNodeEx(instance_name, g_imgui.tree_depth != 0 && inner_default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0, "%s <%s>", instance_name, class_name);
	else
		ret = ImGui::TreeNodeEx(class_name, g_imgui.tree_depth != 0 && inner_default_open ? ImGuiTreeNodeFlags_DefaultOpen : 0, "<%s>", class_name);

	if (ret) g_imgui.tree_depth++;
	return ret;
}
inline void imgui_pop () {
	ImGui::TreePop();
	g_imgui.tree_depth--;
}

inline bool imgui_treenode (const char* label, bool* popen=nullptr) {
	if (popen) ImGui::SetNextItemOpen(*popen);
	bool open = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen);
	if (popen) *popen = open;
	return open;
}

inline bool imgui_header (const char* label, bool* popen=nullptr) {
	if (popen) ImGui::SetNextItemOpen(*popen);
	bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);
	if (popen) *popen = open;
	return open;
}

namespace ImGui { // copy paste from imgui_stdlib.h to make it work for tracy-tracked std::string
	struct _InputTextCallback_UserData {
		std::string*             Str;
		ImGuiInputTextCallback  ChainCallback;
		void*                   ChainCallbackUserData;
	};

	inline int _InputTextCallback(ImGuiInputTextCallbackData* data) {
		_InputTextCallback_UserData* user_data = (_InputTextCallback_UserData*)data->UserData;
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			// Resize string callback
			// If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
			std::string* str = user_data->Str;
			IM_ASSERT(data->Buf == str->c_str());
			str->resize(data->BufTextLen);
			data->Buf = (char*)str->c_str();
		}
		else if (user_data->ChainCallback)
		{
			// Forward to user callback, if any
			data->UserData = user_data->ChainCallbackUserData;
			return user_data->ChainCallback(data);
		}
		return 0;
	}

	inline bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) {
		IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
		flags |= ImGuiInputTextFlags_CallbackResize;

		_InputTextCallback_UserData cb_user_data;
		cb_user_data.Str = str;
		cb_user_data.ChainCallback = callback;
		cb_user_data.ChainCallbackUserData = user_data;
		return InputText(label, (char*)str->c_str(), str->capacity() + 1, flags, _InputTextCallback, &cb_user_data);
	}
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

// Global GuiConsole
inline Logger g_logger;

// log() function name is already taken by <cmath>

// log to console
void clog (char const* format, ...);

// log to console
void clog (LogLevel level, char const* format, ...);
