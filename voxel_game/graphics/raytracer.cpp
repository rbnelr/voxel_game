#include "raytracer.hpp"
#include "graphics.hpp"
#include "../util/timer.hpp"
#include "../chunks.hpp"

static constexpr int3 child_offset_lut[8] = {
	int3(0,0,0),
	int3(1,0,0),
	int3(0,1,0),
	int3(1,1,0),
	int3(0,0,1),
	int3(1,0,1),
	int3(0,1,1),
	int3(1,1,1),
};

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

void Octree::build_non_sparse_octree (Chunk* chunk) {
	RawArray<block_id> l0 = RawArray<block_id>(CHUNK_DIM * CHUNK_DIM * CHUNK_DIM);

	int out = 0;
	for (int z=0; z<CHUNK_DIM; ++z) {
		for (int y=0; y<CHUNK_DIM; ++y) {
			for (int x=0; x<CHUNK_DIM; ++x) {
				l0[out++] = chunk->get_block_unchecked(bpos(x,y,z))->id;
			}
		}
	}

	levels.push_back(std::move(l0));

	int size = CHUNK_DIM;
	while (size >= 2) {
		size /= 2;

		auto l = filter_octree_level(size, levels.back());

		levels.push_back(std::move(l));
	}

	pos = (float3)chunk->chunk_pos_world();
}

// build sparse octree depth-first
int recurse_build_sparse_octree(int idx, int level, int3 pos, Octree* octree) {
	int voxel_count = CHUNK_DIM >> level;

	octree->nodes[idx]._children = 0; // clears has_children
	octree->nodes[idx].bid = octree->levels[level][pos.z * voxel_count*voxel_count + pos.y * voxel_count + pos.x];

	if (level > 0 && octree->nodes[idx].bid == B_NULL) {
		uint32_t children_idx = (uint32_t)octree->nodes.size();
		octree->nodes[idx].set_children_indx(children_idx);
		
		octree->nodes.resize(children_idx + 8); // push 8 consecutive children nodes
		
		for (int i=0; i<8; ++i) {
			int3 child_pos = pos * 2 + child_offset_lut[i];

			int child_idx = recurse_build_sparse_octree(children_idx + i, level - 1, child_pos, octree);
		}
	}

	return idx;
}

Octree build_octree (Chunk* chunk) {
	Octree o;

	o.build_non_sparse_octree(chunk);

	o.nodes.emplace_back(); // push root
	o.root = 0;
	recurse_build_sparse_octree(0, (int)o.levels.size() - 1, 0, &o);

	o.node_count = (int)o.nodes.size();
	o.total_size = o.node_count * o.node_size;
	return o;
}

#define TIME_START(name) auto __##name = Timer::start()
#define TIME_END(name) auto __##name##_time = __##name.end()

void Raytracer::draw (Chunks& chunks, Camera_View const& view, TileTextures const& tile_textures, Sampler const& sampler) {
	if (!raytracer_draw) return;

	Chunk* chunk;
	chunks.query_block(floori(view.cam_to_world * float3(0)), &chunk);
	if (!chunk) return;

	if (BlockTileInfo_texture.size <= 0) {
		int* data = (int*)&tile_textures.block_tile_info[0];
		int size = sizeof(tile_textures.block_tile_info) / sizeof(tile_textures.block_tile_info[0]) * 3;
		
		BlockTileInfo_texture.upload(data, size, false, GL_RGB32I, GL_RGB_INTEGER, GL_INT);
	}

TIME_START(build);
	octree = build_octree(chunk);
TIME_END(build);

	build_octree(chunk);

	// raytracer outputs premultiplied alpha because it needs to alpha blend front to back internally
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	if (shader) {
		shader.bind();

		glBindVertexArray(vao);

		shader.set_uniform("svo_root_pos", octree.pos);

		shader.set_uniform("slider", slider);

		shader.set_uniform("max_iterations", max_iterations);
		shader.set_uniform("visualize_iterations", visualize_iterations);

		svo_texture.upload(&octree.nodes[0], (int)octree.nodes.size(), false, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT);

		glActiveTexture(GL_TEXTURE0 + 0);
		shader.set_texture_unit("tile_textures", 0);
		sampler.bind(0);
		tile_textures.tile_textures.bind();
		
		glActiveTexture(GL_TEXTURE0 + 1);
		shader.set_texture_unit("BlockTileInfo_texture", 1);
		lut_sampler.bind(1);
		BlockTileInfo_texture.bind();

		glActiveTexture(GL_TEXTURE0 + 2);
		shader.set_texture_unit("svo_texture", 2);
		lut_sampler.bind(2);
		svo_texture.bind();

		glActiveTexture(GL_TEXTURE0 + 3);
		shader.set_texture_unit("heat_gradient", 3);
		heat_gradient.bind();

		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	ImGui::Text("Raytrace performance:  build_octree %7.3f ms", __build_time * 1000);
}
