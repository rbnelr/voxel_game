﻿#include "raytracer.hpp"
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
	RawArray<block_id> l0 = RawArray<block_id>(CHUNK_DIM_X * CHUNK_DIM_Y * CHUNK_DIM_Z);

	int out = 0;
	for (int z=0; z<CHUNK_DIM_Z; ++z) {
		for (int y=0; y<CHUNK_DIM_Y; ++y) {
			for (int x=0; x<CHUNK_DIM_X; ++x) {
				l0[out++] = chunk->get_block_unchecked(bpos(x,y,z))->id;
			}
		}
	}

	levels.push_back(std::move(l0));

	int size = CHUNK_DIM_X;
	while (size >= 2) {
		size /= 2;

		auto l = filter_octree_level(size, levels.back());

		levels.push_back(std::move(l));
	}

	pos = (::float3)chunk->chunk_pos_world();
}

#if SPARSE_OCTREE
// build sparse octree depth-first
int recurse_build_sparse_octree(int idx, int level, int3 pos, Octree* octree) {
	int voxel_count = CHUNK_DIM_X >> level;

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
#endif

Octree build_octree (Chunk* chunk) {
	static_assert(CHUNK_DIM_X == CHUNK_DIM_Y && CHUNK_DIM_X == CHUNK_DIM_Z);

	Octree o;

	o.build_non_sparse_octree(chunk);

#if SPARSE_OCTREE
	o.nodes.emplace_back(); // push root
	o.root = 0;
	recurse_build_sparse_octree(0, (int)o.levels.size() - 1, 0, &o);

	o.node_count = (int)o.nodes.size();
	o.total_size = o.node_count * o.node_size;
#else
	o.node_count = (int)(o.levels.size() * CHUNK_DIM_X*CHUNK_DIM_Y*CHUNK_DIM_Z);
	o.total_size = o.node_count * o.node_size;
#endif
	return o;
}

#if 0
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
#endif

namespace otr {

	struct ParametricOctreeTraverser {
		// An Efficient Parametric Algorithm for Octree Traversal
		// J. Revelles, C.Ure ̃na, M.Lastra
		// http://citeseerx.ist.psu.edu/viewdoc/download;jsessionid=F6810427A1DC4136D615FBD178C1669C?doi=10.1.1.29.987&rep=rep1&type=pdf

		Octree& octree;
		RaytraceHit hit;

		int mirror_mask_int; // algo assumes ray.dir x,y,z >= 0, x,y,z < 0 cause the ray to be mirrored, to make all components positive, this mask is used to also mirror the octree nodes to make iteration work
		bool3 mirror_mask; // algo assumes ray.dir x,y,z >= 0, x,y,z < 0 cause the ray to be mirrored, to make all components positive, this mask is used to also mirror the octree nodes to make iteration work

		Ray ray;

		struct OctreeNode {
		#if SPARSE_OCTREE
			int oct_idx; // in sparse octree nodes
		#else
			int3 pos; // 3d index (scaled to octree level cell size so that one increment is always as step to the next cell on that level)
		#endif
			int level;
			float3 min;
			float3 mid;
			float3 max;

			OctreeNode get_child (int index, bool3 mask, Octree& octree) {
				OctreeNode ret;

			#if SPARSE_OCTREE
				ret.oct_idx = octree.nodes[oct_idx].children_indx() + index;
			#else
				ret.pos = pos * 2 + child_offset_lut[index];
			#endif
				ret.level = level - 1;
				ret.min = select(mask, mid, min);
				ret.max = select(mask, max, mid);
				ret.mid = 0.5f * (ret.max + ret.min);

				return ret;
			}
		};

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

		void traverse_octree (OctreeNode root, Ray ray) {
			this->ray = ray;
			mirror_mask_int = 0;
			mirror_mask = false;

			for (int i=0; i<3; ++i) {
				if (ray.dir[i] < 0) {
					ray.pos[i] = root.mid[i] * 2 - ray.pos[i];
					ray.dir[i] = -ray.dir[i];
					mirror_mask_int |= 1 << i;
					//mirror_mask[i] = true;
					mirror_mask.v.m128_u32[i] = 0xffffffffu;
				}
			}

			float3 rdir_inv = 1.0f / ray.dir;

			float3 t0 = (root.min - ray.pos) * rdir_inv;
			float3 t1 = (root.max - ray.pos) * rdir_inv;

			if (max_component(t0) < min_component(t1))
				traverse_subtree(root, t0, t1);
		}

		int first_node (float3 t0, float3 tm) {
			float cond = max_component(t0);

			int ret = 0;
			ret |= (tm[0] < cond) ? 1 : 0;
			ret |= (tm[1] < cond) ? 2 : 0;
			ret |= (tm[2] < cond) ? 4 : 0;
			return ret;
		}
		int next_node (float3 t1, const int indices[3]) {
			int min_comp = min_component_indx(t1);

			return indices[min_comp];
		}

		bool traverse_subtree (OctreeNode node, float3 t0, float3 t1) {

			if (min_component(t1) < 0)
				return false;

			bool stop;
			bool decend = eval_octree_cell(node, t0, t1, &stop);
			if (decend) {
				assert(!stop);

				float3 tm = select(ray.dir != 0.0f, 0.5f * (t0 + t1), select(ray.pos < node.mid, float3(+INF), float3(-INF)));

				int cur_node = first_node(t0, tm);

				do {
					bool3 mask = bool3(cur_node & 1, (cur_node >> 1) & 1, (cur_node >> 2) & 1);

					stop = traverse_subtree(node.get_child(cur_node ^ mirror_mask_int, mask ^ mirror_mask, octree), select(mask, tm, t0), select(mask, t1, tm));
					if (stop)
						return true;

					cur_node = next_node(select(mask, t1, tm), node_seq_lut[cur_node]);
				} while (cur_node < 8);
			}

			return stop;
		}

		bool eval_octree_cell (OctreeNode node, float3 t0, float3 t1, bool* stop_traversal) {
			//float3 size = node.max - node.min;
			{
				//int voxel_count = CHUNK_DIM_X >> node.level;
				//int voxel_size = 1 << node.level;

				*stop_traversal = false;
				if (octree.nodes[node.oct_idx].has_children())
					return true; // need to decend further down into octree to find actual voxels

			#if SPARSE_OCTREE
				auto b = octree.nodes[node.oct_idx].bid;
			#else
				auto b = octree.levels[node.level][node.pos.z * voxel_count*voxel_count + node.pos.y * voxel_count + node.pos.x];
			#endif

				bool stop = b != B_AIR;
				if (stop) {
					hit.did_hit = true;
					hit.id = b;
					hit.dist = max_component(t0);
					hit.pos_world = (::float3)(ray.pos + ray.dir * hit.dist);
				}

				*stop_traversal = stop;
				return false;
			}
		}
	};

	RaytraceHit Octree::raycast (Ray ray) {

		ParametricOctreeTraverser::OctreeNode o;
	#if SPARSE_OCTREE
		o.oct_idx = root;
	#else
		o.pos = 0;
	#endif
		o.level = (int)levels.size() - 1;
		o.min = pos;
		o.max = pos + (float3)(float)(1 << o.level);
		o.mid = 0.5f * (o.min + o.max);

		ParametricOctreeTraverser t = { *this };
		t.traverse_octree(o, ray);

		return t.hit;
	}
}

void OctreeDevTest::draw (Chunks& chunks) {
	//
	//ImGui::DragFloat3("ray.pos", &ray.pos.x, 0.05f);
	//
	//ImGui::DragFloat2("ray_ang", &ray_ang.x, 1);
	//ray.dir = normalize( rotate3_Z(to_radians(ray_ang.x)) * rotate3_X(to_radians(ray_ang.y)) * float3(0,1,0) );
	//
	//debug_graphics->push_arrow(ray.pos, ray.dir * 100, srgba(255,255,0));
	//
	//Chunk* chunk;
	//chunks.query_block(floori(ray.pos), &chunk);
	//if (!chunk) return;
	//
	//octree = build_octree(chunk);
	//
	//int cell_count = 0;
	//octree.recurs_draw(0, (int)octree.octree_levels.size() - 1, (float3)chunk->chunk_pos_world(), cell_count);
	//
	//ImGui::Text("Octree stats: %d^3 (%7d voxels) can be stored as %7d octree nodes", CHUNK_DIM_X, CHUNK_DIM_X*CHUNK_DIM_X*CHUNK_DIM_X, cell_count);
	//
	//octree.raycast(ray);
}

#define TIME_START(name) auto __##name = Timer::start()
#define TIME_END(name) auto __##name##_time = __##name.end()

Ray ray_for_pixel (int2 pixel, int2 resolution, Camera_View const& view) {
	float2 ndc = ((float2)pixel + 0.5f) / (float2)resolution * 2 - 1;

	//float4 near_plane_clip = view.cam_to_clip * float4(0, 0, -view.clip_near, 1);
	float4 near_plane_clip = float4(0, 0, -view.clip_near, view.clip_near);

	float4 clip = float4(ndc, -1, 1) * near_plane_clip.w; // ndc = clip / clip.w;

	float3 pos_cam = (float3)(view.clip_to_cam * clip);
	float3 dir_cam = pos_cam;

	Ray ray;
	ray.pos = (float3)( view.cam_to_world * float4(pos_cam, 1) );
	ray.dir = (float3)( view.cam_to_world * float4(dir_cam, 0) );
	ray.dir = normalize(ray.dir);

	return ray;
}

lrgba Raytracer::raytrace_pixel (int2 pixel, Camera_View const& view) {
	auto ray = ray_for_pixel(pixel, renderimage.size, view);

	auto time0 = get_timestamp();
	auto hit = octree.raycast({ ray.pos, ray.dir });
	auto time = (int)(get_timestamp() - time0);

	if (visualize_time) {
		if (visualize_time_compare) {
			auto time1 = get_timestamp();
			raycast_voxels(ray, 9999, [&] (int3 voxel, int face, float dist) {
				if (any(voxel < 0 || voxel >= CHUNK_DIM)) return true;
				return octree.levels[0][voxel.z * CHUNK_DIM_Y*CHUNK_DIM_X + voxel.y * CHUNK_DIM_X + voxel.x] != B_AIR;
				});
			auto time_b = (int)(get_timestamp() - time1);

			if (visualize_time_compare_diff) {
				time = time - time_b;
			} else {
				if (pixel.x > (int)(renderimage.size.x * visualize_time_slider))
					time = time_b;
			}
		}

		float diff_mag = (float)abs(time) / (float)visualize_max_time;
		//float diff_mag = (float)iterations / (float)20;

		if (time > 0)
			return lrgba(diff_mag, 0, 0, 1);
		else
			return lrgba(0, diff_mag, 0, 1);
	}

	if (!hit)
		return lrgba(0,0,0,0);

	return lrgba((float3)hit.dist / 100, 1);
}

////
void Raytracer::raytrace (Chunks& chunks, Camera_View const& view) {

	Chunk* chunk;
	chunks.query_block(floori(view.cam_to_world * float3(0)), &chunk);
	if (!chunk) return;

	TIME_START(build);
	octree = build_octree(chunk);
	TIME_END(build);

	ImGui::Text("Octree stats:  node_count %d  node_size %d B  total_size %.3f KB", octree.node_count, octree.node_size, octree.total_size / 1024.0f);

	TIME_START(raytrace);
	float aspect = (float)input.window_size.x / (float)input.window_size.y;

	int2 res;
	res.y = resolution;
	res.x = roundi(aspect * (float)resolution);
	res = max(res, 1);

	if (!equal(renderimage.size, res)) {
		renderimage = Image<lrgba>(res);
	}

	for (int y=0; y<res.y; ++y) {
		for (int x=0; x<res.x; ++x) {
			float4 col = raytrace_pixel(int2(x,y), view);
			renderimage.set(x,y, col);
		}
	}
	TIME_END(raytrace);

	rendertexture.upload(renderimage, false, false);

	ImGui::Text("Raytrace performance:  build_octree %7.3f ms  raytrace %7.3f ms", __build_time * 1000, __raytrace_time * 1000);

}

void Raytracer::draw () {
	if (shader) {
		shader.bind();

		glBindVertexArray(vao);

		shader.set_uniform("slider", slider);

		glActiveTexture(GL_TEXTURE0 + 0);
		shader.set_texture_unit("rendertexture", 0);
		voxel_sampler.bind(0);
		rendertexture.bind();

		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
}