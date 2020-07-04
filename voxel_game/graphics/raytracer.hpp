#pragma once
#include "graphics_common.hpp"
#include "../blocks.hpp"
#include "../util/raw_array.hpp"

class Chunk;
class Chunks;
class Graphics;

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

	void recurs_draw (Node& node, int3 index, int level, int max_level);
	void debug_draw (int max_depth=9999);
};

class Raytracer {
public:

	Octree octree;

	Shader shader = Shader("raytrace", { FOG_UNIFORMS });

	gl::Vao vao; // empty vao even though I generate a screen-filling quad in the vertex shader, no vao works but generates an error on my machine

	Texture1D svo_texture;
	Sampler svo_sampler = Sampler(gl::Enum::NEAREST, gl::Enum::NEAREST, gl::Enum::CLAMP_TO_EDGE);
	Sampler gradient_sampler = Sampler(gl::Enum::LINEAR, gl::Enum::LINEAR_MIPMAP_LINEAR, gl::Enum::CLAMP_TO_EDGE);

	Texture2D heat_gradient = { "textures/heat_gradient.png" };

	bool raytracer_draw = true;
	float slider = 0.85f;

	int max_iterations = 256;
	bool visualize_iterations = false;

	//
	bool octree_debug_draw = false;
	int octree_debug_draw_depth = 99;

	void imgui (Chunks& chunks);

	//lrgba raytrace_pixel (int2 pixel, Camera_View const& view);

	void draw (Chunks& chunks, Camera_View const& view);
};
