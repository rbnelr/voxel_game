#pragma once
#include "graphics_common.hpp"

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

	Sampler voxel_sampler = Sampler(gl::Enum::NEAREST, gl::Enum::NEAREST, gl::Enum::CLAMP_TO_EDGE);

	Texture2D chunks_lut;
	Texture3D voxels_tex;

	UploadedChunks uploaded_chunks;

	void regen_data (Chunks& chunks);
	void draw (Chunks& chunks, Graphics& graphics, float view_dist, float slider=1.0f);
};
