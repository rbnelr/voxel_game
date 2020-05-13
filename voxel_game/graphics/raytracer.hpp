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

	RaytraceHit raycast (Ray ray);
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
	float visualize_dist = 32;

	int resolution = 100; // vertical

	void imgui (Chunks& chunks) {
		if (!imgui_push("Raytracer")) return;

		ImGui::Checkbox("draw", &raytracer_draw);
		ImGui::Checkbox("overlay", &overlay);
		ImGui::SliderFloat("slider", &slider, 0,1);
		ImGui::DragFloat("visualize_dist", &visualize_dist, .05f);

		ImGui::SliderInt("resolution", &resolution, 1, 1440);

		imgui_pop();
	}

	lrgba raytrace_pixel (int2 pixel, Camera_View const& view);

	void raytrace (Chunks& chunks, Camera_View const& view);
	void draw ();
};
