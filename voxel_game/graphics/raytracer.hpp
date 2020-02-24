#pragma once
#include "graphics_common.hpp"

class Chunk;
class Graphics;

class Raytracer {
public:

	Shader shader = Shader("raytrace", { FOG_UNIFORMS });

	Sampler voxel_sampler = Sampler(gl::Enum::NEAREST, gl::Enum::NEAREST, gl::Enum::CLAMP_TO_EDGE);

	Texture3D chunk_tex;

	void draw (Chunk* chunk, Graphics& graphics);
};
