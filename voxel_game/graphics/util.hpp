#pragma once
#include "gl.hpp"
#include "glshader.hpp"
#include "camera.hpp"
#include <vector>

template <typename Vertex>
struct MeshStreamDrawer {
	std::vector<Vertex> data;
	Mesh<Vertex> mesh;

	void draw (Shader& shader, Camera_View& view) {
		if (shader && data.size() != 0) {
			shader.bind();

			shader.set_uniform("world_to_cam", (float4x4)view.world_to_cam);
			shader.set_uniform("cam_to_world", (float4x4)view.cam_to_world);
			shader.set_uniform("cam_to_clip", view.cam_to_clip);

			mesh.upload(data);

			mesh.bind();
			mesh.draw();
		}

		data.clear();
	}
};
