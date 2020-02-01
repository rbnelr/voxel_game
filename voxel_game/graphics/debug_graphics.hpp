#pragma once
#include "../kissmath.hpp"
#include "util.hpp"

struct DebugGraphics {
	struct Vertex {
		float3	pos_world;
		lrgba	color;

		static void bind (Attributes& a) {
			a.add<decltype(pos_world)>(0, "pos_world", sizeof(Vertex), offsetof(Vertex, pos_world));
			a.add<decltype(color    )>(1, "color"    , sizeof(Vertex), offsetof(Vertex, color    ));
		}
	};

	Shader shader = Shader("overlay");

	MeshStreamDrawer<Vertex> faces;
	MeshStreamDrawer<Vertex> lines;

	// draw quad
	
	void push_wire_cube (float3 center, float size, lrgba col);

	// draw arrow

	void push_cylinder (float3 center, float radius, float height, lrgba col, int sides);

	void draw (Camera_View& view);
};

// global DebugGraphics
extern std::unique_ptr<DebugGraphics> debug_graphics;
