#pragma once
#include "common.hpp"
#include "vulkan_helper.hpp"
#include "vulkan_window.hpp"
#include "vulkan_shaders.hpp"
#include "chunk_mesher.hpp"
#include "graphics.hpp"

namespace vk {

struct Renderer;

struct ChunkRenderer {
	static constexpr uint64_t ALLOC_SIZE = 64 * (1024ull * 1024); // size of vram allocations
	static constexpr int SLICES_PER_ALLOC = (int)(ALLOC_SIZE / CHUNK_SLICE_BYTESIZE);

	static constexpr uint64_t STAGING_SIZE = 1 * (1024ull * 1024);

	static constexpr uint64_t MAX_ALLOCS = 64;

	struct PerDrawData {
		float4 chunk_pos; // pad to float4 to avoid std140 padding rules
	};

	static constexpr int MAX_UBO_SIZE = 64*1024;
	static constexpr int MAX_UBO_SLICES = MAX_UBO_SIZE / sizeof(PerDrawData);
	static_assert(SLICES_PER_ALLOC <= MAX_UBO_SLICES, "too many SLICES_PER_ALLOC to allow PerDrawData to fit in UBO!");

	struct AllocBlock {
		Allocation		mesh_data;

	};
	struct UploadSlice {
		uint32_t		slice_id;
		ChunkSliceData*	data;
		size_t			vertex_count;
	};

	struct StagingBuf {
		Allocation	buf;
		void*		mapped_ptr;
	};

	struct FrameData {
		std_vector<StagingBuf> staging_bufs;

		int slices_end; // one past max slice id allocated
	};

	std_vector<AllocBlock>	allocs;
	std_vector<FrameData>	frames;

	VkPipelineLayout		pipeline_layout;
	Pipeline*				opaque_pipeline;
	Pipeline*				transparent_pipeline;

	bool is_slice_allocated (Chunks& chunks, slice_id id) {
		auto i = id / 64;
		auto j = id % 64;
		if (i >= chunks.slices_alloc.bits.size())
			return false;
		return ((chunks.slices_alloc.bits[i] >> j) & 1) == 0;
	}

	void imgui (Chunks& chunks) {

		size_t vertices = 0;
		for (size_t i=0; i<chunks.slices.size(); ++i) {
			if (is_slice_allocated(chunks, (slice_id)i)) {
				vertices += chunks.slices[i].vertex_count;
			}
		}

		ImGui::Text("Mesh allocs: %2d  slices: %5d  vertices: %12s",
			allocs.size(), chunks.slices.size(), format_thousands(vertices).c_str());
		ImGui::Text("Mesh VRAM: used: %7.3f MB  commited: %7.3f MB (%6.2f%% usage)",
			(float)(vertices * sizeof(BlockMeshInstance)) / 1024 / 1024,
			(float)(allocs.size() * ALLOC_SIZE) / 1024 / 1024,
			(float)(vertices * sizeof(BlockMeshInstance)) / (float)(allocs.size() * ALLOC_SIZE) * 100);

		ImGui::Text("Staging bufs [frames]:");
		for (auto& f : frames) {
			ImGui::SameLine();
			ImGui::Text(" %d", f.staging_bufs.size());
		}

		if (ImGui::TreeNode("slices")) {
			for (size_t i=0; i<chunks.slices.size(); ++i) {
				if (!is_slice_allocated(chunks, (slice_id)i)) {
					ImGui::Text("[%5d] <not allocated>", i);
				} else {
					ImGui::Text("[%5d]vertices: %7d  (%3.0f%%)", i, chunks.slices[i].vertex_count,
						(float)(chunks.slices[i].vertex_count * sizeof(BlockMeshInstance)) / (float)CHUNK_SLICE_BYTESIZE * 100);
				}
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("slices alloc")) {
			print_bitset_allocator(chunks.slices_alloc, CHUNK_SLICE_BYTESIZE, ALLOC_SIZE);
			ImGui::TreePop();
		}

	}

	void new_alloc (VulkanWindowContext& ctx) {
		ZoneScopedC(tracy::Color::Crimson);
		
		AllocBlock alloc;

		alloc.mesh_data = allocate_buffer(ctx.dev, ctx.pdev, ALLOC_SIZE,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		GPU_DBG_NAMEf(ctx, alloc.mesh_data.buf, "slices.allocs[%d].mesh_data", allocs.size());
		GPU_DBG_NAMEf(ctx, alloc.mesh_data.mem, "slices.allocs[%d].mesh_mem", allocs.size());

		allocs.push_back(alloc);
	}
	void free_alloc (VkDevice dev, AllocBlock& alloc) {
		ZoneScopedC(tracy::Color::Crimson);
		alloc.mesh_data.free(dev);
	}

	StagingBuf new_staging_buffer (VulkanWindowContext& ctx, int cur_frame) {
		ZoneScopedC(tracy::Color::Crimson);
		
		auto buf = allocate_buffer(ctx.dev, ctx.pdev, STAGING_SIZE,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		GPU_DBG_NAMEf(ctx, buf.buf, "slices.frames[%d].staging_buf", cur_frame);
		GPU_DBG_NAMEf(ctx, buf.mem, "slices.frames[%d].staging_mem", cur_frame);

		void* ptr;
		vkMapMemory(ctx.dev, buf.mem, 0, STAGING_SIZE, 0, &ptr);

		return { buf, ptr };
	}
	void free_staging_buffer (VkDevice dev, StagingBuf& buf) {
		ZoneScopedC(tracy::Color::Crimson);
		vkUnmapMemory(dev, buf.buf.mem);
		buf.buf.free(dev);
	}

	void create (VulkanWindowContext& ctx, PipelineManager& pipelines, VkRenderPass main_renderpass, VkDescriptorSetLayout common, int frames_in_flight);
	void destroy (VkDevice dev) {
		for (auto& f : frames) {
			for (auto& buf : f.staging_bufs)
				free_staging_buffer(dev, buf);
		}
		for (auto& a : allocs)
			free_alloc(dev, a);

		vkDestroyPipelineLayout(dev, pipeline_layout, nullptr);
	}

	size_t remesh_chunks_count;
	void upload_slices (Chunks& chunks, chunk_id chunkid, slice_id* chunk_slices, uint16_t type, MeshData& mesh, Renderer& r);

	void queue_remeshing (Renderer& r, RenderData& data);

	std_vector<UploadSlice> uploads;
	void upload_remeshed (VulkanWindowContext& ctx, Renderer& r, VkCommandBuffer cmds, Chunks& chunks, int cur_frame);

	void draw_chunks (VulkanWindowContext& ctx, VkCommandBuffer cmds, RenderData& data, bool debug_frustrum_culling, int cur_frame);

};

struct RemeshChunkJob { // Chunk remesh
	// input
	Chunk*					chunk;
	Chunks*					chunks;
	Assets const*			assets;
	WorldGenerator const*	wg;
	// output
	ChunkMesh				mesh;

	void execute ();
};

inline auto parallelism_threadpool = Threadpool<RemeshChunkJob>(parallelism_threads, parallelism_threads_prio, ">> parallelism threadpool" ); // parallelism_threads - 1 to let main thread contribute work too

} // namespace vk
