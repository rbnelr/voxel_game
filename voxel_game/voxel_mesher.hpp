#pragma once
#include "stdafx.hpp"
#include "util/virtual_allocator.hpp"
#include "threading.hpp"
#include "graphics/graphics.hpp"

struct WorldGenerator;
class Voxels;
namespace svo {
	struct Page;
}

namespace meshing {
	struct Vertex {
		float3	pos_model;
		float2	uv;
		uint8	tex_indx;
		uint8	block_light;
		uint8	sky_light;
		uint8	hp;

		static void bind (Attributes& a) {
			int cur = 0;
			a.add    <decltype(pos_model  )>(cur++, "pos_model" ,  sizeof(Vertex), offsetof(Vertex, pos_model  ));
			a.add    <decltype(uv         )>(cur++, "uv",          sizeof(Vertex), offsetof(Vertex, uv         ));
			a.add_int<decltype(tex_indx   )>(cur++, "tex_indx",    sizeof(Vertex), offsetof(Vertex, tex_indx   ));
			a.add    <decltype(block_light)>(cur++, "block_light", sizeof(Vertex), offsetof(Vertex, block_light), true);
			a.add    <decltype(sky_light  )>(cur++, "sky_light",   sizeof(Vertex), offsetof(Vertex, sky_light  ), true);
			a.add    <decltype(hp         )>(cur++, "hp",          sizeof(Vertex), offsetof(Vertex, hp         ), true);
		}
	};

	static constexpr int MAX_PAGES = 2 << 14;

	struct Page;

	struct PageInfo {
		Page* next;
		uint32_t count;
		uint32_t _pad;
	};

	static inline constexpr uint32_t PAGE_SIZE = 4096;
	static inline constexpr uint32_t PAGE_VERTEX_COUNT = (PAGE_SIZE - sizeof(PageInfo)) / sizeof(Vertex);

	struct Page {
		PageInfo info;
		Vertex vertices[PAGE_VERTEX_COUNT];
	};

	static_assert(sizeof(Page) == PAGE_SIZE);

	struct VoxelMesher {
	//	ThreadsafeSparseAllocator<Page> allocator = ThreadsafeSparseAllocator<Page>(MAX_PAGES);
	//
	//	void imgui () {
	//
	//	}
	//
	//	Vertex* push_vertex (Page** cur_page) {
	//		assert((*cur_page)->info.next == nullptr);
	//		if (*cur_page == nullptr || (*cur_page)->info.count == PAGE_VERTEX_COUNT) {
	//			auto* newpage = allocator.alloc_threadsafe();
	//			newpage->info.next = nullptr;
	//			newpage->info.count = 0;
	//
	//			if (*cur_page != nullptr)
	//				(*cur_page)->info.next = newpage;
	//
	//			*cur_page = newpage;
	//		}
	//
	//		return &(*cur_page)->vertices[ (*cur_page)->info.count++ ];
	//	}
	//	void free_vertices (Page* vertices) {
	//		while (vertices) {
	//			auto* next = vertices->info.next;
	//			allocator.free_threadsafe(vertices);
	//			vertices = next;
	//		}
	//	}
	};

	struct MeshingJob : ThreadingJob {
		// input
		svo::Page* page;
		Voxels const* voxels;
		Graphics const* graphics;
		WorldGenerator const* wg; 
		// output
		Page* opaque_mesh;
		Page* transparent_mesh;

		virtual void execute ();
	};
}
using meshing::VoxelMesher;
