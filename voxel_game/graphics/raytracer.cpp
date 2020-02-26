#include "raytracer.hpp"
#include "graphics.hpp"
#include "../chunks.hpp"

void UploadedChunks::upload (Chunks& chunks, Texture2D& chunks_lut, Texture3D& voxels_tex) {
	chunks_count = chunks.chunks.count();

	min_chunk = MAX_INT;
	max_chunk = -MAX_INT;
	for (auto& chunk : chunks.chunks) {
		min_chunk = min(min_chunk, chunk.coord);
		max_chunk = max(max_chunk, chunk.coord);
	}

	chunks_lut_size = max_chunk + 1 - min_chunk;
	chunks_lut_count = chunks_lut_size.x * chunks_lut_size.y;

	RawArray<float> chunks_lut_data = RawArray<float>(chunks_lut_count);
	RawArray<uint8> voxels = RawArray<uint8>((uint64_t)chunks_count * CHUNK_DIM_Z * CHUNK_DIM_Y * CHUNK_DIM_X);

	for (int y=0; y<chunks_lut_size.y; ++y) {
		for (int x=0; x<chunks_lut_size.x; ++x) {
			chunks_lut_data[y * chunks_lut_size.x + x] = -1;
		}
	}

	int i = 0;
	for (auto& chunk : chunks.chunks) {
		int2 pos = chunk.coord - min_chunk;
		chunks_lut_data[pos.y * chunks_lut_size.x + pos.x] = (float)i;
		
		for (int z=0; z<CHUNK_DIM_Z; ++z) {
			for (int y=0; y<CHUNK_DIM_Y; ++y) {
				for (int x=0; x<CHUNK_DIM_X; ++x) {
					uint8 id = (uint8)chunk.get_block(bpos(x,y,z)).id;
		
					voxels[
						i * CHUNK_DIM_Z*CHUNK_DIM_Y*CHUNK_DIM_X +
						z * CHUNK_DIM_Y*CHUNK_DIM_X +
						y * CHUNK_DIM_X +
						x
					] = id;
				}
			}
		}
		
		i++;
	}

	//int max_dim;
	//glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_dim);
	// Currently 32k is max texture dimension -> 512 chunks causes glTexImage3D to fail
	// arrange chunk data in a 2d grid or just use a different data structure, or use compute shaders

	chunks_lut.upload(chunks_lut_data.ptr, chunks_lut_size, 1, false);
	voxels_tex.upload(voxels.ptr, int3(CHUNK_DIM_X, CHUNK_DIM_Y, CHUNK_DIM_Z * chunks_count), 1, false, false);
};

void Raytracer::regen_data (Chunks& chunks) {
	uploaded_chunks.upload(chunks, chunks_lut, voxels_tex);
}

void Raytracer::draw (Chunks& chunks, Graphics& graphics) {
	if (shader) {
		shader.bind();

		shader.set_uniform("slider", slider);
		shader.set_uniform("min_chunk", (float2)uploaded_chunks.min_chunk);
		shader.set_uniform("chunks_lut_size", (float2)uploaded_chunks.chunks_lut_size);
		shader.set_uniform("voxels_chunks_count", (float)uploaded_chunks.chunks_count);
		shader.set_uniform("view_dist", view_dist);
		shader.set_uniform("iterations_visualize_max", iterations_visualize_max);

		glActiveTexture(GL_TEXTURE0 + 0);
		shader.set_texture_unit("chunks_lut", 0);
		voxel_sampler.bind(0);
		chunks_lut.bind();

		glActiveTexture(GL_TEXTURE0 + 1);
		shader.set_texture_unit("voxels_tex", 1);
		voxel_sampler.bind(1);
		voxels_tex.bind();

		glActiveTexture(GL_TEXTURE0 + 2);
		shader.set_texture_unit("tile_textures", 2);
		graphics.tile_textures.tile_textures.bind();
		graphics.chunk_graphics.sampler.bind(2);

		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
}
