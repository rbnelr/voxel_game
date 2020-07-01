#pragma once
#include "graphics_common.hpp"
#include "../blocks.hpp"
#include "../util/raw_array.hpp"

class Chunk;
class Chunks;
class Graphics;

struct RaytraceHit {
	bool did_hit = false;

	float dist;
	float3 pos_world;

	block_id id;

	operator bool () { return did_hit; }
};

// non-sparse (2 B per node)
// ~3584 KB
// ~13 ms

// sparse (36 B per node)
// ~762 KB
// ~10.5 ms

// sparse, memory optimized (4 B per node)
// ~84.7 KB
// ~10 ms

struct Octree {
	std::vector<RawArray<block_id>> levels; // non-sparse version for comparison

	union Node {
		// MSB set if has children, mask MSB to zero to get actual children index
		// after masking MSB to 0 -> index of 8 consecutive chilren nodes in nodes array, only valid if bid == B_NULL
		uint32_t _children;
		
		struct { // payload if leaf node, ie. when !has_children
			block_id bid; // == B_NULL -> this has child nodes   != B_NULL -> this is a leaf node
			uint16_t _padding;
		};

		bool has_children () {
			return _children & 0x80000000u;
		}
		uint32_t children_indx () {
			return _children & 0x7fffffffu;
		}
		void set_children_indx (uint32_t childen_indx) {
			_children = childen_indx | 0x80000000u;
		}
	};

	std::vector<Node> nodes;
	int root; // root index

	int node_count;
	int node_size = sizeof(Node);
	int total_size;

	float3 pos;

	void build_non_sparse_octree (Chunk* chunk);

	void recurs_draw (Node& node, int3 index, int level);
	void debug_draw ();

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
	bool overlay = true;
	float slider = 0.7f;

	bool visualize_time = false;
	int visualize_max_time = 250;

	bool visualize_time_compare = true;
	bool visualize_time_compare_diff = true;
	float visualize_time_slider = 0.5f;

	int resolution = 100; // vertical

	//
	bool octree_debug_draw = false;

	bool draw_debug_ray = false;

	int2 debug_cursor_pos;

	OctreeDevTest dev;

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

		ImGui::Checkbox("octree_debug_draw", &octree_debug_draw);

		ImGui::Checkbox("draw_debug_ray", &draw_debug_ray);

		imgui_pop();
	}

	lrgba raytrace_pixel (int2 pixel, Camera_View const& view);

	void raytrace (Chunks& chunks, Camera_View const& view);
	void draw ();
};
