#pragma once
#include "common.hpp"
#include "vulkan_helper.hpp"
#include "vulkan_window.hpp"
#include "vulkan_shaders.hpp"
#include "assets.hpp"
#include "game.hpp"

namespace vk {

class VulkanRenderer;

struct ChunkRenderer {
	static constexpr uint64_t ALLOC_SIZE = 64 * (1024ull * 1024); // size of vram allocations
	static constexpr int SLICES_PER_ALLOC = (int)(ALLOC_SIZE / CHUNK_SLICE_BYTESIZE);

	enum DrawType { DT_OPAQUE=0, DT_TRANSPARENT=1 };
	
	struct AllocBlock {
		Allocation		mesh_data;

		struct DrawList {
			struct DrawSlice {
				uint16_t	vertex_count;
				uint16_t	slice_idx; // in alloc (not global slice id)
				chunk_id	chunk;
			};
			int				count;
			DrawSlice		slices[SLICES_PER_ALLOC];
		};

		DrawList		draw_lists[2];
	};

	struct FrameData {
		int slices_end;
	};

	std_vector<AllocBlock>	allocs;
	std_vector<FrameData>	frames;

	VkPipelineLayout		pipeline_layout;
	Pipeline*				opaque_pipeline;
	Pipeline*				transparent_pipeline;

	int						drawcount_opaque = 0;
	int						drawcount_transparent = 0;

	void imgui (Chunks& chunks) {
		
		size_t vertices = 0;
		size_t slices_total = 0;
		for (chunk_id cid=0; cid<chunks.end(); ++cid) {
			if (chunks[cid].flags == 0) continue;
			
			vertices += chunks[cid].opaque_mesh_vertex_count;
			vertices += chunks[cid].transp_mesh_vertex_count;

			slices_total += _slices_count(chunks[cid].opaque_mesh_vertex_count);
			slices_total += _slices_count(chunks[cid].transp_mesh_vertex_count);
		}
		
		ImGui::Separator();

		ImGui::Text("Drawcalls: opaque: %3d  transparent: %3d (%3d / %3d slices - %3.0f%%)",
			drawcount_opaque, drawcount_transparent, drawcount_opaque + drawcount_transparent,
			slices_total, (float)(drawcount_opaque + drawcount_transparent) / slices_total * 100);
		
		ImGui::Text("Mesh allocs: %2d  slices: %5d  vertices: %12s",
			allocs.size(), slices_total, format_thousands(vertices).c_str());
		ImGui::Text("Mesh VRAM: used: %7.3f MB  commited: %7.3f MB (%6.2f%% usage)",
			(float)(vertices * sizeof(BlockMeshInstance)) / 1024 / 1024,
			(float)(allocs.size() * ALLOC_SIZE) / 1024 / 1024,
			(float)(vertices * sizeof(BlockMeshInstance)) / (float)(allocs.size() * ALLOC_SIZE) * 100);
		
		if (ImGui::TreeNode("slices alloc")) {
			print_bitset_allocator(chunks.slices.slots, CHUNK_SLICE_BYTESIZE, ALLOC_SIZE);
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

	void create (VulkanWindowContext& ctx, PipelineManager& pipelines, VkRenderPass main_renderpass, VkDescriptorSetLayout common, int frames_in_flight) {
		frames.resize(frames_in_flight);

		pipeline_layout = create_pipeline_layout(ctx.dev, { common }, {{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float3) }});
		GPU_DBG_NAME(ctx, pipeline_layout, "ChunkRenderer.pipeline_layout");

		auto attribs = make_attribs<BlockMeshInstance>();

		PipelineOptions opt;
		opt.alpha_blend = false;
		opt.depth_test = true;
		opt.color_attachments = 2;
		{
			auto cfg = PipelineConfig("vk/chunks", pipeline_layout, main_renderpass, 0, opt, attribs, {{"ALPHA_TEST", "1"}});
			opaque_pipeline = pipelines.create_pipeline(ctx, "ChunkRenderer.opaque_pipeline", cfg);
		}
		{
			opt.alpha_blend = true;
			auto cfg = PipelineConfig("vk/chunks", pipeline_layout, main_renderpass, 0, opt, attribs, {{"ALPHA_TEST", "0"}});
			transparent_pipeline = pipelines.create_pipeline(ctx, "ChunkRenderer.transparent_pipeline", cfg);
		}
	}

	void destroy (VkDevice dev) {
		for (auto& a : allocs)
			free_alloc(dev, a);

		vkDestroyPipelineLayout(dev, pipeline_layout, nullptr);
	}

	void upload_remeshed (VulkanRenderer& r, Chunks& chunks, VkCommandBuffer cmds, int cur_frame);

	void draw_chunks (VulkanWindowContext& ctx, VkCommandBuffer cmds, Game& game, int cur_frame);

};

} // namespace vk
