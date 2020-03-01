#include "raytracer.hpp"
#include "graphics.hpp"
#include "../chunks.hpp"

RawArray<block_id> filter_octree_level (int size, int chunks_count, RawArray<block_id> const& prev_level) {
	RawArray<block_id> ret = RawArray<block_id>(chunks_count * size * size * size);

	int src_size = size * 2;

	auto get = [&] (bpos xyz, int chunk) {
		return prev_level[chunk * src_size*src_size*src_size + xyz.z * src_size*src_size + xyz.y * src_size + xyz.x];
	};

	int out = 0;
	for (int chunk=0; chunk<chunks_count; ++chunk) {
		for (int z=0; z<size; ++z) {
			for (int y=0; y<size; ++y) {
				for (int x=0; x<size; ++x) {
					block_id blocks[8];
					static int3 lut[] = { int3(0,0,0), int3(1,0,0), int3(0,1,0), int3(1,1,0), int3(0,0,1), int3(1,0,1), int3(0,1,1), int3(1,1,1) };
					for (int i=0; i<8; ++i)
						blocks[i] = get(bpos(x,y,z)*2 + lut[i], chunk);

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
	}

	return ret;
}

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
	RawArray<block_id> voxels = RawArray<block_id>((uint64_t)chunks_count * CHUNK_DIM_Z * CHUNK_DIM_Y * CHUNK_DIM_X);

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
					block_id id = chunk.get_block(bpos(x,y,z)).id;

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
	voxels_tex.upload_mip(0, voxels.ptr, int3(CHUNK_DIM_X, CHUNK_DIM_Y, CHUNK_DIM_Z * chunks_count), GL_R16, GL_RED, GL_UNSIGNED_SHORT);

	RawArray<block_id> prev = std::move(voxels);
	int size = CHUNK_DIM_X;
	int mip = 1;
	while (size >= 2) {
		size /= 2;
	
		auto cur = filter_octree_level(size, chunks_count, prev);
	
		voxels_tex.upload_mip(mip++, cur.ptr, int3(size, size, size * chunks_count), GL_R16, GL_RED, GL_UNSIGNED_SHORT);
	
		prev = std::move(cur);
	}
	
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, mip-1);
};

void Raytracer::imgui (Chunks& chunks) {
	if (!imgui_push("Raytracer")) return;

	ImGui::Checkbox("draw", &raytracer_draw);
	ImGui::Checkbox("overlay", &overlay);
	ImGui::SliderFloat("slider", &slider, 0,1);
	ImGui::SliderFloat("octree_slider", &octree_slider, 0,1);
	ImGui::SliderFloat("view_dist", &view_dist, 0, 1000, "%.2f", 2);
	ImGui::SliderFloat("iterations_visualize_max", &iterations_visualize_max, 0, 1500, "%.2f", 2);
	ImGui::Checkbox("iterations_visualize", &iterations_visualize);

	if (ImGui::Button("regen_data")) {
		regen_data(chunks);
	}

	imgui_texture_debug("chunks_lut", chunks_lut);
	imgui_texture_debug_4d("voxels_tex", voxels_tex, int4(CHUNK_DIM, uploaded_chunks.chunks_count));

	imgui_pop();
}

void Raytracer::regen_data (Chunks& chunks) {
	uploaded_chunks.upload(chunks, chunks_lut, voxels_tex);
}

void Raytracer::draw (Chunks& chunks, Graphics& graphics) {
	if (shader) {
		shader.bind();

		shader.set_uniform("slider", slider);
		shader.set_uniform("octree_slider", octree_slider);
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

		auto l = filter_octree_level(size, 1, o.octree_levels.back());

		o.octree_levels.push_back(std::move(l));
	}

	o.pos = (float3)chunk->chunk_pos_world();
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
		//debug_graphics->push_wire_cube((float3)index*(float)voxel_size + (float)voxel_size/2 + offset, (float)voxel_size * 0.99f, cols[level]);
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

struct ParametricOctreeTraverser {
	Octree& octree;
	
	// An Efficient Parametric Algorithm for Octree Traversal
	// J. Revelles, C.Ure ̃na, M.Lastra
	// http://citeseerx.ist.psu.edu/viewdoc/download;jsessionid=F6810427A1DC4136D615FBD178C1669C?doi=10.1.1.29.987&rep=rep1&type=pdf

	int first_node (float3 t0, float3 tm) {
		int max_comp;
		max_component(t0, &max_comp);

		static constexpr int lut_a[] = { 1, 0, 0 };
		static constexpr int lut_b[] = { 2, 2, 1 };

		int a = lut_a[max_comp];
		int b = lut_b[max_comp];

		float cond = t0[max_comp];

		int ret = 0;
		ret |= (tm[a] < cond) ? (1 << a) : 0;
		ret |= (tm[b] < cond) ? (1 << b) : 0;
		return ret;
	}
	int next_node (float3 t1, const int indices[3]) {
		int min_comp;
		min_component(t1, &min_comp);

		return indices[min_comp];
	}

	static constexpr int node_seq_lut[8][3] = {
		{ 1, 2, 4 }, // 001 010 100
		{ 8, 3, 5 }, //   - 011 101
		{ 3, 8, 6 }, // 011   - 110
		{ 8, 8, 7 }, //   -   - 111
		{ 5, 6, 8 }, // 101 110   -
		{ 8, 7, 8 }, //   - 111   -
		{ 7, 8, 8 }, // 111   -   -
		{ 8, 8, 8 }, //   -   -   -
	};

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

	static constexpr int MAX_DEPTH = 6;

	struct Stackframe {
		int3 pos; // 3d index (scaled to octree level cell size so that one increment is always as step to the next cell on that level)
		float3 min, max, mid;
		float3 t0, t1, tm;

		int cur_node;
		float3 lo, hi;
	};

	Stackframe get_child (Stackframe const& stk, int index, bool3 mask) {
		Stackframe ret;

		ret.pos = stk.pos * 2 + child_offset_lut[index];
		ret.min = select(mask, stk.mid, stk.min);
		ret.max = select(mask, stk.max, stk.mid);
		ret.mid = 0.5f * (ret.max + ret.min);

		return ret;
	}

	enum StatemachineOp {
		PRE_LOOP,
		LOOP_PRE_RECURSE,
		LOOP_CHILD_POST_RECURSE,
		RETURN
	};

	void traverse_octree (Stackframe _stk, Ray ray) {
		Stackframe stack[MAX_DEPTH+1];
		int depth = 0;
		auto* stk = &stack[depth];
		*stk = _stk;

		int mirror_mask_int = 0;
		bool3 mirror_mask = false;

		for (int i=0; i<3; ++i) {
			if (ray.dir[i] < 0) {
				ray.pos[i] = stk->mid[i] * 2 - ray.pos[i];
				ray.dir[i] = -ray.dir[i];
				mirror_mask_int |= 1 << i;
				mirror_mask[i] = true;
			}
		}

		float3 rdir_inv = 1.0f / ray.dir;

		stk->t0 = (stk->min - ray.pos) * rdir_inv;
		stk->t1 = (stk->max - ray.pos) * rdir_inv;

		if (max_component(stk->t0) >= min_component(stk->t1))
			return;

		bool stop = false;
		bool3 ray_mask = ray.dir != 0;

		StatemachineOp op = PRE_LOOP;

		for (;;) {
			switch (op) {

				case PRE_LOOP: {
					stk->tm = select(ray_mask, 0.5f * (stk->t0 + stk->t1), select(ray.pos < stk->mid, float3(+INF), float3(-INF)));

					if (all(stk->t1 >= 0) && eval_octree_cell(depth, *stk, &stop)) {
						assert(!stop);

						stk->cur_node = first_node(stk->t0, stk->tm);
					
						op = LOOP_PRE_RECURSE;
					} else {
						op = RETURN;
					}
				} break;

				case LOOP_PRE_RECURSE: {
					bool3 mask = bool3(stk->cur_node & 1, (stk->cur_node >> 1) & 1, (stk->cur_node >> 2) & 1);
					stk->lo = select(mask, stk->tm, stk->t0);
					stk->hi = select(mask, stk->t1, stk->tm);

					// Recursive call
					auto* new_stk = &stack[++depth];

					*new_stk = get_child(*stk, stk->cur_node ^ mirror_mask_int, mask != mirror_mask);
					new_stk->t0 = stk->lo,
					new_stk->t1 = stk->hi;

					stk = new_stk;

					op = PRE_LOOP; // recursive call
				} break;

				case LOOP_CHILD_POST_RECURSE: {
					if (!stop)
						stk->cur_node = next_node(stk->hi, node_seq_lut[stk->cur_node]);

					if (stop || stk->cur_node >= 8) {
						op = RETURN;
					} else {
						op = LOOP_PRE_RECURSE;
					}
				} break;

				case RETURN: {
					// Return from recursive call
					--depth;
					if (depth >= 0) {
						stk = &stack[depth];
						op = LOOP_CHILD_POST_RECURSE; // return from recusive call
					} else {
						return; // finish
					}
				} break;
			}
		}
	}

	bool eval_octree_cell (int depth, Stackframe const& stk, bool* stop_traversal) {
		float3 size = stk.max - stk.min;
		{
			int level = MAX_DEPTH - depth;
			int voxel_count = 1 << depth;
			int voxel_size = CHUNK_DIM_X >> depth;

			debug_graphics->push_wire_cube((float3)stk.pos*(float)voxel_size + (float)voxel_size/2, (float)voxel_size, cols[level]);

			auto get = [&] (int3 xyz) {
				return octree.octree_levels[level][xyz.z * voxel_count*voxel_count + xyz.y * voxel_count + xyz.x];
			};

			auto b = get(stk.pos);
			*stop_traversal = b != B_NULL && b != B_AIR;
			return b == B_NULL; // true == need to drill further down into octree
		}
		
	#if 0
		int entry_axis;
		float entry_t = min_component(t0, &entry_axis);

		float entry_dist = length(ray.dir * entry_t);
		float3 entry_pos = ray.dir * entry_t + ray.pos;

		//debug_graphics->push_wire_cube(node.min + size*0.5f, size, cols[node.level]);

		return false;
	#endif
	}
};

void Octree::raycast (Ray ray) {
	ParametricOctreeTraverser t = { *this };

	ParametricOctreeTraverser::Stackframe stk;
	stk.pos = 0;
	stk.min = pos;
	stk.max = pos + (float3)(float)(CHUNK_DIM_X);
	stk.mid = 0.5f * (stk.min + stk.max);

	t.traverse_octree(stk, ray);
}

void OctreeDevTest::draw (Chunks& chunks) {

	ImGui::DragFloat3("ray.pos", &ray.pos.x, 0.05f);

	ImGui::DragFloat2("ray_ang", &ray_ang.x, 1);
	ray.dir = normalize( rotate3_Z(to_radians(ray_ang.x)) * rotate3_X(to_radians(ray_ang.y)) * float3(0,1,0) );

	debug_graphics->push_arrow(ray.pos, ray.dir * 100, srgba(255,255,0));

	Chunk* chunk;
	chunks.query_block(floori(ray.pos), &chunk);
	if (!chunk) return;

	octree = build_octree(chunk);

	int cell_count = 0;
	octree.recurs_draw(0, (int)octree.octree_levels.size() - 1, (float3)chunk->chunk_pos_world(), cell_count);

	ImGui::Text("Octree stats: %d^3 (%7d voxels) can be stored as %7d octree nodes", CHUNK_DIM_X, CHUNK_DIM_X*CHUNK_DIM_X*CHUNK_DIM_X, cell_count);

	octree.raycast(ray);
}
