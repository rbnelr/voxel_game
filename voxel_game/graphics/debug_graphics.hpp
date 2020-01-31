#pragma once
#include "../kissmath.hpp"
#include "util.hpp"

struct DebugGraphics {
	struct Vertex {
		float3	pos_world;
		lrgba	color;

		static constexpr Vertex_Layout<2> layout = {
			Attribute{ "pos_world", gl::T_V3, 7*4, 0 },
			Attribute{ "color",		gl::T_V4, 7*4, 3*4 }
		};
	};
	static_assert(sizeof(Vertex) == 7*4);

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
