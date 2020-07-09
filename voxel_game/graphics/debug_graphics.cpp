#include "debug_graphics.hpp"
#include "../util/geometry.hpp"

void DebugGraphics::push_point (float3 pos, float3 size, lrgba col) {
	lines.push_back({ pos - float3(size.x/2, 0, 0), 0, col });
	lines.push_back({ pos + float3(size.x/2, 0, 0), 0, col });
	lines.push_back({ pos - float3(0, size.y/2, 0), 0, col });
	lines.push_back({ pos + float3(0, size.y/2, 0), 0, col });
	lines.push_back({ pos - float3(0, 0, size.z/2), 0, col });
	lines.push_back({ pos + float3(0, 0, size.z/2), 0, col });
}

void DebugGraphics::push_arrow (float3 pos, float3 dir, lrgba col, GraphicsMode mode) {
	lines.push_back({ pos, mode, col });
	lines.push_back({ pos + dir, mode, col });
}

void DebugGraphics::push_wire_cube (float3 center, float3 size, lrgba col, GraphicsMode mode) {
	float3 L = center - size / 2;
	float3 H = center + size / 2;
	
	float3 LLL = float3(L.x,L.y,L.z);
	float3 HLL = float3(H.x,L.y,L.z);
	float3 LHL = float3(L.x,H.y,L.z);
	float3 HHL = float3(H.x,H.y,L.z);
	float3 LLH = float3(L.x,L.y,H.z);
	float3 HLH = float3(H.x,L.y,H.z);
	float3 LHH = float3(L.x,H.y,H.z);
	float3 HHH = float3(H.x,H.y,H.z);

	lines.push_back({ LLL, mode, col });
	lines.push_back({ HLL, mode, col });
	lines.push_back({ HLL, mode, col });
	lines.push_back({ HHL, mode, col });
	lines.push_back({ HHL, mode, col });
	lines.push_back({ LHL, mode, col });
	lines.push_back({ LHL, mode, col });
	lines.push_back({ LLL, mode, col });

	lines.push_back({ LLL, mode, col });
	lines.push_back({ LLH, mode, col });
	lines.push_back({ HLL, mode, col });
	lines.push_back({ HLH, mode, col });
	lines.push_back({ HHL, mode, col });
	lines.push_back({ HHH, mode, col });
	lines.push_back({ LHL, mode, col });
	lines.push_back({ LHH, mode, col });

	lines.push_back({ LLH, mode, col });
	lines.push_back({ HLH, mode, col });
	lines.push_back({ HLH, mode, col });
	lines.push_back({ HHH, mode, col });
	lines.push_back({ HHH, mode, col });
	lines.push_back({ LHH, mode, col });
	lines.push_back({ LHH, mode, col });
	lines.push_back({ LLH, mode, col });
}

void DebugGraphics::push_cylinder (float3 center, float radius, float height, lrgba col, int sides, GraphicsMode mode) {
	::push_cylinder<Vertex>(sides, [&] (float3 pos) {
		faces.push_back({ center + pos * float3(radius, radius, height), mode, col });
	});
}

void DebugGraphics::push_wire_frustrum (Camera_View const& view, lrgba col, GraphicsMode mode) {

	lines.push_back({ view.frustrum.corners[0], mode, col });
	lines.push_back({ view.frustrum.corners[1], mode, col });
	lines.push_back({ view.frustrum.corners[1], mode, col });
	lines.push_back({ view.frustrum.corners[2], mode, col });
	lines.push_back({ view.frustrum.corners[2], mode, col });
	lines.push_back({ view.frustrum.corners[3], mode, col });
	lines.push_back({ view.frustrum.corners[3], mode, col });
	lines.push_back({ view.frustrum.corners[0], mode, col });

	lines.push_back({ view.frustrum.corners[0], mode, col });
	lines.push_back({ view.frustrum.corners[4], mode, col });
	lines.push_back({ view.frustrum.corners[1], mode, col });
	lines.push_back({ view.frustrum.corners[5], mode, col });
	lines.push_back({ view.frustrum.corners[2], mode, col });
	lines.push_back({ view.frustrum.corners[6], mode, col });
	lines.push_back({ view.frustrum.corners[3], mode, col });
	lines.push_back({ view.frustrum.corners[7], mode, col });

	lines.push_back({ view.frustrum.corners[4], mode, col });
	lines.push_back({ view.frustrum.corners[5], mode, col });
	lines.push_back({ view.frustrum.corners[5], mode, col });
	lines.push_back({ view.frustrum.corners[6], mode, col });
	lines.push_back({ view.frustrum.corners[6], mode, col });
	lines.push_back({ view.frustrum.corners[7], mode, col });
	lines.push_back({ view.frustrum.corners[7], mode, col });
	lines.push_back({ view.frustrum.corners[4], mode, col });
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

		glLineWidth(2.0f);

		if (lines.size() != 0) {
			lines_mesh.upload(lines);

			lines_mesh.bind();
			lines_mesh.draw_lines();
		}

		//glEnable(GL_DEPTH_TEST);
	}

	faces.clear();
	lines.clear();
}

std::unique_ptr<DebugGraphics> debug_graphics = nullptr;
