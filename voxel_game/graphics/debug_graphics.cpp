#include "debug_graphics.hpp"

void DebugGraphics::push_wire_cube (float3 center, float size, lrgba col) {
	//vector_append(

	//vertices.push_back
}

// draw arrow

void DebugGraphics::push_cylinder (float3 center, float radius, float height, lrgba col, int sides) {
	faces.resize(faces.size() + 12 * sides);

	float2 rv = float2(radius, 0);
	float h = height;

	float2x2 prev_rot = float2x2::identity();

	for (int i=0; i<sides; ++i) {
		float rot_b = (float)(i + 1) / (float)sides * deg(360);

		float2x2 ma = prev_rot;
		float2x2 mb = rotate2(rot_b);

		prev_rot = mb;

		faces[i*12 +  0] = { center +float3(0,0,     +h/2), col };
		faces[i*12 +  1] = { center +float3(ma * rv, +h/2), col };
		faces[i*12 +  2] = { center +float3(mb * rv, +h/2), col };

		faces[i*12 +  3] = { center +float3(mb * rv, -h/2), col };
		faces[i*12 +  4] = { center +float3(mb * rv, +h/2), col };
		faces[i*12 +  5] = { center +float3(ma * rv, -h/2), col };
		faces[i*12 +  6] = { center +float3(ma * rv, -h/2), col };
		faces[i*12 +  7] = { center +float3(mb * rv, +h/2), col };
		faces[i*12 +  8] = { center +float3(ma * rv, +h/2), col };

		faces[i*12 +  9] = { center +float3(0,0,     -h/2), col };
		faces[i*12 + 10] = { center +float3(mb * rv, -h/2), col };
		faces[i*12 + 11] = { center +float3(ma * rv, -h/2), col };
	}
}

void DebugGraphics::draw () {
	if (shader) {
		shader.bind();

		//glDisable(GL_DEPTH_TEST);

		//// triangles
		//glDisable(GL_CULL_FACE);

		if (faces.size() != 0) {
			faces_mesh.upload(faces);

			faces_mesh.bind();
			faces_mesh.draw();
		}

		//glEnable(GL_CULL_FACE);

		//// lines

		if (lines.size() != 0) {
			lines_mesh.upload(lines);

			lines_mesh.bind();
			lines_mesh.draw();
		}

		//glEnable(GL_DEPTH_TEST);
	}

	faces.clear();
	lines.clear();
}

std::unique_ptr<DebugGraphics> debug_graphics = nullptr;
