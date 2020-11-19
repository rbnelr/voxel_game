#pragma once
#include "common.hpp"
#include "vulkan_helper.hpp"
#include "vulkan_window.hpp"
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

	struct StagingBuf {
		Allocation	buf;
		void*		mapped_ptr;
	};

	struct FrameData {
		std::vector<StagingBuf> staging_bufs;
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

	Allocation new_alloc (VulkanWindowContext& ctx) {
<<<<<<< HEAD
		ZoneScopedC(tracy::Color::Crimson);
		
=======
>>>>>>> e23da88abbef911d650508a418b94af69ab8ad7e
		auto buf = allocate_buffer(ctx.dev, ctx.pdev, ALLOC_SIZE,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		GPU_DBG_NAME(ctx, buf.buf, "chunk_slice_buf");
		GPU_DBG_NAME(ctx, buf.mem, "chunk_slice_alloc");
		return buf;
	}
<<<<<<< HEAD
	StagingBuf new_staging_buffer (VulkanWindowContext& ctx, int cur_frame) {
		ZoneScopedC(tracy::Color::Crimson);
		
=======
	Allocation new_staging_buffer (VulkanWindowContext& ctx, int cur_frame) {
>>>>>>> e23da88abbef911d650508a418b94af69ab8ad7e
		auto buf = allocate_buffer(ctx.dev, ctx.pdev, ALLOC_SIZE,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		GPU_DBG_NAMEI(ctx, buf.buf, "chunk_staging_buf[%d]", cur_frame);
		GPU_DBG_NAMEI(ctx, buf.mem, "chunk_staging_alloc[%d]", cur_frame);
<<<<<<< HEAD

		void* ptr;
		vkMapMemory(ctx.dev, buf.mem, 0, ALLOC_SIZE, 0, &ptr);

		return { buf, ptr };
	}
	void free_staging_buffer (VkDevice dev, StagingBuf& buf) {
		ZoneScopedC(tracy::Color::Crimson);
		
		vkUnmapMemory(dev, buf.buf.mem);

		vkDestroyBuffer(dev, buf.buf.buf, nullptr);
		vkFreeMemory(dev, buf.buf.mem, nullptr);
=======
		return buf;
>>>>>>> e23da88abbef911d650508a418b94af69ab8ad7e
	}

	void create (VulkanWindowContext& ctx, int frames_in_flight) {
		frames.resize(frames_in_flight);
	}
	void destroy (VkDevice dev) {
		for (auto& f : frames) {
			for (auto& buf : f.staging_bufs)
				free_staging_buffer(dev, buf);
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
	void upload_remeshed (VulkanWindowContext& ctx, int cur_frame, VkCommandBuffer cmds);

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
