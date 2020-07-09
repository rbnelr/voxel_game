#pragma once
#include "../kissmath.hpp"
#include "shaders.hpp"
#include "camera.hpp"

enum GraphicsMode {
	GM_FILL=0,
	GM_STRIPED,
};

struct DebugGraphics {
	struct Vertex {
		float3	pos_world;
		int		mode;
		lrgba	color;

		static void bind (Attributes& a) {
			a.add    <decltype(pos_world)>(0, "pos_world", sizeof(Vertex), offsetof(Vertex, pos_world));
			a.add_int<decltype(mode     )>(1, "mode"     , sizeof(Vertex), offsetof(Vertex, mode     ));
			a.add    <decltype(color    )>(2, "color"    , sizeof(Vertex), offsetof(Vertex, color    ));
		}
	};

	Shader shader = { "debug_graphics" };

	std::vector<Vertex> faces;
	std::vector<Vertex> lines;

	Mesh<Vertex> faces_mesh;
	Mesh<Vertex> lines_mesh;

	// draw quad

	void push_point (float3 pos, float3 size, lrgba col);

	void push_arrow (float3 pos, float3 dir, lrgba col, GraphicsMode mode=GM_FILL);

	void push_wire_cube (float3 center, float3 size, lrgba col, GraphicsMode mode=GM_FILL);

	void push_wire_frustrum (Camera_View const& view, lrgba col, GraphicsMode mode=GM_FILL);

	void push_cylinder (float3 center, float radius, float height, lrgba col, int sides, GraphicsMode mode=GM_FILL);

	void draw ();
};

// global DebugGraphics
extern std::unique_ptr<DebugGraphics> debug_graphics;
