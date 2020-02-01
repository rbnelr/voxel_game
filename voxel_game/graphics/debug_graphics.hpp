#pragma once
#include "../kissmath.hpp"
#include "util.hpp"

struct DebugGraphics {
	struct Vertex {
		float3	pos_world;
		lrgba	color;

		VERTEX_LAYOUT(Vertex,
			VERTEX_ATTRIBUTE(Vertex, pos_world),
			VERTEX_ATTRIBUTE(Vertex, color    )
		)
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
