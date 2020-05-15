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

// An Efficient Parametric Algorithm for Octree Traversal
// J. Revelles, C.Ure ̃na, M.Lastra
// http://citeseerx.ist.psu.edu/viewdoc/download;jsessionid=F6810427A1DC4136D615FBD178C1669C?doi=10.1.1.29.987&rep=rep1&type=pdf

struct Data {
	int mirror_mask_int; // algo assumes ray.dir x,y,z >= 0, x,y,z < 0 cause the ray to be mirrored, to make all components positive, this mask is used to also mirror the octree nodes to make iteration work
	bool3 mirror_mask; // algo assumes ray.dir x,y,z >= 0, x,y,z < 0 cause the ray to be mirrored, to make all components positive, this mask is used to also mirror the octree nodes to make iteration work

	float3 ray_pos;
	float3 ray_dir;

	Octree* octree;
	RaytraceHit hit;
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

bool hit_octree_leaf (Data& d, uint32_t node_data, float3 t0) {
	auto b = (block_id)node_data;

	bool did_hit = b != B_AIR;
	if (did_hit) {
		d.hit.did_hit = true;
		d.hit.id = b;
		d.hit.dist = max_component(t0);
		d.hit.pos_world = d.ray_pos + d.ray_dir * d.hit.dist;
	}

	return did_hit;
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
	int min_comp;
	min_component(t1, &min_comp);

	return indices[min_comp];
}

bool traverse_subtree (Data& d, uint32_t node_data, float3 min, float3 max, float3 t0, float3 t1) {

	if (any(t1 < 0))
		return false;

	bool has_children = node_data & 0x80000000u;
	if (!has_children)
		return hit_octree_leaf(d, node_data, t0);

	// need to decend further down into octree to find actual voxels
		
	float3 mid = 0.5f * (min + max);
	float3 tm = select(d.ray_dir != 0, 0.5f * (t0 + t1), select(d.ray_pos < mid, float3(+INF), float3(-INF)));

	int cur_node = first_node(t0, tm);

	do {
		bool3 mask = bool3(cur_node & 1, cur_node & 2, cur_node & 4);

		//
		int node_index = cur_node ^ d.mirror_mask_int;
		bool3 node_mask = mask != d.mirror_mask;

		int children_index = (node_data & 0x7fffffffu) + node_index;

		uint32_t _node_data = d.octree->nodes[children_index]._children;
		float3 _min = select(node_mask, mid, min);
		float3 _max = select(node_mask, max, mid);

		if (traverse_subtree(d, _node_data, _min, _max, select(mask, tm, t0), select(mask, t1, tm)))
			return true; // hit in subtree

		cur_node = next_node(select(mask, t1, tm), node_seq_lut[cur_node]);
	} while (cur_node < 8);

	return false; // no hit in this node, step into next node
}

void ray_for_pixel (Data* d, int2 pixel, int2 resolution, Camera_View const& view) {
	float2 ndc = ((float2)pixel + 0.5f) / (float2)resolution * 2 - 1;

	//float4 near_plane_clip = view.cam_to_clip * float4(0, 0, -view.clip_near, 1);
	float4 near_plane_clip = float4(0, 0, -view.clip_near, view.clip_near);

	float4 clip = float4(ndc, -1, 1) * near_plane_clip.w; // ndc = clip / clip.w;

	float3 pos_cam = (float3)(view.clip_to_cam * clip);
	float3 dir_cam = pos_cam;

	d->ray_pos = (float3)( view.cam_to_world * float4(pos_cam, 1) );
	d->ray_dir = (float3)( view.cam_to_world * float4(dir_cam, 0) );
	d->ray_dir = normalize(d->ray_dir);
}

RaytraceHit Octree::raycast (int2 pixel, Camera_View const& view, Image<lrgba>* image) {

	Data d;
	d.hit.did_hit = false;
	d.octree = this;

	ray_for_pixel(&d, pixel, image->size, view);

	d.mirror_mask_int = 0;

	uint32_t node_data = nodes[root]._children;
	float3 min = pos;
	float3 max = pos + (float3)(float)CHUNK_DIM_X;
	float3 mid = 0.5f * (min + max);

	for (int i=0; i<3; ++i) {
		bool mirror = d.ray_dir[i] < 0;

		d.ray_dir[i] = abs(d.ray_dir[i]);
		d.mirror_mask[i] = mirror;
		if (mirror) d.ray_pos[i] = mid[i] * 2 - d.ray_pos[i];
		if (mirror) d.mirror_mask_int |= 1 << i;
	}

	float3 rdir_inv = 1.0f / d.ray_dir;

	float3 t0 = (min - d.ray_pos) * rdir_inv;
	float3 t1 = (max - d.ray_pos) * rdir_inv;

	if (max_component(t0) < min_component(t1))
		traverse_subtree(d, node_data, min, max, t0, t1);

	return d.hit;
}

#define TIME_START(name) auto __##name = Timer::start()
#define TIME_END(name) auto __##name##_time = __##name.end()

lrgba Raytracer::raytrace_pixel (int2 pixel, Camera_View const& view) {

auto time0 = get_timestamp();
	auto hit = octree.raycast(pixel, view, &renderimage);
auto time = (int)(get_timestamp() - time0);

	if (visualize_time) {
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

std::string raytrace_cl = kiss::load_text_file("shaders/raytrace.cl");

////
void Raytracer::raytrace (Chunks& chunks, Camera_View const& view) {

	if (!init_cl) {
		std::vector<cl::Platform> all_platforms;
		cl::Platform::get(&all_platforms);

		if (all_platforms.size() == 0) {
			printf("OpenCL: No platforms found. Check OpenCL installation!\n");
			return;
		}

		platform = all_platforms[0];
		printf("OpenCL: Using platform: %s\n", platform.getInfo<CL_PLATFORM_NAME>().c_str());

		// get default device of the default platform
		std::vector<cl::Device> all_devices;
		platform.getDevices(CL_DEVICE_TYPE_ALL, &all_devices);

		if (all_devices.size() == 0) {
			printf("OpenCL: No devices found. Check OpenCL installation!\n");
			return;
		}

		device = all_devices[0];
		printf("OpenCL: Using device: %s\n", device.getInfo<CL_DEVICE_NAME>().c_str());

		context = cl::Context({ device });

		cl::Program::Sources sources;

		std::string kernel_code = raytrace_cl;

		sources.push_back({ raytrace_cl.c_str(), raytrace_cl.size() });

		program = cl::Program(context, sources);

		if (program.build({ device }) != CL_SUCCESS) {
			printf("OpenCL: Error building: %s\n", program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(device).c_str());
			return;
		}

		//create queue to which we will push commands for the device.
		queue = cl::CommandQueue(context, device);

		// create buffers on the device
		SVO_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, sizeof(uint32_t) * octree.nodes.size());
		image_buffer = cl::Buffer(context, CL_MEM_READ_WRITE, sizeof(float)*4 * renderimage.size.x * renderimage.size.y);

		//// write SVO
		//queue.enqueueWriteBuffer(SVO_buffer, CL_FALSE, 0, sizeof(uint32_t) * octree.nodes.size(), &octree.nodes[0]);
		//
		//auto simple_add = cl::Kernel(program, "simple_add");
		//simple_add.setArg(0, buffer_A);
		//simple_add.setArg(1, buffer_B);
		//simple_add.setArg(2, renderimage.size.x);
		//queue.enqueueNDRangeKernel(simple_add, cl::NullRange, cl::NDRange(10), cl::NullRange);
		//
		////int C[10];
		////// read result C from the device to array C
		////queue.enqueueReadBuffer(buffer_C, CL_TRUE, 0, sizeof(int)*10, C);
		//
		//queue.finish();

		init_cl = true;
	}

	//return;

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
