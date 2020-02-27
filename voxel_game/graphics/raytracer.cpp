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
		shader.set_uniform("iterations_visualize", iterations_visualize);

		glBindVertexArray(vao);

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

RawArray<block_id> filter_octree_level (int size, RawArray<block_id> const& prev_level) {
	RawArray<block_id> ret = RawArray<block_id>(size * size * size);

	auto get = [&] (bpos xyz) {
		return prev_level[xyz.z * size*2*size*2 + xyz.y * size*2 + xyz.x];
	};

	int out = 0;
	for (int z=0; z<size; ++z) {
		for (int y=0; y<size; ++y) {
			for (int x=0; x<size; ++x) {
				block_id blocks[8];
				static int3 lut[] = { int3(0,0,0), int3(1,0,0), int3(0,1,0), int3(1,1,0), int3(0,0,1), int3(1,0,1), int3(0,1,1), int3(1,1,1) };
				for (int i=0; i<8; ++i)
					blocks[i] = get(bpos(x,y,z)*2 + lut[i]);

				bool all_equal = true;
				for (int i=1; i<8; ++i) {
					if (blocks[i] != blocks[0]) {
						all_equal = false;
						break;
					}
				}

				ret[out++] = all_equal ? blocks[0] : B_NULL; // B_NULL == not leaf node
			}
		}
	}

	return ret;
}

Octree build_octree (Chunk* chunk) {
	static_assert(CHUNK_DIM_X == CHUNK_DIM_Y && CHUNK_DIM_X == CHUNK_DIM_Z);

	Octree o;

	RawArray<block_id> l0 = RawArray<block_id>(CHUNK_DIM_X * CHUNK_DIM_Y * CHUNK_DIM_Z);

	int out = 0;
	for (int z=0; z<CHUNK_DIM_Z; ++z) {
		for (int y=0; y<CHUNK_DIM_Y; ++y) {
			for (int x=0; x<CHUNK_DIM_X; ++x) {
				l0[out++] = chunk->get_block_unchecked(bpos(x,y,z))->id;
			}
		}
	}

	o.octree_levels.push_back(std::move(l0));

	int size = CHUNK_DIM_X;
	while (size >= 2) {
		size /= 2;

		auto l = filter_octree_level(size, o.octree_levels.back());

		o.octree_levels.push_back(std::move(l));
	}

	return o;
}

lrgba cols[] = {
	srgba(255,0,0),
	srgba(0,255,0),
	srgba(0,0,255),
	srgba(255,255,0),
	srgba(255,0,255),
	srgba(0,255,255),
	srgba(127,0,255),
	srgba(255,0,127),
	srgba(255,127,255),
};

void Octree::recurs_draw (int3 index, int level, float3 offset, int& cell_count) { // level 0 == full res
	int voxel_count = CHUNK_DIM_X >> level;
	int voxel_size = 1 << level;
	
	auto get = [&] (int3 xyz) {
		return octree_levels[level][xyz.z * voxel_count*voxel_count + xyz.y * voxel_count + xyz.x];
	};

	auto b = get(index);
	if (b != B_NULL) {
		debug_graphics->push_wire_cube((float3)index*(float)voxel_size + (float)voxel_size/2 + offset, (float)voxel_size * 0.99f, cols[level]);
		cell_count++;
	} else {
		if (level > 0) {
			for (int z=0; z<2; ++z) {
				for (int y=0; y<2; ++y) {
					for (int x=0; x<2; ++x) {
						int3 rec_index = index * 2 + int3(x,y,z);
						recurs_draw(rec_index, level - 1, offset, cell_count);
					}
				}
			}
		}
	}
}

void OctreeDevTest::draw (Chunks& chunks) {

	ImGui::DragFloat3("ray.pos", &ray.pos.x, 0.05f);

	ImGui::DragFloat2("ray_ang", &ray_ang.x, 1);
	ray.dir = normalize( rotate3_Z(to_radians(ray_ang.x)) * rotate3_X(to_radians(ray_ang.y)) * float3(0,1,0) );

	debug_graphics->push_arrow(ray.pos, ray.dir, srgba(255,255,0));

	Chunk* chunk;
	chunks.query_block(floori(ray.pos), &chunk);
	if (!chunk) return;

	octree = build_octree(chunk);

	int cell_count = 0;
	octree.recurs_draw(0, (int)octree.octree_levels.size() - 1, (float3)chunk->chunk_pos_world(), cell_count);

	ImGui::Text("Octree stats: %d^3 (%7d voxels) can be stored as %7d octree nodes", CHUNK_DIM_X, CHUNK_DIM_X*CHUNK_DIM_X*CHUNK_DIM_X, cell_count);
}
