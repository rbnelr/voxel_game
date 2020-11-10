#pragma once
#include "common.hpp"
#include "vulkan_helper.hpp"
#include "chunk_mesher.hpp"
#include "graphics.hpp"

namespace vk {

struct Renderer;

struct ChunkRenderer {
	static constexpr uint64_t ALLOC_SIZE = 64 * (1024ull * 1024); // size of vulkan allocations

	struct UploadSlice {
		ChunkSliceData*	data;
		size_t			vertex_count;
	};
	struct DrawSlice {
		// data is implicitly placed in allocs based on the slice id
		uint32_t		vertex_count;
	};

	struct FrameData {
		Allocation staging_buf;
	};

	std::vector<Allocation>	allocs;
	std::vector<FrameData>	frames;

	std::vector<DrawSlice>	slices;

	void create (VkDevice dev, VkPhysicalDevice pdev, int frames_in_flight) {
		for (int i = 0; i < frames_in_flight; ++i) {
			frames.push_back({ allocate_buffer(dev, pdev, ALLOC_SIZE,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
			});
		}
		allocs.push_back( allocate_buffer(dev, pdev, ALLOC_SIZE,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		);
	}
	void destroy (VkDevice dev) {
		for (auto& f : frames) {
			vkDestroyBuffer(dev, f.staging_buf.buf, nullptr);
			vkFreeMemory(dev, f.staging_buf.mem, nullptr);
		}
		for (auto& a : allocs) {
			vkDestroyBuffer(dev, a.buf, nullptr);
			vkFreeMemory(dev, a.mem, nullptr);
		}
	}

	bool stop = false;

	size_t remesh_chunks_count;
	void upload_slices (Chunks& chunks, Chunk* chunk, MeshData& mesh, Renderer& r);

	void queue_remeshing (Renderer& r, RenderData& data);

	void upload_remeshed ();

	void draw_chunks (VkCommandBuffer cmds);
};

struct RemeshChunkJob : ThreadingJob { // Chunk remesh
// input
	Chunk* chunk;
	Chunks& chunks;
	Assets			const& assets;
	WorldGenerator	const& wg;
	Renderer& renderer;
	// output
	ChunkMesh		mesh;

	RemeshChunkJob (Chunk* chunk, Chunks& chunks, Assets const& assets, WorldGenerator const& wg, Renderer& renderer) :
		chunk{ chunk }, chunks{ chunks }, assets{ assets }, wg{ wg }, renderer{ renderer }, mesh{} {}
	virtual ~RemeshChunkJob() = default;

	virtual void execute () {
		mesh_chunk(assets, wg, chunk, &mesh);
	}
	virtual void finalize ();
};

} // namespace vk
