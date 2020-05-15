#pragma once
#include "graphics_common.hpp"
#include "../blocks.hpp"
#include "../util/raw_array.hpp"

class Chunk;
class Chunks;
class Graphics;

struct RaytraceHit {
	bool did_hit = false;

	float dist;
	float3 pos_world;

	block_id id;

	operator bool () { return did_hit; }
};

// non-sparse (2 B per node)
// ~3584 KB
// ~13 ms

// sparse (36 B per node)
// ~762 KB
// ~10.5 ms

// sparse, memory optimized (4 B per node)
// ~84.7 KB
// ~10 ms

// what did I change?
// ~9 ms

// vector-wise sse (float3 becomes sse _m128 register) -> not how simd is supposed to be used but works a bit
// ~7 ms

// first_node() simplification
// <7 ms ?

#define SPARSE_OCTREE 1

#include <xmmintrin.h>
#include <immintrin.h>

namespace otr {

#define NOINLINE __declspec(noinline)

	inline float _m128_true () {
		uint32_t i = 0xffffffffu;
		return *((float*)&i);
	}
#define _M128_TRUE _m128_true()
#define _M128_BOOL(b) (b ? _M128_TRUE : 0.0f)

	struct bool3 {
		__m128 v;

		inline bool3 (__m128 v): v{v} {}
		inline bool3 (): v{ _mm_undefined_ps() } {}
		inline bool3 (bool all): v{ all ? _mm_castsi128_ps(_mm_set1_epi32(0xffffffffu)) : _mm_set1_ps(0) } {}

		inline bool3 (bool x, bool y, bool z):
			v{ _mm_set_ps(_mm_undefined_ps().m128_f32[0], _M128_BOOL(z), _M128_BOOL(y), _M128_BOOL(x)) } {}
	};
	struct float3 {
		__m128 v;

		inline float3 (): v{ _mm_undefined_ps() } {}
		inline float3 (__m128 v): v{v} {}

		inline float3 (float all): v{ _mm_set_ps1(all) } {}
		inline float3 (::float3 v):
			v{ _mm_set_ps(_mm_undefined_ps().m128_f32[0], v.z, v.y, v.x) } {}

		float& operator[] (int indx) {
			return v.m128_f32[indx];
		}

		operator ::float3 () {
			return { v.m128_f32[0], v.m128_f32[1], v.m128_f32[2] };
		}
	};

	inline float3 select (bool3 c, float3 l, float3 r) {
		__m128 a = _mm_and_ps(c.v, l.v);
		__m128 b = _mm_andnot_ps(c.v, r.v);
		__m128 res = _mm_or_ps(a, b);
		return { res };
	}

	inline float3 operator+ (float3 l, float3 r) {
		return { _mm_add_ps(l.v, r.v) };
	}
	inline float3 operator- (float3 l, float3 r) {
		return { _mm_sub_ps(l.v, r.v) };
	}
	inline float3 operator* (float3 l, float3 r) {
		return { _mm_mul_ps(l.v, r.v) };
	}
	inline float3 operator/ (float3 l, float3 r) {
		return { _mm_div_ps(l.v, r.v) };
	}

	inline bool3 operator!= (bool3 l, bool3 r) {
		return { _mm_cmpneq_ps(l.v, r.v) };
	}
	inline bool3 operator!= (float3 l, float3 r) {
		return { _mm_cmpneq_ps(l.v, r.v) };
	}
	inline bool3 operator< (float3 l, float3 r) {
		return { _mm_cmplt_ps(l.v, r.v) };
	}

	inline bool3 operator^ (bool3 l, bool3 r) {
		return { _mm_xor_ps(l.v, r.v) };
	}

	inline float min_component (float3 v) {
		__m128 x = v.v;
		__m128 y = _mm_shuffle_ps(v.v, v.v, _MM_SHUFFLE(0,0,0,1));
		__m128 z = _mm_shuffle_ps(v.v, v.v, _MM_SHUFFLE(0,0,0,2));
		__m128 res = _mm_min_ss(x, y);
		res = _mm_min_ss(res, z);
		return res.m128_f32[0];
	}
	inline float max_component (float3 v) {
		__m128 x = v.v;
		__m128 y = _mm_shuffle_ps(v.v, v.v, _MM_SHUFFLE(0,0,0,1));
		__m128 z = _mm_shuffle_ps(v.v, v.v, _MM_SHUFFLE(0,0,0,2));
		__m128 res = _mm_max_ss(x, y);
		res = _mm_max_ss(res, z);
		return res.m128_f32[0];
	}

	inline int min_component_indx (float3 v) {
		if (v.v.m128_f32[0] < v.v.m128_f32[1] && v.v.m128_f32[0] < v.v.m128_f32[2])
			return 0;
		if (v.v.m128_f32[1] < v.v.m128_f32[2])
			return 1;
		return 2;
	}
	inline int max_component_indx (float3 v) {
		if (v.v.m128_f32[0] > v.v.m128_f32[1] && v.v.m128_f32[0] > v.v.m128_f32[2])
			return 0;
		if (v.v.m128_f32[1] > v.v.m128_f32[2])
			return 1;
		return 2;
	}

	struct Ray {
		float3 pos;
		float3 dir;
	};

	struct Octree {
		std::vector<RawArray<block_id>> levels; // non-sparse version for comparison

	#if SPARSE_OCTREE
		union Node {
			// MSB set if has children, mask MSB to zero to get actual children index
			// after masking MSB to 0 -> index of 8 consecutive chilren nodes in nodes array, only valid if bid == B_NULL
			uint32_t _children;

			struct { // payload if leaf node, ie. when !has_children
				block_id bid; // == B_NULL -> this has child nodes   != B_NULL -> this is a leaf node
				uint16_t _padding;
			};

			bool has_children () {
				return _children & 0x80000000u;
			}
			uint32_t children_indx () {
				return _children & 0x7fffffffu;
			}
			void set_children_indx (uint32_t childen_indx) {
				_children = childen_indx | 0x80000000u;
			}
		};

		std::vector<Node> nodes;
		int root; // root index

		int node_count;
		int node_size = sizeof(Node);
		int total_size;

	#else

		int node_count;
		int node_size = sizeof(block_id);
		int total_size;
	#endif

		float3 pos;

		void build_non_sparse_octree (Chunk* chunk);

		void recurs_draw (int3 index, int level, float3 offset, int& cell_count);

		RaytraceHit raycast (Ray ray);
	};
}
using otr::Octree;

struct OctreeDevTest {

	float2 ray_ang = float2(-50, 30);
	Ray ray = { float3(5,5,40) };

	Octree octree;

	void draw (Chunks& chunks);
};

class Raytracer {
public:

	Octree octree;

	Shader shader = Shader("raytrace_display", { FOG_UNIFORMS });

	gl::Vao vao; // empty vao even though I generate the mesh in the vertex shader, no vao works but generates an error on my machine

	Sampler voxel_sampler = Sampler(gl::Enum::NEAREST, gl::Enum::NEAREST, gl::Enum::CLAMP_TO_EDGE);

	Image<lrgba> renderimage;
	Texture2D rendertexture;

	bool raytracer_draw = true;
	bool overlay = false;
	float slider = 1.0f;

	bool visualize_time = true;
	int visualize_max_time = 250;

	bool visualize_time_compare = false;
	bool visualize_time_compare_diff = true;
	float visualize_time_slider = 0.5f;

	int resolution = 100; // vertical

	void imgui (Chunks& chunks) {
		if (!imgui_push("Raytracer")) return;

		ImGui::Checkbox("draw", &raytracer_draw);
		ImGui::Checkbox("overlay", &overlay);
		ImGui::SliderFloat("slider", &slider, 0,1);

		ImGui::Checkbox("visualize_time", &visualize_time);
		ImGui::DragInt("visualize_max_time", &visualize_max_time, 1);

		ImGui::Checkbox("visualize_time_compare", &visualize_time_compare);
		ImGui::Checkbox("visualize_time_compare_diff", &visualize_time_compare_diff);
		ImGui::SliderFloat("visualize_time_slider", &visualize_time_slider, 0,1);

		ImGui::SliderInt("resolution", &resolution, 1, 1440);

		imgui_pop();
	}

	lrgba raytrace_pixel (int2 pixel, Camera_View const& view);

	void raytrace (Chunks& chunks, Camera_View const& view);
	void draw ();
};
