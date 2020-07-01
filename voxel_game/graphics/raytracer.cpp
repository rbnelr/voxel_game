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

	pos = (float3)chunk->chunk_pos_world();
}

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

Octree build_octree (Chunk* chunk) {
	static_assert(CHUNK_DIM_X == CHUNK_DIM_Y && CHUNK_DIM_X == CHUNK_DIM_Z);

	Octree o;

	o.build_non_sparse_octree(chunk);

	o.nodes.emplace_back(); // push root
	o.root = 0;
	recurse_build_sparse_octree(0, (int)o.levels.size() - 1, 0, &o);

	o.node_count = (int)o.nodes.size();
	o.total_size = o.node_count * o.node_size;
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

void Octree::recurs_draw (Node& node, int3 index, int level) {
	float voxel_size = (float)(1 << level);

	debug_graphics->push_wire_cube((float3)index + pos + 0.5f * voxel_size, voxel_size * 0.99f, cols[level]);

	if (node.has_children()) {
		Node* children = &nodes[ node.children_indx() ];
		for (int i=0; i<8; ++i) {
			int3 child_indx = int3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
			int child_level = level - 1;
			recurs_draw(*children++, index + int3(child_indx.x << child_level, child_indx.y << child_level, child_indx.z << child_level), child_level);
		}
	}
}

void Octree::debug_draw () {
	recurs_draw(nodes[root], 0, CHUNK_DIM_SHIFT_X);
}

struct EfficentSVORaytracer {
	// https://research.nvidia.com/sites/default/files/pubs/2010-02_Efficient-Sparse-Voxel/laine2010i3d_paper.pdf

	Octree& octree;

	RaytraceHit hit;
	int iterations = 0;

	Ray ray;
	float3 rinv_dir;

	int mirror_mask_int = 0;
	bool3 mirror_mask = false;

	float3 intersect_ray (float3 planes) {
		return rinv_dir * (planes - ray.pos);
	}
	void intersect_ray (int3 cube_pos, int cube_scale, float* t0, float* t1, bool3* exit_faces) {
		float3 cube_min = (float3)cube_pos;
		float3 cube_max = (float3)(cube_pos + (1 << cube_scale));

		float3 t0v = rinv_dir * (cube_min - ray.pos);
		float3 t1v = rinv_dir * (cube_max - ray.pos);

		*t0 = max_component( t0v );
		*t1 = min_component( t1v );

		*exit_faces = *t1 == t1v;
	}

	struct NodeCoord {
		int3 pos; // absolute pos code
		int scale; // inverse depth
	};

	NodeCoord select_child (float t0, NodeCoord parent) {
		NodeCoord child;
		child.scale = parent.scale - 1;

		float3 parent_mid = (float3)(parent.pos + (1 << child.scale));
		
		float3 tmid = intersect_ray(parent_mid);

		bool3 comp = t0 >= tmid;
		int3 bits = (int3)comp;

		child.pos.x = parent.pos.x | (bits.x << child.scale);
		child.pos.y = parent.pos.y | (bits.y << child.scale);
		child.pos.z = parent.pos.z | (bits.z << child.scale);

		return child;
	}

	int highest_differing_bit (int a, int b) {
		unsigned long diff = (unsigned long)(a ^ b); // bita != bitb => diffbit
		
		unsigned long bit_index;
		if (_BitScanReverse(&bit_index, diff))
			return bit_index;

		return -1;
	}
	int highest_differing_bit (int3 a, int3 b) {
		return max(max(highest_differing_bit(a.x, b.x), highest_differing_bit(a.y, b.y)), highest_differing_bit(a.z, b.z));
	}

	RaytraceHit raytrace (Ray ray, float max_dist) {
		ray.pos -= octree.pos;

		float3 root_min = 0;
		float3 root_max = (float3)CHUNK_DIM_X;
		float3 root_mid = 0.5f * (root_min + root_max);

		static constexpr int MAX_SCALE = CHUNK_DIM_SHIFT_X; // 64: 6

		NodeCoord parent_coord = { 0, MAX_SCALE };
		
		// mirror coord system to make all ray dir components positive, like in "An Efficient Parametric Algorithm for Octree Traversal"
		for (int i=0; i<3; ++i) {
			if (ray.dir[i] < 0) {
				ray.pos[i] = root_mid[i] * 2 - ray.pos[i]; // eqivalent to mirror around mid (-1 * (ray.pos[i] - root_mid[i]) + root_mid[i])
				ray.dir[i] = -ray.dir[i];
				mirror_mask_int |= 1 << i;
				mirror_mask[i] = true;
			}
		}

		this->ray = ray;
		rinv_dir = 1.0f / ray.dir;

		Octree::Node parent_node = octree.nodes[ octree.root ];

		// desired ray bounds
		float tmin = 0;
		float tmax = max_dist;

		// cube ray range
		float t0 = max_component( intersect_ray(root_min) );
		float t1 = min_component( intersect_ray(root_max) );

		t0 = max(tmin, t0);
		t1 = min(tmax, t1);

		NodeCoord child_coord = select_child(t0, parent_coord);

		struct StackData {
			Octree::Node parent_node;
			float t1;
		};

		StackData stack[ MAX_SCALE -1 ];

		for (;;) {
			// child cube ray range
			float child_t0, child_t1;
			bool3 exit_face;
			intersect_ray(child_coord.pos, child_coord.scale, &child_t0, &child_t1, &exit_face);

			bool voxel_exists = parent_node.has_children();
			if (voxel_exists && t0 < t1) {

				int idx = 0;
				idx |= ((child_coord.pos.x >> child_coord.scale) & 1) << 0;
				idx |= ((child_coord.pos.y >> child_coord.scale) & 1) << 1;
				idx |= ((child_coord.pos.z >> child_coord.scale) & 1) << 2;

				Octree::Node node = octree.nodes[ parent_node.children_indx() + (idx ^ mirror_mask_int) ];

				//// Intersect
				// child cube ray range
				float tv0 = max(child_t0, t0);
				float tv1 = min(child_t1, t1);

				if (tv0 < tv1) {
					bool leaf = !node.has_children();
					if (leaf) {
						if (node.bid != B_AIR) {
							hit.did_hit = true;
							hit.dist = tv0;
							break; // hit
						}
					} else {
						//// Push
						assert(child_coord.scale >= 0 && child_coord.scale < MAX_SCALE);
						stack[child_coord.scale - 1] = { parent_node, t1 };

						// child becomes parent
						parent_node = node;

						parent_coord = child_coord;
						child_coord = select_child(tv0, parent_coord);

						t0 = tv0;
						t1 = tv1;

						continue;
					}
				}
			}

			int3 old_pos = child_coord.pos;
			bool parent_changed;
			
			{ //// Advance
				int3 step = (int3)exit_face;
				step.x <<= child_coord.scale;
				step.y <<= child_coord.scale;
				step.z <<= child_coord.scale;

				parent_changed =
					(old_pos.x & step.x) != 0 ||
					(old_pos.y & step.y) != 0 ||
					(old_pos.z & step.z) != 0;

				child_coord.pos += step;

				t0 = child_t1;
			}

			if (parent_changed) {
				//// Pop
				child_coord.scale = highest_differing_bit(old_pos, child_coord.pos);

				if (child_coord.scale >= MAX_SCALE)
					break; // out of root

				assert(child_coord.scale >= 0 && child_coord.scale < MAX_SCALE);
				parent_node	= stack[child_coord.scale - 1].parent_node;
				t1			= stack[child_coord.scale - 1].t1;

				int clear_mask = ~((1 << child_coord.scale) - 1);
				child_coord.pos.x &= clear_mask;
				child_coord.pos.y &= clear_mask;
				child_coord.pos.z &= clear_mask;
			}
		}

		return hit;
	};
};

RaytraceHit Octree::raycast (Ray ray) {

	EfficentSVORaytracer t = { *this };
	auto hit = t.raytrace(ray, 100);

	return hit;
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
	auto hit = octree.raycast(ray);
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

	debug_cursor_pos = floori(input.cursor_pos * (float)resolution / (float)input.window_size.y);

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
	if (octree_debug_draw)
		octree.debug_draw();

	if (overlay && shader) {
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
