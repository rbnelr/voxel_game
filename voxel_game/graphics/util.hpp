#pragma once
#include "globjects.hpp"
#include "glshader.hpp"
#include "camera.hpp"
#include "vertex_layout.hpp"
#include <vector>

template <typename Vertex>
struct MeshStreamDrawer {
	gl::Vbo vbo;

	std::vector<Vertex> mesh;

	void draw (Shader& shad, Camera_View& view) {
		if (shad && mesh.size()) {
			glUseProgram(shad.shader->shad);

			shad.set_uniform("world_to_cam", (float4x4)view.world_to_cam);
			shad.set_uniform("cam_to_world", (float4x4)view.cam_to_world);
			shad.set_uniform("cam_to_clip", view.cam_to_clip);

			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, mesh.size() * sizeof(mesh[0]), NULL, GL_STREAM_DRAW);
			glBufferData(GL_ARRAY_BUFFER, mesh.size() * sizeof(mesh[0]), mesh.data(), GL_STREAM_DRAW);

			bind_attrib_arrays(Vertex::layout(), shad);

			glDrawArrays(GL_TRIANGLES, 0, (GLsizei)mesh.size());
		}

		mesh.clear();
	}
};
