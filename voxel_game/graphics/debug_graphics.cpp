#include "debug_graphics.hpp"

void DebugGraphics::push_wire_cube (float3 center, float size, lrgba col) {
	//vector_append(

	//vertices.push_back
}

// draw arrow

void DebugGraphics::push_cylinder (float3 center, float radius, float height, lrgba col, int sides) {
	faces.mesh.resize(faces.mesh.size() + 12 * sides);

	float2 rv = float2(radius, 0);
	float h = height;

	float2x2 prev_rot = float2x2::identity();

	for (int i=0; i<sides; ++i) {
		float rot_b = (float)(i + 1) / (float)sides * deg(360);

		float2x2 ma = prev_rot;
		float2x2 mb = rotate2(rot_b);

		prev_rot = mb;

		faces.mesh[i*12 +  0] = { center +float3(0,0,     +h/2), col };
		faces.mesh[i*12 +  1] = { center +float3(ma * rv, +h/2), col };
		faces.mesh[i*12 +  2] = { center +float3(mb * rv, +h/2), col };

		faces.mesh[i*12 +  3] = { center +float3(mb * rv, -h/2), col };
		faces.mesh[i*12 +  4] = { center +float3(mb * rv, +h/2), col };
		faces.mesh[i*12 +  5] = { center +float3(ma * rv, -h/2), col };
		faces.mesh[i*12 +  6] = { center +float3(ma * rv, -h/2), col };
		faces.mesh[i*12 +  7] = { center +float3(mb * rv, +h/2), col };
		faces.mesh[i*12 +  8] = { center +float3(ma * rv, +h/2), col };

		faces.mesh[i*12 +  9] = { center +float3(0,0,     -h/2), col };
		faces.mesh[i*12 + 10] = { center +float3(mb * rv, -h/2), col };
		faces.mesh[i*12 + 11] = { center +float3(ma * rv, -h/2), col };
	}
}

void DebugGraphics::draw (Camera_View& view) {

		glEnable(GL_BLEND);

		//// triangles
		//glDisable(GL_CULL_FACE);
		//glDisable(GL_DEPTH_TEST);

		faces.draw(shader, view);

		//glEnable(GL_DEPTH_TEST);
		//glEnable(GL_CULL_FACE);

		//// lines
		//glDisable(GL_DEPTH_TEST);

		lines.draw(shader, view);

		//glEnable(GL_DEPTH_TEST);

		glDisable(GL_BLEND);
}

std::unique_ptr<DebugGraphics> debug_graphics = nullptr;
