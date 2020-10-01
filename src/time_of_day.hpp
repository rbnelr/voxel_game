#pragma once
#include "kissmath.hpp"
#include "dear_imgui.hpp"

struct SkyColors {
	float3 sky_col;
	float3 horiz_col;
	float3 ambient_col;

	void imgui (char const* name) {
		if (!imgui_push("SkyColors", name)) return;

		imgui_ColorEdit3("sky_col", &sky_col.x, ImGuiColorEditFlags_DisplayHSV);
		imgui_ColorEdit3("horiz_col", &horiz_col.x, ImGuiColorEditFlags_DisplayHSV);
		imgui_ColorEdit3("ambient_col", &ambient_col.x, ImGuiColorEditFlags_DisplayHSV);

		imgui_pop();
	}
};

class TimeOfDay {
public:
	float time = 12; // [0,24)

	SkyColors day_colors = {
		srgb(121,192,255),
		srgb(199,211,219),
		srgb(41,49,52)
	};
	SkyColors night_colors = {
		srgb(3,10,3),
		srgb(4,8,4),
		srgb(1,1,1)
	};

	void imgui () {
		if (!imgui_push("TimeOfDay")) return;

		ImGui::SliderFloat("time", &time, 0, 24);
		day_colors.imgui("day_colors");
		night_colors.imgui("night_colors");

		imgui_pop();
	}

	SkyColors calc_sky_colors (uint8* sky_light_reduce);
};
