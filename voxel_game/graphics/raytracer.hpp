#pragma once
#include "graphics_common.hpp"
#include "../blocks.hpp"
#include "../util/raw_array.hpp"

class Chunks;
class Graphics;

struct RaytraceHit {
	bool did_hit = false;

	float dist;
	float3 pos_world;

	block_id id;

	operator bool () { return did_hit; }
};

struct Octree {
	std::vector<RawArray<block_id>> octree_levels;
	float3 pos;

	void recurs_draw (int3 index, int level, float3 offset, int& cell_count);

	RaytraceHit raycast (Ray ray, int* iterations=nullptr);
};

struct OctreeDevTest {

	float2 ray_ang = float2(-50, 30);
	Ray ray = { float3(5,5,40) };

	Octree octree;

	void draw (Chunks& chunks);
};

class Raytracer {
public:

	Octree octree;

	Shader shader = Shader("raytrace_display", { FOG_UNIFORMS });

	gl::Vao vao; // empty vao even though I generate the mesh in the vertex shader, no vao works but generates an error on my machine

	Sampler voxel_sampler = Sampler(gl::Enum::NEAREST, gl::Enum::NEAREST, gl::Enum::CLAMP_TO_EDGE);

	Image<lrgba> renderimage;
	Texture2D rendertexture;

	bool raytracer_draw = true;
	bool overlay = false;
	float slider = 0.7f;

	bool visualize_time = true;
	int visualize_max_time = 250;

	bool visualize_time_compare = true;
	bool visualize_time_compare_diff = true;
	float visualize_time_slider = 0.5f;

	int resolution = 100; // vertical

	void imgui (Chunks& chunks) {
		if (!imgui_push("Raytracer")) return;

		ImGui::Checkbox("draw", &raytracer_draw);
		ImGui::Checkbox("overlay", &overlay);
		ImGui::SliderFloat("slider", &slider, 0,1);

		ImGui::Checkbox("visualize_time", &visualize_time);
		ImGui::DragInt("visualize_max_time", &visualize_max_time, 1);

		ImGui::Checkbox("visualize_time_compare", &visualize_time_compare);
		ImGui::Checkbox("visualize_time_compare_diff", &visualize_time_compare_diff);
		ImGui::SliderFloat("visualize_time_slider", &visualize_time_slider, 0,1);

		ImGui::SliderInt("resolution", &resolution, 1, 1440);

		imgui_pop();
	}

	lrgba raytrace_pixel (int2 pixel, Camera_View const& view);

	void raytrace (Chunks& chunks, Camera_View const& view);
	void draw ();
};
