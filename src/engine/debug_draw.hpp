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

	void clear () {
		lines.clear();
		lines.shrink_to_fit();

		tris.clear();
		tris.shrink_to_fit();
	}

	void vector (float3 pos, float3 dir, lrgba col);

	void wire_cube (float3 pos, float3 size, lrgba col);
	void wire_frustrum (Camera_View const& view, lrgba col);

	void cylinder (float3 base, float radius, float height, lrgba col, int sides=32);
};

inline DebugDraw g_debugdraw;