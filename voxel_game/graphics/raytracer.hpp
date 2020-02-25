#pragma once
#include "graphics_common.hpp"

class Chunks;
class Graphics;

class Raytracer {
public:

	Shader shader = Shader("raytrace", { FOG_UNIFORMS });

	Sampler voxel_sampler = Sampler(gl::Enum::NEAREST, gl::Enum::NEAREST, gl::Enum::CLAMP_TO_EDGE);

	Texture2D chunks_lut;
	Texture3D voxels_tex;

	void draw (Chunks& chunks, Graphics& graphics);
};
