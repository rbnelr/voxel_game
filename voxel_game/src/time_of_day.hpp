#pragma once
#include "stdafx.hpp"

struct SkyColors {
	lrgb sky_col;
	lrgb horiz_col;
	lrgb ambient_col;
	lrgb sun_col;

	void imgui (char const* name) {
		if (!imgui_push("SkyColors", name)) return;

		imgui_ColorEdit("sky_col", &sky_col, ImGuiColorEditFlags_DisplayHSV);
		imgui_ColorEdit("horiz_col", &horiz_col, ImGuiColorEditFlags_DisplayHSV);
		imgui_ColorEdit("ambient_col", &ambient_col, ImGuiColorEditFlags_DisplayHSV);
		imgui_ColorEdit("sun_col", &sun_col, ImGuiColorEditFlags_DisplayHSV);

		imgui_pop();
	}
};

struct TimeOfDay {
	float time = 9.3f; // [0,24)

	SkyColors day_colors = {
		srgb(121,192,255),
		srgb(199,211,219),
		srgb(41,49,52),
		srgb(255, 240, 110),
	};
	SkyColors night_colors = {
		srgb(3,10,3),
		srgb(4,8,4),
		srgb(1,1,1),
		srgb(255, 240, 110),
	};

	float sun_azim = deg(20);

	SkyColors cols;
	float3 sun_dir = deg(20);

	void imgui () {
		if (!imgui_push("TimeOfDay")) return;

		ImGui::SliderFloat("time", &time, 0, 24);
		ImGui::SliderAngle("sun_azim", &sun_azim, -180, 180);
		day_colors.imgui("day_colors");
		night_colors.imgui("night_colors");

		imgui_pop();
	}

	void calc_sky_colors (uint8* sky_light_reduce);
};
