#include "raytracer.hpp"
#include "../chunks.hpp"

void Raytracer::draw (Chunk* chunk) {
	RawArray<uint8> data = RawArray<uint8>(CHUNK_DIM_Z*CHUNK_DIM_Y*CHUNK_DIM_X);

	for (int z=0; z<CHUNK_DIM_Z; ++z) {
		for (int y=0; y<CHUNK_DIM_Y; ++y) {
			for (int x=0; x<CHUNK_DIM_X; ++x) {
				uint8 id = (uint8)chunk->get_block(bpos(x,y,z)).id;

				data.ptr[z * CHUNK_DIM_Y*CHUNK_DIM_X + y * CHUNK_DIM_X + x] = id;
			}
		}
	}

	chunk_tex.upload(data.ptr, CHUNK_DIM, 1, false, false);

	if (shader) {
		shader.bind();

		glActiveTexture(GL_TEXTURE0);
		shader.set_texture_unit("chunk_tex", 0);
		voxel_sampler.bind(0);
		chunk_tex.bind();

		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
}
