#pragma once
#include "stdafx.hpp"
#include "graphics_common.hpp"
#include "../blocks.hpp"
#include "../util/raw_array.hpp"

class Graphics;
class TimeOfDay;
namespace svo {
	struct SVO;
}

class Raytracer {
public:

	Shader shader = Shader("raytrace_svo", { FOG_UNIFORMS });

	gl::Vao vao; // empty vao even though I generate a screen-filling quad in the vertex shader, no vao works but generates an error on my machine

	Sampler svo_sampler = Sampler(gl::Enum::NEAREST, gl::Enum::NEAREST, gl::Enum::CLAMP_TO_EDGE);
	Sampler water_sampler = Sampler(gl::Enum::LINEAR, gl::Enum::LINEAR_MIPMAP_LINEAR, gl::Enum::REPEAT);
	
	Texture1D block_tile_info_texture;
	
	Sampler trilinear_sampler = Sampler(gl::Enum::LINEAR, gl::Enum::LINEAR_MIPMAP_LINEAR, gl::Enum::CLAMP_TO_EDGE);
	Sampler nearest_sampler = Sampler(gl::Enum::NEAREST, gl::Enum::LINEAR, gl::Enum::CLAMP_TO_EDGE);
	Texture2D heat_gradient = { "textures/heat_gradient.png" };
	Texture2D dbg_font = { "textures/consolas_ascii9x17.png" };

	Texture2D water_normal = { "textures/water_normal.jpg", false, true };

	bool raytracer_draw = 0;
	bool overlay = 0;

	float slider = 1.00f;

	int max_iterations = 300;
	bool visualize_iterations = false;

	float sun_radius = 0.1f;

	float water_F0 = 0.42f;
	float water_IOR = 1.333f;

	float2 water_scroll_dir1 = float2(-0.3f, 0.02f), water_scroll_dir2 = float2(-0.163f, -0.216f);
	float water_scale1 = 0.1f, water_scale2 = 0.641f;
	float water_strength1 = 0.05f, water_strength2 = 0.025f;
	float water_lod_bias = 0.0f;

	float time = 0;
	float time_speed = 1.0f;

	void imgui ();

	void draw (svo::SVO& octree, Camera_View const& view, Graphics& graphics, TimeOfDay& tod);
};