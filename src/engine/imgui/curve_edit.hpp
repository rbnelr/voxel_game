#pragma once
// TODO: move this to source file to hide include
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "dear_imgui.hpp"

#include "kisslib/serialization.hpp"
#include "kisslib/string.hpp"
using namespace kiss;


struct AnimCurve {
	SERIALIZE(AnimCurve, keys)

	struct Key {
		SERIALIZE(Key, x,y,slope)
		float x;
		float y;
		float slope;
	};
	std::vector<Key> keys = {
		// Default curve
		{ 0, 0, 0 },
		{ 1, 1, 0 },
	};

	float _interp (int idx, float absolute_x) {
		auto& key0 = keys[idx];
		auto& key1 = keys[idx+1];

		// formula for xy keyframe with slopes, but shifted to make key0.xy the origin for simpler equation
		// ax^3 + bx^2 + cx
		float m0 = key0.slope;
		float m1 = key1.slope;
		float x1 = key1.x - key0.x;
		float y1 = key1.y - key0.y;
		float x1_2 = x1*x1;
		float x1_3 = x1_2*x1;
		// calculate formula a,b,c derived via system of equations
		float num0 = y1 - m0*x1;
		float num1 = x1 * (m1 - m0);
		float a = ( num1 - 2.0f*(num0) ) / x1_3;
		float b = ( 3.0f*(num0) - num1 ) / x1_2;
		float c = m0;
		// actually compute formula with local coord space
		float x = absolute_x - key0.x;
		float x2 = x * x;
		float x3 = x2 * x;
		float f_local = a*x3 + b*x2 + c*x;
		// turn back into real coord space
		return f_local + key0.y;
	}
	float eval (float x) {
		//assert((int)keys.size() >= 1);
		if (keys.empty()) return 0;

		auto& first_key = keys[0];
		if (keys.size() <= 1 || x < first_key.x) return first_key.y;

		// TODO: could binary search but likely faster to do this
		for (int i=0; i<(int)keys.size()-1; i++) {
			if (x < keys[i+1].x) {
				return _interp(i, x);
			}
		}
		
		auto& last_key = keys.back();
		return last_key.y;
	}
	
	float2 _get_minmaxy (int idx) {
	#if 0
		auto& key0 = keys[idx];
		auto& key1 = keys[idx+1];
		
		float minY = min(key0.y, key1.y);
		float maxY = max(key0.y, key1.y);
		
		float m0 = key0.slope;
		float m1 = key1.slope;
		float x1 = key1.x - key0.x;
		float y1 = key1.y - key0.y;
		float x1_2 = x1*x1;
		float x1_3 = x1_2*x1;

		float num0 = y1 - m0*x1;
		float num1 = x1 * (m1 - m0);
		float a = ( num1 - 2.0f*(num0) ) / x1_3;
		float b = ( 3.0f*(num0) - num1 ) / x1_2;
		float c = m0;

		float p = 2.0f*b / (3.0f*a);
		float q = c / (3.0f*a);

		p *= 0.5f;
		float sq = p - q;
		if (sq >= 0.0f) {
			float sq = sqrt(p - q);

			float resX0 = sq - p;
			float resX1 = sq + p;
		
			float f0 = a*resX0*resX0*resX0 + b*resX0*resX0 + c*resX0 + key0.y;
			float f1 = a*resX1*resX1*resX1 + b*resX1*resX1 + c*resX1 + key0.y;
			
			minY = min(minY, min(f0, f1));
			maxY = max(maxY, max(f0, f1));
		}
		return float2(minY, maxY);
	#else
		auto& key0 = keys[idx];
		auto& key1 = keys[idx+1];

		float ymin = INF, ymax = -INF;

		for (int i=0; i<16; i++) {
			float x = lerp(key0.x, key1.x, (float)i / 15);
			auto y = _interp(idx, x);
			ymin = min(ymin, y);
			ymax = max(ymax, y);
		}
		return float2(ymin, ymax);
	#endif
	}
	
	ImRect get_range (float extend_relative=0) {
		//assert((int)keys.size() >= 1);
		ImRect res(ImVec2(0,0), ImVec2(1,1));

		if (keys.size() > 0) {
			res.Min.x = keys.front().x;
			res.Min.y = keys.front().y;
			res.Max.x = keys.back().x;
			res.Max.y = keys.back().y;
		}
		if (keys.size() > 1) {
			res.Min.y = INF; res.Max.y = -INF;
			for (int i=0; i<(int)keys.size()-1; i++) {
				auto t = _get_minmaxy(i);
				res.Min.y = min(res.Min.y, t.x);
				res.Max.y = max(res.Max.y, t.y);
			}
		}
		
		float sizeX = res.Max.x - res.Min.x;
		float sizeY = res.Max.y - res.Min.y;
		res.Min.x -= sizeX == 0.0f ? (0.5f + extend_relative) : sizeX * extend_relative;
		res.Max.x += sizeX == 0.0f ? (0.5f + extend_relative) : sizeX * extend_relative;
		res.Min.y -= sizeY == 0.0f ? (0.5f + extend_relative) : sizeY * extend_relative;
		res.Max.y += sizeY == 0.0f ? (0.5f + extend_relative) : sizeY * extend_relative;

		return res;
	}
	
	bool imgui (const char* label) {
		bool changed = false;

		ImGui::SetNextItemWidth(-1);
		auto rang = get_range(0.1f);
		int resX = (int)(ImGui::GetContentRegionAvail().x / 2.0f);

		auto getter = [&] (int idx) {
			float t = ((float)idx + 0.5f) / (float)resX;
			float x = lerp(rang.Min.x, rang.Max.x, t);
			return eval(x);
		};
		auto getter2 = [] (void* data, int idx) {
			// https://bannalia.blogspot.com/2016/07/passing-capturing-c-lambda-functions-as.html
			return (*static_cast<decltype(getter)*>(data))(idx);
		};

		ImGui::PlotLines("###_debug_vel", getter2, &getter, resX, 0, "Curve", rang.Min.y, rang.Max.y, ImVec2(0, 25));
		if (_BeginPopupContextWindow(NULL, ImVec2(600, 400),
				ImGuiPopupFlags_MouseButtonLeft | ImGuiPopupFlags_MouseButtonRight)) {
			changed = _popup() || changed;
			ImGui::EndPopup();
		}

		return changed;
	}
	bool _popup () {
		
		bool changed = imgui_edit_vector("Keys (x, y, slope)", keys, [&] (int i, Key& key) {
			return ImGui::DragFloat3(prints("[%d]", i).c_str(), &key.x, 0.1f);
		}, true, false, false);

		changed = _curve() || changed;

		return changed;
	}
	bool _curve () {
		using namespace ImGui;

		ImGuiContext& g = *GImGui;
		ImGuiStyle& Style = GetStyle();
		ImGuiIO& IO = GetIO();
		ImDrawList* DrawList = GetWindowDrawList();
		ImGuiWindow* Window = GetCurrentWindow();
		if (Window->SkipItems)
			return false;

		ImVec2 dim = GetContentRegionAvail();

		ImRect bb(Window->DC.CursorPos, Window->DC.CursorPos + dim);
		ItemSize(bb); // Does this cause things like IsItemActive to work?
		if (!ItemAdd(bb, NULL))
			return false;

		auto col_white = ImColor(ImVec4(GetStyle().Colors[ImGuiCol_Text]));
		//float luma = IsItemActive() || IsItemHovered() ? 0.5f : 1.0f;
		//ImVec4 pink = ImVec4(1.00f, 0.00f, 0.75f, luma), cyan(0.00f, 0.75f, 1.00f, luma);
		
		auto _curve_color = GetStyle().Colors[ ImGuiCol_PlotLinesHovered ];
		auto curve_color = ImColor(_curve_color);
		auto curve_colorG = ImColor(ImVec4(_curve_color.x, _curve_color.y, _curve_color.z, 0.4f));

		static ImRect view_range = get_range(0.1f);
		//ImRect curv_range = get_range(0.1f);

		ImRect bbFlippedY = ImRect(ImVec2(bb.Min.x, bb.Max.y), ImVec2(bb.Max.x, bb.Min.y));
		ImVec2 scaleFac = (bb.Max - bb.Min) / (view_range.Max - view_range.Min);

		auto transf = [&] (ImVec2 pos_curv) {
			return map(pos_curv, view_range.Min, view_range.Max, bbFlippedY.Min, bbFlippedY.Max);
		};
		auto transfInv = [&] (ImVec2 pos_imgui) {
			return map(pos_imgui, bbFlippedY.Min, bbFlippedY.Max, view_range.Min, view_range.Max);
		};

		RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_FrameBg, 1), true, 1.0f);

		DrawList->PushClipRect(bb.Min, bb.Max);
		
		//// background grid
		// find appropriate power of 10 grid spacing depending on final pixel size
		auto gridSpacing = [&] (float min_spacing) {
			float logi = ceil(logf(min_spacing) / logf(10.0f));
			return powf(10.0f, logi);
		};
		ImVec2 grid_spacing = ImVec2(
			gridSpacing(32.0f / scaleFac.x),
			gridSpacing(18.0f / scaleFac.y));

		int2 grid0 = floori(transfInv(bbFlippedY.Min) / grid_spacing);
		int2 grid1 = ceili(transfInv(bbFlippedY.Max) / grid_spacing);
		
		auto grid_col = GetColorU32(ImGuiCol_WindowBg);
		auto gridLineThickness = [&] (int idx) {
			if (idx == 0) return 3.0f; // Axis lines
			if ((idx % 5) == 0) return 2.0f; // 0, 5, 10th of 10 lines
			return 1.0f;
		};

		for (int i=grid0.x; i<=grid1.x; i++) {
			float x = float(i)*grid_spacing.x;
			ImVec2 a = transf(ImVec2(x, view_range.Min.y));
			ImVec2 b = transf(ImVec2(x, view_range.Max.y));
			float thickness = gridLineThickness(i);
			DrawList->AddLine(a, b, grid_col, thickness);
			
			char buf[64];
			ImFormatString(buf, sizeof(buf), " %g", x);
			DrawList->AddText(a + ImVec2(0,-g.FontSize-1), col_white, buf);
		}
		for (int i=grid0.y; i<=grid1.y; i++) {
			float y = float(i)*grid_spacing.y;
			ImVec2 a = transf(ImVec2(view_range.Min.x, y));
			ImVec2 b = transf(ImVec2(view_range.Max.x, y));
			float thickness = gridLineThickness(i);
			DrawList->AddLine(a, b, grid_col, thickness);

			char buf[64];
			ImFormatString(buf, sizeof(buf), " %g", y);
			DrawList->AddText(a + ImVec2(0,-g.FontSize-1), col_white, buf);
		}
		
		float curve_width = 2.0f;

		for (int i=0; i<(int)keys.size()-1; i++) {
			float x0 = transf(ImVec2(keys[i].x, 0)).x;
			float x1 = transf(ImVec2(keys[i+1].x, 0)).x;
			float pixels = x1 - x0;
			int res = max(int(pixels / 2), 1);

			ImVec2 prev_point = transf(ImVec2(keys[i].x, keys[i].y));
			for (int j=0; j<res; j++) {
				float x = lerp(keys[i].x, keys[i+1].x, float(j+1) / float(res));
				float y = _interp(i, x);
				auto point = transf(ImVec2(x,y));

				DrawList->AddLine(prev_point, point, curve_color, curve_width);

				prev_point = point;
			}
		}
		if ((int)keys.size() >= 1) {
			auto p0 = transf(ImVec2(keys[0].x, keys[0].y));
			auto p1 = transf(ImVec2(keys.back().x, keys.back().y));
			DrawList->AddLine(p0, ImVec2(bb.Min.x, p0.y), curve_colorG, curve_width);
			DrawList->AddLine(p1, ImVec2(bb.Max.x, p1.y), curve_colorG, curve_width);
		}

		auto draw_square = [&] (ImVec2 pos, float size, ImColor col) {
			DrawList->AddLine(pos + ImVec2(-size, -size), pos + ImVec2( size, -size), col);
			DrawList->AddLine(pos + ImVec2( size, -size), pos + ImVec2( size,  size), col);
			DrawList->AddLine(pos + ImVec2( size,  size), pos + ImVec2(-size,  size), col);
			DrawList->AddLine(pos + ImVec2(-size,  size), pos + ImVec2(-size, -size), col);
		};
		auto draw_diamond = [&] (ImVec2 pos, float size, ImColor col) {
			DrawList->AddLine(pos + ImVec2(-size, 0), pos + ImVec2(0,  size), col);
			DrawList->AddLine(pos + ImVec2( size, 0), pos + ImVec2(0,  size), col);
			DrawList->AddLine(pos + ImVec2( size, 0), pos + ImVec2(0, -size), col);
			DrawList->AddLine(pos + ImVec2(-size, 0), pos + ImVec2(0, -size), col);
		};

		float radius = 6;
		float radius2 = 4;
		float slope_line_len = 32;
		
		int hovered = -1;
		float min_hovered_dist = INF;

		for (int i=0; i<(int)keys.size(); i++) {
			ImVec2 point = transf(ImVec2(keys[i].x, keys[i].y));

			float dist = length(IO.MousePos - point);
			if (dist < 10 && dist < min_hovered_dist) {
				hovered = i;
				min_hovered_dist = dist;
			}
		}
		
		static int selection = -1; // TODO: use actual state
		if (IsMouseReleased(0)) selection = -1;
		if (IsMouseClicked(0)) selection = hovered;

		for (int i=0; i<(int)keys.size(); i++) {
			ImVec2 point = transf(ImVec2(keys[i].x, keys[i].y));
			ImVec2 dir = normalize(ImVec2(1, keys[i].slope) * scaleFac);

			if (selection == i && IsMouseDragging(0)) {
				point = point + IO.MouseDelta;
				auto point_curv = transfInv(point);
				keys[i].x = point_curv.x;
				keys[i].y = point_curv.y;
			}
			
			ImVec2 slopeP0 = point - slope_line_len * dir;
			ImVec2 slopeP1 = point + slope_line_len * dir;

			if (selection == i) {
				DrawList->AddLine(point, slopeP0, col_white);
				DrawList->AddLine(point, slopeP1, col_white);

				draw_square(slopeP0, radius2, col_white);
				draw_square(slopeP1, radius2, col_white);
			}

			draw_diamond(point,
				selection == i || hovered == i ? radius * 1.4f : radius,
				selection == i ? curve_color : col_white);
		}
		
		DrawList->PopClipRect();

		return false;
	}
	
	// Allow for resizable popup winodw
	bool _BeginPopupContextWindow(const char* str_id, ImVec2 initial_size, ImGuiPopupFlags popup_flags) {
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = g.CurrentWindow;
		if (!str_id)
			str_id = "window_context";
		ImGuiID id = window->GetID(str_id);
		int mouse_button = (popup_flags & ImGuiPopupFlags_MouseButtonMask_);
		if (ImGui::IsMouseReleased(mouse_button) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
			if (!(popup_flags & ImGuiPopupFlags_NoOpenOverItems) || !ImGui::IsAnyItemHovered()) {
				ImGui::OpenPopupEx(id, popup_flags);
				ImGui::SetNextWindowSize(initial_size);
			}
		}

		return _BeginPopupEx(id, "Curve Editor", ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse);
	}
	// Allow for window_title
	bool _BeginPopupEx(ImGuiID id, const char* window_title, ImGuiWindowFlags flags) {
		ImGuiContext& g = *GImGui;
		if (!ImGui::IsPopupOpen(id, ImGuiPopupFlags_None))
		{
			g.NextWindowData.ClearFlags(); // We behave like Begin() and need to consume those values
			return false;
		}

		char name[20];
		if (flags & ImGuiWindowFlags_ChildMenu)
			ImFormatString(name, IM_ARRAYSIZE(name), "%s##Menu_%02d", window_title, g.BeginMenuDepth); // Recycle windows based on depth
		else
			ImFormatString(name, IM_ARRAYSIZE(name), "%s##Popup_%08x", window_title, id); // Not recycling, so we can close/open during the same frame

		flags |= ImGuiWindowFlags_Popup | ImGuiWindowFlags_NoDocking;
		bool is_open = ImGui::Begin(name, NULL, flags);
		if (!is_open) // NB: Begin can return false when the popup is completely clipped (e.g. zero size display)
			ImGui::EndPopup();

		//g.CurrentWindow->FocusRouteParentWindow = g.CurrentWindow->ParentWindowInBeginStack;

		return is_open;
	}
};
