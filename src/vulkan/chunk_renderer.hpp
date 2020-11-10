#pragma once
#include "common.hpp"
#include "vulkan_helper.hpp"
#include "chunk_mesher.hpp"
#include "graphics.hpp"

namespace vk {

struct Renderer;

struct ChunkRenderer {
	static constexpr uint64_t ALLOC_SIZE = 64 * (1024ull * 1024); // size of vulkan allocations
	static constexpr uint64_t SLICES_PER_ALLOC = ALLOC_SIZE / CHUNK_SLICE_BYTESIZE;

	struct UploadSlice {
		uint32_t		slice_id;
		ChunkSliceData*	data;
		size_t			vertex_count;
	};
	struct DrawSlice {
		// data is implicitly placed in allocs based on the slice id
		uint32_t		vertex_count;
	};

	struct FrameData {
		std::vector<Allocation> staging_bufs;
	};

	std::vector<Allocation>	allocs;
	std::vector<FrameData>	frames;

	std::vector<DrawSlice>	slices;

	// offset is in vertices, not bytes
	VkBuffer calc_slice_buf (uint32_t slice_id, uint32_t* out_buf_offset) {
		uint32_t bufi   =  slice_id / SLICES_PER_ALLOC;
		*out_buf_offset = (slice_id % SLICES_PER_ALLOC) * CHUNK_SLICE_LENGTH;
		return allocs[bufi].buf;
	}

	Allocation new_alloc (VkDevice dev, VkPhysicalDevice pdev) {
		return allocate_buffer(dev, pdev, ALLOC_SIZE,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}
	Allocation new_staging_buffer (VkDevice dev, VkPhysicalDevice pdev) {
		return allocate_buffer(dev, pdev, ALLOC_SIZE,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	void create (VkDevice dev, VkPhysicalDevice pdev, int frames_in_flight) {
		frames.resize(frames_in_flight);
	}
	void destroy (VkDevice dev) {
		for (auto& f : frames) {
			for (auto& buf : f.staging_bufs) {
				vkDestroyBuffer(dev, buf.buf, nullptr);
				vkFreeMemory(dev, buf.mem, nullptr);
			}
		}
		for (auto& a : allocs) {
			vkDestroyBuffer(dev, a.buf, nullptr);
			vkFreeMemory(dev, a.mem, nullptr);
		}
	}

	size_t remesh_chunks_count;
	void upload_slices (Chunks& chunks, Chunk* chunk, MeshData& mesh, Renderer& r);

	void queue_remeshing (Renderer& r, RenderData& data);

	std::vector<UploadSlice> uploads;
	void upload_remeshed (VkDevice dev, VkPhysicalDevice pdev, int cur_frame, VkCommandBuffer cmds);

	void draw_chunks (VkCommandBuffer cmds, Chunks& chunks, VkPipeline pipeline, VkPipelineLayout layout);
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
