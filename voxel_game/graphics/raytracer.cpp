#include "raytracer.hpp"
#include "graphics.hpp"
#include "../util/timer.hpp"
#include "../chunks.hpp"

#include "../util/simd.hpp"

//#define DECL 
#define DECL _declspec(noinline)
#define CALL _vectorcall

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

struct sse_RaytraceHit {
	sse_bool did_hit = false;

	sse_float dist;
	sse_float3 pos_world;

	sse_int bid;
};

struct SIMD_Raytracer {

	Octree& octree;
	Image<lrgba>& renderimage;
	Camera_View const& view;
	Raytracer& r;

	//
	sse_RaytraceHit hit;

	//
	#define PIXEL_PATTERN sse_int2{ SETi(0,1,0,1,0,1,0,1), SETi(0,0,1,1,2,2,3,3) }

	sse_int mirror_mask_int; // algo assumes ray.dir x,y,z >= 0, x,y,z < 0 cause the ray to be mirrored, to make all components positive, this mask is used to also mirror the octree nodes to make iteration work
	sse_bool3 mirror_mask;

	sse_float3 ray_pos;
	sse_float3 ray_dir;

	// An Efficient Parametric Algorithm for Octree Traversal
	// J. Revelles, C.Ure ̃na, M.Lastra
	// http://citeseerx.ist.psu.edu/viewdoc/download;jsessionid=F6810427A1DC4136D615FBD178C1669C?doi=10.1.1.29.987&rep=rep1&type=pdf
		
	struct sse_OctreeNode {
		sse_int node_data;
		int level;
		sse_float3 min;
		sse_float3 mid;
		sse_float3 max;

		DECL sse_OctreeNode CALL get_child (sse_int index, sse_bool3 mask, Octree const& octree, sse_bool active) const {
			sse_OctreeNode ret;

			sse_int child_indicies = (node_data & sse_int(0x7fffffffu)) + index;

			ret.node_data = gather(&octree.nodes[0], child_indicies, active, 0);
			ret.level = level - 1;
			ret.min = select(mask, mid, min);
			ret.max = select(mask, max, mid);
			ret.mid = 0.5f * (ret.max + ret.min);

			return ret;
		}
	};

	static constexpr int node_seq_lut[8*4] = {
		1, 2, 4,  -1, // 001 010 100
		8, 3, 5,  -1, //   - 011 101
		3, 8, 6,  -1, // 011   - 110
		8, 8, 7,  -1, //   -   - 111
		5, 6, 8,  -1, // 101 110   -
		8, 7, 8,  -1, //   - 111   -
		7, 8, 8,  -1, // 111   -   -
		8, 8, 8,  -1, //   -   -   -
	};

	DECL void CALL traverse_octree (sse_OctreeNode const& root) {
		mirror_mask_int = 0;

		for (int i=0; i<3; ++i) {
			sse_bool mask = ray_dir[i] < 0;
			ray_pos[i] = select(mask, fmsub(root.mid[i], 2, ray_pos[i]), ray_pos[i]);
			ray_dir[i] = select(mask, -ray_dir[i], ray_dir[i]);
			mirror_mask[i] = mask;
			mirror_mask_int = mirror_mask_int | (sse_int(1 << i) & (sse_int)mask);
		}

		sse_float3 rdir_inv = 1.0f / ray_dir;

		sse_float3 t0 = (root.min - ray_pos) * rdir_inv;
		sse_float3 t1 = (root.max - ray_pos) * rdir_inv;

		sse_bool active = max_component(t0) < min_component(t1);
		if (any(active))
			traverse_subtree(root, t0, t1, active);
	}

	DECL sse_int CALL first_node (sse_float3 t0, sse_float3 tm) {
		sse_float cond = max_component(t0);

		sse_int ret;
		ret =        (sse_int)(tm.x < cond) & 1;
		ret = ret | ((sse_int)(tm.y < cond) & 2);
		ret = ret | ((sse_int)(tm.z < cond) & 4);
		return ret;
	}
	DECL sse_int CALL next_node (sse_float3 t1, sse_int indices_offset, sse_bool active) {
		sse_int min_comp = min_component_indx(t1);

		return gather(node_seq_lut, min_comp + indices_offset, active, sse_int(8));
	}

	DECL sse_bool CALL traverse_subtree (sse_OctreeNode const& node, sse_float3 t0, sse_float3 t1, sse_bool active=true) {

		active = active & (min_component(t1) >= 0); // if (min_component(t1) < 0) return false;
		
		sse_bool stop;
		sse_bool decend = eval_octree_cell(node, t0, t1, active, &stop);
		active = decend;

		if (any(decend)) {

			sse_float3 tm = select(ray_dir != 0.0f, 0.5f * (t0 + t1), select(ray_pos < node.mid, float3(+INF), float3(-INF)));

			sse_int cur_node = first_node(t0, tm);
			
			do {
				sse_bool3 mask;
				mask.x = (cur_node & sse_int(1)) == 1;
				mask.y = (cur_node & sse_int(2)) == 2;
				mask.z = (cur_node & sse_int(4)) == 4;

				auto inner_stop = traverse_subtree(node.get_child(cur_node ^ mirror_mask_int, mask ^ mirror_mask, octree, active),
					select(mask, tm, t0), select(mask, t1, tm), active);

				active = active & !stop;
				stop = stop | (inner_stop);

				//if (all(!active))
				//	break;

				cur_node = next_node(select(mask, t1, tm), cur_node << 2, active); // * 4 to get right offset for row in node_seq_lut

				active = active & cur_node < 8;
			} while (any(active));
		}

		return stop;
	}

	DECL sse_bool CALL eval_octree_cell (sse_OctreeNode const& node, sse_float3 t0, sse_float3 t1, sse_bool active, sse_bool* stop_traversal) {
		
		sse_bool has_children = (node.node_data & sse_int(0x80000000u)) == sse_int(0x80000000u);

		sse_bool decend = has_children; // need to decend further down into octree to find actual voxels
		{ // if (has_children) return true; // need to decend further down into octree to find actual voxels
			sse_int bid = node.node_data; // octree.nodes[node.oct_idx].bid;

			sse_bool did_hit = !(decend | (bid == (sse_int)B_AIR)) & active;
			{ // if (did_hit)
				sse_float hit_dist = max_component(t0);
				sse_float3 pos_world = fmadd(ray_dir, hit_dist, ray_pos);

				hit.did_hit		= select(did_hit, sse_bool(true),	hit.did_hit);
				hit.bid			= select(did_hit, bid,				hit.bid);
				hit.dist		= select(did_hit, hit_dist,			hit.dist);
				hit.pos_world	= select(did_hit, pos_world,		hit.pos_world);
			}

			*stop_traversal = did_hit | !active;
		}
		return decend & active;
	}

	DECL void CALL raycast () {

		sse_OctreeNode o;
		o.node_data = octree.root | 0x80000000;
		o.level = (int)octree.levels.size() - 1;
		o.min = octree.pos;
		o.max = octree.pos + (float3)(float)(1 << o.level);
		o.mid = 0.5f * (o.min + o.max);

		traverse_octree(o);
	}

	#define TIME_START(name) auto __##name = Timer::start()
	#define TIME_END(name) auto __##name##_time = __##name.end()
	
	DECL void CALL ray_for_pixel (sse_int2 pixel, sse_float2 ndc_scale, sse_float2 ndc_offs) {
		sse_float2 ndc = fmadd((sse_float2)pixel, ndc_scale, ndc_offs);

		sse_float4 clip = sse_float4(ndc * sse_float2(view.clip_near), SET1(-view.clip_near), SET1(view.clip_near)); // ndc = clip / clip.w;

		sse_float3 pos_cam = matmul(view.clip_to_cam, clip);
		sse_float3 dir_cam = pos_cam;

		ray_pos = matmul_transl(view.cam_to_world, pos_cam);
		ray_dir = matmul       (view.cam_to_world, dir_cam);
		ray_dir = normalize(ray_dir);
	}

	DECL void CALL raytrace_pixel (sse_int2 pixel) {

		hit = {};

	auto time0 = get_timestamp();
		raycast();
	auto time = (int)(get_timestamp() - time0);
		
		sse_float4 col;

		if (r.visualize_time) {

			float diff_mag = (float)abs(time) / (float)r.visualize_max_time;

			float4 _col;
			if (time > 0)
				_col = float4(diff_mag, 0, 0, 1);
			else
				_col = float4(0, diff_mag, 0, 1);

			col = sse_float4(_col);
		} else {
			col = select(hit.did_hit, sse_float4(hit.dist / 32, 1), sse_float4(0));
		}

		for (int i=0; i<8; ++i) {
			renderimage.set(pixel.x.m256i_i32[i], pixel.y.m256i_i32[i], col[i]);
		}
	}

	void execute () {
		float2 _ndc_scale = 1.0f / (float2)renderimage.size;
		sse_float2 ndc_offs = _ndc_scale - float2(1.0f);
		sse_float2 ndc_scale = _ndc_scale * float2(2);

		for (int y=0; y+3 < renderimage.size.y; y += 4) {
			int x = 0;
			for (; x+1 < renderimage.size.x; x += 2) {

				sse_int2 pixel = sse_int2(SET1i(x), SET1i(y)) + PIXEL_PATTERN;

				ray_for_pixel(pixel, ndc_scale, ndc_offs);

				raytrace_pixel(pixel);
			}
		}
	}
};

////
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
	
	SIMD_Raytracer r = { octree, renderimage, view, *this };
	r.execute();
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
