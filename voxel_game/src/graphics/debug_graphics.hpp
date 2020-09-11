#pragma once
#include "stdafx.hpp"
#include "../kissmath.hpp"
#include "shaders.hpp"
#include "camera.hpp"
#include "../dear_imgui.hpp"

struct DebugGraphics {
	struct Vertex {
		float3	pos_world;
		lrgba	color;

		static void bind (Attributes& a) {
			a.add    <decltype(pos_world)>(0, "pos_world", sizeof(Vertex), offsetof(Vertex, pos_world));
			a.add    <decltype(color    )>(1, "color"    , sizeof(Vertex), offsetof(Vertex, color    ));
		}
	};

	Shader shader = { "debug_graphics" };

	std::vector<Vertex> faces;
	std::vector<Vertex> lines;

	Mesh<Vertex> faces_mesh;
	Mesh<Vertex> lines_mesh;

	float gl_line_width = 2;

	void imgui () {
		if (!imgui_push("DebugGraphics")) return;

		ImGui::SliderFloat("glLineWidth", &gl_line_width, .5f, 4);

		imgui_pop();
	}

	// draw quad

	void push_point (float3 pos, float3 size, lrgba col);

	void push_arrow (float3 pos, float3 dir, lrgba col);

	void push_wire_cube (float3 center, float3 size, lrgba col);

	void push_wire_frustrum (Camera_View const& view, lrgba col);

	void push_cylinder (float3 center, float radius, float height, lrgba col, int sides);

	void draw ();
};

// global DebugGraphics
extern std::unique_ptr<DebugGraphics> debug_graphics;
