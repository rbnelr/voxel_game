#pragma once
#include "vulkan_helper.hpp"
#include "vulkan_window.hpp"
#include "vulkan_shaders.hpp"

namespace vk {

inline constexpr size_t BUFFER_SIZE = 4 * 1024ull * 1024ull;

struct DebugDrawer {

	struct Buffer {
		Allocation	buf;
		void*		mapped_ptr;
	};

	struct FrameData {
		std_vector<Buffer> bufs;
	};
	std_vector<FrameData>	frames;

	VkPipelineLayout pipeline_layout;
	Pipeline* lines_pipeline;
	Pipeline* tris_pipeline;

	Buffer new_buffer (VulkanWindowContext& ctx, int cur_frame) {
		ZoneScopedC(tracy::Color::Crimson);

		auto buf = allocate_buffer(ctx.dev, ctx.pdev, BUFFER_SIZE,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		GPU_DBG_NAMEf(ctx, buf.buf, "DebugDrawer.frames[%d].buf", cur_frame);
		GPU_DBG_NAMEf(ctx, buf.mem, "DebugDrawer.frames[%d].buf_mem", cur_frame);

		void* ptr;
		vkMapMemory(ctx.dev, buf.mem, 0, BUFFER_SIZE, 0, &ptr);

		return { buf, ptr };
	}
	void free_buffer (VkDevice dev, Buffer& buf) {
		ZoneScopedC(tracy::Color::Crimson);
		vkUnmapMemory(dev, buf.buf.mem);
		buf.buf.free(dev);
	}

	void create (VulkanWindowContext& ctx, PipelineManager& pipelines, VkRenderPass main_renderpass, VkDescriptorSetLayout common, int frames_in_flight) {
		frames.resize(frames_in_flight);

		// NOTE:  // setting PC to { VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int) } (same as chunk_renderer.pipeline_layout) would allows us to not re-bind set 0 for these calls
		pipeline_layout = create_pipeline_layout(ctx.dev, { common }, {});
		GPU_DBG_NAME(ctx, pipeline_layout, "DebugDrawer.pipeline_layout");

		PipelineOptions opt;
		opt.alpha_blend = true;
		opt.depth_test = true;
		opt.no_depth_write = true;
		opt.primitive_mode = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		auto cfg = PipelineConfig("debug_lines", pipeline_layout, main_renderpass, 0, opt, make_attribs<DebugDraw::LineVertex>());
		lines_pipeline = pipelines.create_pipeline(ctx, "DebugDrawer.lines_pipeline", cfg);

		PipelineOptions opt2;
		opt2.alpha_blend = true;
		opt2.depth_test = true;
		cfg = PipelineConfig("debug_tris", pipeline_layout, main_renderpass, 0, opt2, make_attribs<DebugDraw::TriVertex>());
		tris_pipeline = pipelines.create_pipeline(ctx, "DebugDrawer.tris_pipeline", cfg);
	}
	void destroy (VkDevice dev) {
		for (auto& f : frames) {
			for (auto& buf : f.bufs)
				free_buffer(dev, buf);
		}

		vkDestroyPipelineLayout(dev, pipeline_layout, nullptr);
	}

	void draw (VulkanWindowContext& ctx, VkCommandBuffer cmds, int cur_frame) {
		int min_bufs = 2; // one for each lines and tris
		
		auto& frame = frames[cur_frame];

		int bufi = 0;

		{ //// Debug Lines
			GPU_TRACE(ctx, cmds, "draw debug lines");
			
			vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, lines_pipeline->pipeline);
		
			static constexpr int VERTS_PER_BUF = BUFFER_SIZE / sizeof(DebugDraw::LineVertex) /2 *2; // /2 *2 to round down to multiple of 2 as to not straddle across buffers
			int buf_count = (int)( (g_debugdraw.lines.size() + VERTS_PER_BUF-1) / VERTS_PER_BUF );

			for (int i=0; i<buf_count; ++i) {
				if (bufi >= (int)frame.bufs.size())
					frame.bufs.push_back( new_buffer(ctx, cur_frame) );

				size_t first_vertex = i * VERTS_PER_BUF;
				size_t vertex_count = std::min(g_debugdraw.lines.size() - first_vertex, (size_t)VERTS_PER_BUF);

				memcpy(frame.bufs[bufi].mapped_ptr, &g_debugdraw.lines[first_vertex], vertex_count * sizeof(DebugDraw::LineVertex));

				VkDeviceSize offs = 0;
				vkCmdBindVertexBuffers(cmds, 0, 1, &frame.bufs[bufi].buf.buf, &offs);

				vkCmdDraw(cmds, (uint32_t)vertex_count, 1, 0, 0);

				bufi++;
			}
		}

		{ //// Debug tris
			GPU_TRACE(ctx, cmds, "draw debug tris");

			vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, tris_pipeline->pipeline);
		
			static constexpr int VERTS_PER_BUF = BUFFER_SIZE / sizeof(DebugDraw::TriVertex) /3 *3; // /3 *3 to round down to multiple of 3 as to not straddle across buffers
			int buf_count = (int)( (g_debugdraw.tris.size() + VERTS_PER_BUF-1) / VERTS_PER_BUF );

			for (int i=0; i<buf_count; ++i) {
				if (bufi >= (int)frame.bufs.size())
					frame.bufs.push_back( new_buffer(ctx, cur_frame) );
		
				size_t first_vertex = i * VERTS_PER_BUF;
				size_t vertex_count = std::min(g_debugdraw.tris.size() - first_vertex, (size_t)VERTS_PER_BUF);
		
				memcpy(frame.bufs[bufi].mapped_ptr, &g_debugdraw.tris[first_vertex], vertex_count * sizeof(DebugDraw::TriVertex));
		
				VkDeviceSize offs = 0;
				vkCmdBindVertexBuffers(cmds, 0, 1, &frame.bufs[bufi].buf.buf, &offs);
		
				vkCmdDraw(cmds, (uint32_t)vertex_count, 1, 0, 0);
		
				bufi++;
			}
		}

		while ((int)frame.bufs.size() > std::max(bufi, min_bufs)) {
			free_buffer(ctx.dev, frame.bufs.back());
			frame.bufs.pop_back();
		}
	}
};

} // namespace vk
