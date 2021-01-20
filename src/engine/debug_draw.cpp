#include "common.hpp"
#include "debug_draw.hpp"
#include "camera.hpp"

void DebugDraw::vector (float3 pos, float3 dir, lrgba col) {
	size_t idx = lines.size();
	lines.resize(idx + 2);
	auto* out = &lines[idx];

	*out++ = { pos, col };
	*out++ = { pos + dir, col };
}

void DebugDraw::wire_cube (float3 pos, float3 size, lrgba col) {
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

	size_t idx = lines.size();
	lines.resize(idx + ARRLEN(_wire_cube));
	auto* out = &lines[idx];

	for (auto& p : _wire_cube) {
		out->pos = p * size + pos;
		out->col = col;
		out++;
	}
}
void DebugDraw::wire_frustrum (Camera_View const& view, lrgba col) {
	static constexpr int _frustrum_corners[12 * 2] {
		0,1,  1,2,  2,3,  3,0, // bottom lines
		0,4,  1,5,  2,6,  3,7, // vertical lines
		4,5,  5,6,  6,7,  7,4, // top lines
	};

	size_t idx = lines.size();
	lines.resize(idx + ARRLEN(_frustrum_corners));
	auto* out = &lines[idx];

	for (int corner : _frustrum_corners) {
		out->pos = view.frustrum.corners[corner];
		out->col = col;
		out++;
	}
}

void DebugDraw::cylinder (float3 base, float radius, float height, lrgba col, int sides) {
	size_t idx = tris.size();
	tris.resize(idx + sides * 4 * 3); // tri for bottom + top cap + 2 tris for side
	auto* out = &tris[idx];

	float ang_step = 2*PI / (float)sides;

	float sin0=0, cos0=1; // optimize not calling sin 2x per loop

	auto push_tri = [&] (float3 pos, float3 normal) {
		out->pos = pos * float3(radius, radius, height) + base;
		out->normal = normal;
		out->col = col;
		out++;
	};

	for (int i=0; i<sides; ++i) {
		float ang1 = (float)(i+1) * ang_step;

		float sin1 = sin(ang1);
		float cos1 = cos(ang1);

		push_tri(float3(   0,    0, 0), float3(0, 0, -1));
		push_tri(float3(cos1, sin1, 0), float3(0, 0, -1));
		push_tri(float3(cos0, sin0, 0), float3(0, 0, -1));

		push_tri(float3(cos1, sin1, 0), float3(cos1, sin1, 0));
		push_tri(float3(cos1, sin1, 1), float3(cos1, sin1, 0));
		push_tri(float3(cos0, sin0, 0), float3(cos0, sin0, 0));
		push_tri(float3(cos0, sin0, 0), float3(cos0, sin0, 0));
		push_tri(float3(cos1, sin1, 1), float3(cos1, sin1, 0));
		push_tri(float3(cos0, sin0, 1), float3(cos0, sin0, 0));

		push_tri(float3(   0,    0, 1), float3(0, 0, +1));
		push_tri(float3(cos0, sin0, 1), float3(0, 0, +1));
		push_tri(float3(cos1, sin1, 1), float3(0, 0, +1));

		sin0 = sin1;
		cos0 = cos1;
	}
}
