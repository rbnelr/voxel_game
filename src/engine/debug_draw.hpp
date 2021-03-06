#pragma once
#include "common.hpp"
#include "renderer.hpp"

struct Camera_View;

struct DebugDraw {
	struct LineVertex {
		float3 pos;
		float4 col;

		template <typename ATTRIBS>
		static void attributes (ATTRIBS& a) {
			int loc = 0;
			a.init(sizeof(LineVertex));
			a.template add<AttribMode::FLOAT, decltype(pos)>(loc++, "pos", offsetof(LineVertex, pos));
			a.template add<AttribMode::FLOAT, decltype(col)>(loc++, "col", offsetof(LineVertex, col));
		}
	};
	struct TriVertex {
		float3 pos;
		float3 normal;
		float4 col;

		template <typename ATTRIBS>
		static void attributes (ATTRIBS& a) {
			int loc = 0;
			a.init(sizeof(TriVertex));
			a.template add<AttribMode::FLOAT, decltype(pos   )>(loc++, "pos"   , offsetof(TriVertex, pos));
			a.template add<AttribMode::FLOAT, decltype(normal)>(loc++, "normal", offsetof(TriVertex, normal));
			a.template add<AttribMode::FLOAT, decltype(col   )>(loc++, "col"   , offsetof(TriVertex, col));
		}
	};

	std::vector<LineVertex> lines;
	std::vector<TriVertex> tris;

	static constexpr float3 _wire_cube[12 * 2] {
		// bottom lines
		float3(-.5f,-.5f,-.5f), float3(+.5f,-.5f,-.5f),
		float3(+.5f,-.5f,-.5f), float3(+.5f,+.5f,-.5f),
		float3(+.5f,+.5f,-.5f), float3(-.5f,+.5f,-.5f),
		float3(-.5f,+.5f,-.5f), float3(-.5f,-.5f,-.5f),
		// vertical lines
		float3(-.5f,-.5f,-.5f), float3(-.5f,-.5f,+.5f),
		float3(+.5f,-.5f,-.5f), float3(+.5f,-.5f,+.5f),
		float3(+.5f,+.5f,-.5f), float3(+.5f,+.5f,+.5f),
		float3(-.5f,+.5f,-.5f), float3(-.5f,+.5f,+.5f),
		// top lines
		float3(-.5f,-.5f,+.5f), float3(+.5f,-.5f,+.5f),
		float3(+.5f,-.5f,+.5f), float3(+.5f,+.5f,+.5f),
		float3(+.5f,+.5f,+.5f), float3(-.5f,+.5f,+.5f),
		float3(-.5f,+.5f,+.5f), float3(-.5f,-.5f,+.5f),
	};
	static constexpr float3 _wire_vertices[8] {
		float3(-.5f,-.5f,-.5f),
		float3(+.5f,-.5f,-.5f),
		float3(+.5f,+.5f,-.5f),
		float3(-.5f,+.5f,-.5f),
		float3(-.5f,-.5f,+.5f),
		float3(+.5f,-.5f,+.5f),
		float3(+.5f,+.5f,+.5f),
		float3(-.5f,+.5f,+.5f),
	};
	static constexpr uint16_t _wire_indices[12 * 2] {
		// bottom lines
		0,1,  1,2,  2,3,  3,0,
		// vertical lines
		0,4,  1,5,  2,6,  3,7,
		// top lines
		4,5,  5,6,  6,7,  7,4,
	};
	
	struct PosVertex {
		float3 pos;

		template <typename ATTRIBS>
		static void attributes (ATTRIBS& a) {
			a.init(sizeof(PosVertex));
			a.template add<AttribMode::FLOAT, decltype(pos) >(0, "pos",  offsetof(PosVertex, pos));
		}
	};
	struct Instance {
		float3 pos;
		float3 size;
		float4 col;

		Instance () {}

		template <typename ATTRIBS>
		static void attributes (ATTRIBS& a) {
			a.init(sizeof(Instance), true);
			a.template add<AttribMode::FLOAT, decltype(pos)>(1, "instance_pos",  offsetof(Instance, pos));
			a.template add<AttribMode::FLOAT, decltype(size)>(2, "instance_size",  offsetof(Instance, size));
			a.template add<AttribMode::FLOAT, decltype(col)>(3, "instance_col",  offsetof(Instance, col));
		}
	};

	std::vector<Instance> wire_cubes;

	void clear () {
		if (lines.size() == 0)
			lines.shrink_to_fit();
		if (tris.size() == 0)
			tris.shrink_to_fit();
		if (wire_cubes.size() == 0)
			wire_cubes.shrink_to_fit();

		lines.clear();
		tris.clear();
		wire_cubes.clear();
	}

	void point (float3 const& pos, float3 const& size, lrgba const& col);
	void vector (float3 const& pos, float3 const& dir, lrgba const& col);

	void wire_cube (float3 const& pos, float3 const& size, lrgba const& col);
	void wire_sphere (float3 const& pos, float r, lrgba const& col, int angres=32, int wires=16);
	void wire_cone (float3 const& pos, float ang, float length, float3x3 const& rot, lrgba const& col, int circres=16, int wires=8);
	void wire_frustrum (Camera_View const& view, lrgba const& col);

	void cylinder (float3 const& base, float radius, float height, lrgba const& col, int sides=32);

	void axis_gizmo (Camera_View const& view, int2 const& viewport_size);
};

inline DebugDraw g_debugdraw;
