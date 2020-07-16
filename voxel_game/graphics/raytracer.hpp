#pragma once
#include "graphics_common.hpp"
#include "../blocks.hpp"
#include "../util/raw_array.hpp"

class Graphics;
namespace world_octree {
	class WorldOctree;
}

class Raytracer {
public:

	Shader shader = Shader("raytrace_svo", { FOG_UNIFORMS });

	gl::Vao vao; // empty vao even though I generate a screen-filling quad in the vertex shader, no vao works but generates an error on my machine

	Texture2D svo_texture; // 1d Does not fit because GL_MAX_TEXTURE_SIZE
	Sampler svo_sampler = Sampler(gl::Enum::NEAREST, gl::Enum::NEAREST, gl::Enum::CLAMP_TO_EDGE);
	Sampler gradient_sampler = Sampler(gl::Enum::LINEAR, gl::Enum::LINEAR_MIPMAP_LINEAR, gl::Enum::CLAMP_TO_EDGE);

	Texture1D block_tile_info_texture;
	
	Texture2D heat_gradient = { "textures/heat_gradient.png" };

	bool raytracer_draw = 1;
	bool overlay = 0;

	float slider = 1.00f;

	int max_iterations = 256;
	bool visualize_iterations = false;

	float water_F0 = 0.2f;
	float water_IOR = 1.333f;

	void imgui ();

	void draw (world_octree::WorldOctree& octree, Camera_View const& view, Graphics& graphics);
};
