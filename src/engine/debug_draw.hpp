#pragma once
#include "renderer.hpp"
#include "camera.hpp"

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

	static constexpr float3 _wire_cube_vertices[8] {
		float3(-.5f,-.5f,-.5f),
		float3(+.5f,-.5f,-.5f),
		float3(+.5f,+.5f,-.5f),
		float3(-.5f,+.5f,-.5f),
		float3(-.5f,-.5f,+.5f),
		float3(+.5f,-.5f,+.5f),
		float3(+.5f,+.5f,+.5f),
		float3(-.5f,+.5f,+.5f),
	};
	static constexpr uint16_t _wire_cube_indices[12 * 2] {
		// bottom lines
		0,1,  1,2,  2,3,  3,0,
		// vertical lines
		0,4,  1,5,  2,6,  3,7,
		// top lines
		4,5,  5,6,  6,7,  7,4,
	};
	
	static void gen_simple_wire_sphere (std::vector<float3>* vertices, std::vector<uint16_t>* indices, float r, int segments) {
		int count = 3 * segments;

		auto* vert = push_back(*vertices, 3 * segments);
		auto* ind = push_back(*indices, 3 * segments*2);
		
		float ang_step = deg(360) / (float)segments;

		for (int i=0; i<segments; ++i) {
			float s = sin((float)i * ang_step);
			float c = cos((float)i * ang_step);

			vert[i           ] = float3(c,s,0) * r;
			vert[i+segments  ] = float3(c,0,s) * r;
			vert[i+segments*2] = float3(0,c,s) * r;
		}

		uint16_t base = 0;
		for (int i=0; i<segments; ++i) {
			*ind++ = base + (uint16_t)( i );
			*ind++ = base + (uint16_t)((i+1) == segments ? 0 : i+1);
		}
		base += segments;
		for (int i=0; i<segments; ++i) {
			*ind++ = base + (uint16_t)( i );
			*ind++ = base + (uint16_t)((i+1) == segments ? 0 : i+1);
		}
		base += segments;
		for (int i=0; i<segments; ++i) {
			*ind++ = base + (uint16_t)( i );
			*ind++ = base + (uint16_t)((i+1) == segments ? 0 : i+1);
		}
	}
	
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
	void line (float3 const& a, float3 const& b, lrgba const& col);
	void vector (float3 const& pos, float3 const& dir, lrgba const& col);

	void wire_cube (float3 const& pos, float3 const& size, lrgba const& col);
	void wire_sphere (float3 const& pos, float r, lrgba const& col, int angres=32, int wires=16);
	void wire_cone (float3 const& pos, float ang, float length, float3x3 const& rot, lrgba const& col, int circres=16, int wires=8);
	void wire_frustrum (Camera_View const& view, lrgba const& col);
	
	void quad (float3 const& center, float2 size, lrgba const& col);
	void cylinder (float3 const& base, float radius, float height, lrgba const& col, int sides=32);
	
	void axis_gizmo (Camera_View const& view, int2 const& viewport_size);
	
	struct StayCube {
		float3 pos;
		float3 size;
		lrgba col;
		float timer;
	};
	std::vector<StayCube> stay_cubes;
	void wire_cube_stay (float3 const& pos, float3 const& size, lrgba const& col, float stay_time) {
		stay_cubes.push_back({ pos, size, col, stay_time });
	}

	////
	// Imgui-style interface to create selectable 'objects' in the 3d world
	// object is really just a point that you can move (rotate?)
	bool enable_selectables = true;
	char const* selected_id = nullptr;

	Camera_View view;
	float2 window_size;
	float2 cursor_pos = 0;
	bool lmb_went_down = false;
	bool lmb_is_down = false;

	bool grabbed = false;
	int gizmo_axis = -1;
	float3 grabbed_pos;
	float3 grabbed_offset;

	void prepare_selectables (Camera_View const& view, Input& I, bool enable);
	void finish_selectables ();

	// call with some string ptr and a position in the world to display a sphere at that point in the 3d world
	// that is selectable, keeps track of selected state internally returns if this object (id) is selected
	bool selectable (char const* id, float3 const& pos, float size, lrgba const& col);

	bool gizmo (float3* pos);

	bool movable (char const* id, float3* pos, float size, lrgba const& col, float3* vel=nullptr) {
		bool selected = selectable(id, *pos, size, col);
		if (selected) {
			if (gizmo(pos))
				if (vel) *vel = 0;
		}
		return selected;
	}

	void update (Input& I) {
		if (stay_cubes.size() > 0) {
			for (auto it = stay_cubes.begin(); it != stay_cubes.end();) {
				wire_cube(it->pos, it->size, it->col);
				it->timer -= I.real_dt;
				if (it->timer <= 0) {
					it = stay_cubes.erase(it);
				}
				else {
					it++;
				}
			}
		}
	}
};

inline DebugDraw g_debugdraw;
