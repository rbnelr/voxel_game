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

	static constexpr uint64_t STAGING_SIZE = 16 * (1024ull * 1024);

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
	struct DrawSlice {
		// data is implicitly placed in allocs based on the slice id
		uint32_t		vertex_count;
	};

	struct StagingBuf {
		Allocation	buf;
		void*		mapped_ptr;
	};
	struct IndirectDraw {
		Allocation				draw_data;
		Allocation				per_draw_ubo;
		VkDrawIndirectCommand*	draw_data_ptr;
		PerDrawData*			per_draw_ubo_ptr;

		VkDescriptorSet			descriptor_set;
		int						opaque_draw_count;
		int						transparent_draw_count;
	};

	struct FrameData {
		std::vector<StagingBuf> staging_bufs;
		std::vector<IndirectDraw> indirect_draw;

		int slices_end; // one past max slice id allocated
	};

	std::vector<AllocBlock>	allocs;
	std::vector<FrameData>	frames;

	std::vector<DrawSlice>	slices;

	VkDescriptorPool		descriptor_pool;
	VkDescriptorSetLayout	descriptor_layout;
	VkPipelineLayout		pipeline_layout;
	VkPipeline				opaque_pipeline, transparent_pipeline;

	void imgui (Chunks& chunks) {

		size_t vertices = 0;
		for (size_t i=0; i<slices.size(); ++i) {
			if (((chunks.slices_alloc.bits[i/64] >> (i%64)) & 1) == 0) {
				vertices += slices[i].vertex_count;
			}
		}

		ImGui::Text("Mesh allocs: %2d  slices: %5d  vertices: %12s",
			allocs.size(), slices.size(), format_thousands(vertices).c_str());
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
			for (size_t i=0; i<slices.size(); ++i) {
				if (((chunks.slices_alloc.bits[i/64] >> (i%64)) & 1) != 0) {
					ImGui::Text("<not allocated>");
				} else {
					ImGui::Text("vertices: %7d  (%3.0f%%)", slices[i].vertex_count,
						(float)(slices[i].vertex_count * sizeof(BlockMeshInstance)) / (float)CHUNK_SLICE_BYTESIZE * 100);
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

	IndirectDraw new_indirect_draw_buffer (VulkanWindowContext& ctx, int cur_frame, int alloc_i) {
		ZoneScopedC(tracy::Color::Crimson);

		IndirectDraw ret;

		ret.draw_data = allocate_buffer(ctx.dev, ctx.pdev, SLICES_PER_ALLOC * sizeof(VkDrawIndirectCommand),
			VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		GPU_DBG_NAMEf(ctx, ret.draw_data.buf, "slices.frames[%d].indirect_draw[%d].buf", cur_frame, alloc_i);
		GPU_DBG_NAMEf(ctx, ret.draw_data.mem, "slices.frames[%d].indirect_draw[%d].mem", cur_frame, alloc_i);

		ret.per_draw_ubo = allocate_buffer(ctx.dev, ctx.pdev, SLICES_PER_ALLOC * sizeof(PerDrawData),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		GPU_DBG_NAMEf(ctx, ret.per_draw_ubo.buf, "slices.frames[%d].per_draw_ubo[%d].buf", cur_frame, alloc_i);
		GPU_DBG_NAMEf(ctx, ret.per_draw_ubo.mem, "slices.frames[%d].per_draw_ubo[%d].mem", cur_frame, alloc_i);

		vkMapMemory(ctx.dev, ret.draw_data.mem, 0, SLICES_PER_ALLOC * sizeof(VkDrawIndirectCommand), 0, (void**)&ret.draw_data_ptr);
		vkMapMemory(ctx.dev, ret.per_draw_ubo.mem, 0, SLICES_PER_ALLOC * sizeof(PerDrawData), 0, (void**)&ret.per_draw_ubo_ptr);

		{ // create descriptor sets
			VkDescriptorSetAllocateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			info.descriptorPool = descriptor_pool;
			info.descriptorSetCount = 1;
			info.pSetLayouts = &descriptor_layout;

			VK_CHECK_RESULT(vkAllocateDescriptorSets(ctx.dev, &info, &ret.descriptor_set));
			GPU_DBG_NAMEf(ctx, ret.descriptor_set, "slices.frames[%d].indirect_draw[%d].descriptor_set[%d]", cur_frame, alloc_i);

			VkWriteDescriptorSet writes[1] = {};

			VkDescriptorBufferInfo buf = {};
			buf.buffer = ret.per_draw_ubo.buf;
			buf.offset = 0;
			buf.range = VK_WHOLE_SIZE;

			writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet = ret.descriptor_set;
			writes[0].dstBinding = 0;
			writes[0].dstArrayElement = 0;
			writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes[0].descriptorCount = 1;
			writes[0].pBufferInfo = &buf;

			vkUpdateDescriptorSets(ctx.dev, ARRLEN(writes), writes, 0, nullptr);
		}

		return ret;
	}
	void free_indirect_draw_buffer (VkDevice dev, IndirectDraw& buf) {
		ZoneScopedC(tracy::Color::Crimson);
		vkUnmapMemory(dev, buf.draw_data.mem);
		vkUnmapMemory(dev, buf.per_draw_ubo.mem);
		buf.draw_data.free(dev);
		buf.per_draw_ubo.free(dev);

		vkFreeDescriptorSets(dev, descriptor_pool, 1, &buf.descriptor_set);
	}

	void create (VulkanWindowContext& ctx, ShaderManager& shaders, VkRenderPass main_renderpass, VkDescriptorSetLayout common, int frames_in_flight);
	void destroy (VkDevice dev) {
		for (auto& f : frames) {
			for (auto& buf : f.staging_bufs)
				free_staging_buffer(dev, buf);
			for (auto& buf : f.indirect_draw)
				free_indirect_draw_buffer(dev, buf);
		}
		for (auto& a : allocs)
			free_alloc(dev, a);

		if (opaque_pipeline)
			vkDestroyPipeline(dev, opaque_pipeline, nullptr);
		if (transparent_pipeline)
			vkDestroyPipeline(dev, transparent_pipeline, nullptr);
		vkDestroyPipelineLayout(dev, pipeline_layout, nullptr);
		vkDestroyDescriptorPool(dev, descriptor_pool, nullptr);
		vkDestroyDescriptorSetLayout(dev, descriptor_layout, nullptr);
	}

	size_t remesh_chunks_count;
	void upload_slices (Chunks& chunks, std::vector<uint16_t>& chunk_slices, MeshData& mesh, Renderer& r);

	void queue_remeshing (Renderer& r, RenderData& data);

	std::vector<UploadSlice> uploads;
	void upload_remeshed (VulkanWindowContext& ctx, VkCommandBuffer cmds, Chunks& chunks, int cur_frame);

	void draw_chunks (VulkanWindowContext& ctx, VkCommandBuffer cmds, Chunks& chunks, int cur_frame);

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
