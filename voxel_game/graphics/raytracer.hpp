#pragma once
#include "graphics_common.hpp"
#include "../blocks.hpp"
#include "../util/raw_array.hpp"

class Chunks;
class Graphics;

#define MAX_INT ((int)0x7fffffff)

struct UploadedChunks {
	int2 min_chunk = MAX_INT;
	int2 max_chunk = -MAX_INT;

	int2 chunks_lut_size;
	int chunks_lut_count;
	int chunks_count;

	void upload (Chunks& chunks, Texture2D& chunks_lut, Texture3D& voxels_tex);
};

class Raytracer {
public:

	Shader shader = Shader("raytrace", { FOG_UNIFORMS });

	gl::Vao vao; // empty vao even though I generate the mesh in the vertex shader, no vao works but generates an error on my machine

	Sampler voxel_sampler = Sampler(gl::Enum::NEAREST, gl::Enum::NEAREST, gl::Enum::CLAMP_TO_EDGE);

	Texture2D chunks_lut;
	Texture3D voxels_tex;

	UploadedChunks uploaded_chunks;

	bool raytracer_draw = true;
	bool overlay = false;
	float slider = 1.0f;
	float octree_slider = 0.5f;
	float view_dist = 200.0f;
	float iterations_visualize_max = 200.0f;
	bool iterations_visualize = false;

	void imgui (Chunks& chunks) {
		if (!imgui_push("Raytracer")) return;

		ImGui::Checkbox("draw", &raytracer_draw);
		ImGui::Checkbox("overlay", &overlay);
		ImGui::SliderFloat("slider", &slider, 0,1);
		ImGui::SliderFloat("octree_slider", &octree_slider, 0,1);
		ImGui::SliderFloat("view_dist", &view_dist, 0, 1000, "%.2f", 2);
		ImGui::SliderFloat("iterations_visualize_max", &iterations_visualize_max, 0, 1500, "%.2f", 2);
		ImGui::Checkbox("iterations_visualize", &iterations_visualize);

		if (ImGui::Button("regen_data")) {
			regen_data(chunks);
		}

		imgui_pop();
	}

	void regen_data (Chunks& chunks);
	void draw (Chunks& chunks, Graphics& graphics);
};

struct Octree {
	std::vector<RawArray<block_id>> octree_levels;
	float3 pos;

	void recurs_draw (int3 index, int level, float3 offset, int& cell_count);

	void raycast (Ray ray);
};

struct OctreeDevTest {

	float2 ray_ang = float2(-50, 30);
	Ray ray = { float3(5,5,40) };

	Octree octree;

	void draw (Chunks& chunks);
};
