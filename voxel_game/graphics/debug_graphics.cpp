#include "debug_graphics.hpp"
#include "../util/geometry.hpp"

void DebugGraphics::push_wire_cube (float3 center, float size, lrgba col) {
	//vector_append(

	//vertices.push_back
}

// draw arrow

void DebugGraphics::push_cylinder (float3 center, float radius, float height, lrgba col, int sides) {
	::push_cylinder<Vertex>(sides, [&] (float3 pos) {
		faces.push_back({ center + pos * float3(radius, radius, height), col });
	});
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
