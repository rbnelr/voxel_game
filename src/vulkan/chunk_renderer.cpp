#pragma once
#include "common.hpp"
#include "chunk_renderer.hpp"
#include "vulkan_renderer.hpp"

namespace vk {

void ChunkRenderer::queue_remeshing (Renderer& r, RenderData& data) {
	ZoneScoped;

	std::vector<std::unique_ptr<ThreadingJob>> remesh_jobs;
	{
		ZoneScopedN("chunks_to_remesh iterate all chunks");
		auto should_remesh = Chunk::REMESH|Chunk::LOADED|Chunk::ALLOCATED;
		for (chunk_id id = 0; id < data.chunks.max_id; ++id) {
			data.chunks[id]._validate_flags();
			if ((data.chunks[id].flags & should_remesh) != should_remesh) continue;
			remesh_jobs.push_back(std::make_unique<RemeshChunkJob>(&data.chunks[id], data.chunks, r.assets, data.wg, r));
		}
	}

	// remesh all chunks in parallel
	parallelism_threadpool.jobs.push_n(remesh_jobs.data(), remesh_jobs.size());

	remesh_chunks_count = remesh_jobs.size();
}

void ChunkRenderer::upload_slices (Chunks& chunks, std::vector<uint16_t>& chunk_slices, MeshData& mesh, Renderer& r) {
	ZoneScoped;

	for (auto slice_id : chunk_slices)
		chunks.slices_alloc.free(slice_id);
	chunk_slices.clear();

	for (int slice = 0; slice < mesh.used_slices; ++slice) {
		auto slice_id = chunks.slices_alloc.alloc();
		chunk_slices.push_back(slice_id);

		auto count = mesh.get_vertex_count(slice);

		uploads.push_back({ slice_id, mesh.slices[slice], count });
		mesh.slices[slice] = nullptr;

		if (slice_id >= (int)slices.size())
			slices.resize(slice_id+1);

		slices[slice_id].vertex_count = count;
	}
}

void RemeshChunkJob::finalize () {
	mesh.opaque_vertices.free_preallocated();
	mesh.tranparent_vertices.free_preallocated();

	renderer.chunk_renderer.upload_slices(chunks, chunk->opaque_slices, mesh.opaque_vertices, renderer);
	renderer.chunk_renderer.upload_slices(chunks, chunk->transparent_slices, mesh.tranparent_vertices, renderer);

	chunk->flags &= ~Chunk::REMESH;
}

void ChunkRenderer::upload_remeshed (VulkanWindowContext& ctx, VkCommandBuffer cmds, Chunks& chunks, int cur_frame) {
	ZoneScoped;

	for (size_t result_count = 0; result_count < remesh_chunks_count; ) {
		std::unique_ptr<ThreadingJob> results[64];
		size_t count = parallelism_threadpool.results.pop_n_wait(results, 1, ARRLEN(results));

		for (size_t i = 0; i < count; ++i)
			results[i]->finalize();

		result_count += count;
	}

	ZoneValue(uploads.size());

	{ // upload data via mapped staging buffer and vkCmdCopyBuffer
		auto& frame = frames[cur_frame];

		int used_sbufs = 0; // how many staging buffers were needed this frame to upload
		size_t offs_in_sbuf = 0;

		// foreach staging buffer
		for (auto& upload : uploads) {
			
			size_t size = sizeof(BlockMeshInstance) * upload.vertex_count;

			if (used_sbufs == 0 || (offs_in_sbuf + size) > STAGING_SIZE) {
				// staging buffer would overflow, switch to next one
				++used_sbufs;
				offs_in_sbuf = 0;

				// alloc new staging buffer as required
				if (used_sbufs > (int)frame.staging_bufs.size())
					frame.staging_bufs.push_back( new_staging_buffer(ctx, cur_frame) );
			}
			auto& staging_buf = frame.staging_bufs[used_sbufs-1];

			size_t vert_data_offs = offs_in_sbuf;
			offs_in_sbuf += size;

			memcpy((char*)staging_buf.mapped_ptr + vert_data_offs, upload.data->verts, size);

			MeshData::free_slice(upload.data);

			if (upload.slice_id >= (uint32_t)allocs.size() * SLICES_PER_ALLOC)
				new_alloc(ctx);


			uint32_t alloci =  upload.slice_id / SLICES_PER_ALLOC;
			uint32_t slicei = (upload.slice_id % SLICES_PER_ALLOC);

			VkBufferCopy copy_region = {};
			copy_region.srcOffset = vert_data_offs;
			copy_region.dstOffset = slicei * CHUNK_SLICE_LENGTH * sizeof(BlockMeshInstance);
			copy_region.size = size;
			vkCmdCopyBuffer(cmds, staging_buf.buf.buf, allocs[alloci].mesh_data.buf, 1, &copy_region);
		}

		{ // free staging buffers when no longer needed
			int min_staging_bufs = 1;
			while ((int)frame.staging_bufs.size() > max(used_sbufs, min_staging_bufs)) {
				free_staging_buffer(ctx.dev, frame.staging_bufs.back());
				frame.staging_bufs.pop_back();
			}
		}

		{ // free allocation blocks if they are no longer needed by any of the frames in flight
			frame.slices_end = chunks.slices_alloc.alloc_end;
			int slices_end = 0;
			for (auto& f : frames)
				slices_end = max(slices_end, f.slices_end);

			while (slices_end < ((int)allocs.size()-1) * (int)SLICES_PER_ALLOC) {
				free_alloc(ctx.dev, allocs.back());
				allocs.pop_back();
			}
		}

		while (frame.indirect_draw.size() > allocs.size())
			free_indirect_draw_buffer(ctx.dev, frame.indirect_draw.back());
		while (frame.indirect_draw.size() < allocs.size())
			frame.indirect_draw.push_back( new_indirect_draw_buffer(ctx, cur_frame, (int)frame.indirect_draw.size()) );
		
		if (uploads.size() > 0) {
			VkMemoryBarrier mem = {};
			mem.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			mem.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			mem.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
			vkCmdPipelineBarrier(cmds,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				0, 1, &mem, 0, nullptr, 0, nullptr);
		}

		uploads.clear();
	}
}

void ChunkRenderer::draw_chunks (VulkanWindowContext& ctx, VkCommandBuffer cmds, Chunks& chunks, int cur_frame) {
	ZoneScoped;

	auto frame = frames[cur_frame];

	for (auto& draw : frame.indirect_draw) {
		draw.opaque_draw_count = 0;
		draw.transparent_draw_count = 0;
	}

	// Opaque draws
	for (chunk_id cid=0; cid<chunks.max_id; ++cid) {
		if ((chunks[cid].flags & Chunk::LOADED) == 0) continue;

		float3 chunk_pos = (float3)(chunks[cid].pos * CHUNK_SIZE);

		for (auto& sid : chunks[cid].opaque_slices) {
			assert(slices[sid].vertex_count > 0);

			uint32_t alloci =  sid / SLICES_PER_ALLOC;
			uint32_t slicei = (sid % SLICES_PER_ALLOC);

			VkDrawIndirectCommand draw_data;
			draw_data.vertexCount = BlockMeshes::MERGE_INSTANCE_FACTOR; // repeat MERGE_INSTANCE_FACTOR vertices
			draw_data.instanceCount = slices[sid].vertex_count; // for each instance in the mesh
			draw_data.firstVertex = 0;
			draw_data.firstInstance = slicei * CHUNK_SLICE_LENGTH;

			auto& draw = frame.indirect_draw[alloci];

			draw.draw_data_ptr[draw.opaque_draw_count] = draw_data;
			draw.per_draw_ubo_ptr[draw.opaque_draw_count] = { float4(chunk_pos, 1.0f) };

			draw.opaque_draw_count++;
		}
	}

	// Transparent draws
	for (chunk_id cid=0; cid<chunks.max_id; ++cid) {
		if ((chunks[cid].flags & Chunk::LOADED) == 0) continue;
	
		float3 chunk_pos = (float3)(chunks[cid].pos * CHUNK_SIZE);
	
		for (auto& sid : chunks[cid].transparent_slices) {
			assert(slices[sid].vertex_count > 0);
	
			uint32_t alloci =  sid / SLICES_PER_ALLOC;
			uint32_t slicei = (sid % SLICES_PER_ALLOC);
	
			VkDrawIndirectCommand draw_data;
			draw_data.vertexCount = BlockMeshes::MERGE_INSTANCE_FACTOR; // repeat MERGE_INSTANCE_FACTOR vertices
			draw_data.instanceCount = slices[sid].vertex_count; // for each instance in the mesh
			draw_data.firstVertex = 0;
			draw_data.firstInstance = slicei * CHUNK_SLICE_LENGTH;
	
			auto& draw = frame.indirect_draw[alloci];
	
			draw.draw_data_ptr[draw.opaque_draw_count + draw.transparent_draw_count] = draw_data;
			draw.per_draw_ubo_ptr[draw.opaque_draw_count + draw.transparent_draw_count] = { float4(chunk_pos, 1.0f) };
	
			draw.transparent_draw_count++;
		}
	}

	// Opaque draws
	vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, opaque_pipeline);

	for (int i=0; i<(int)frame.indirect_draw.size(); ++i) {
		auto& draw = frame.indirect_draw[i];
		if (draw.opaque_draw_count > 0) {
			GPU_TRACE(ctx, cmds, "draw alloc");

			VkBuffer vertex_bufs[] = { allocs[i].mesh_data.buf };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(cmds, 0, 1, vertex_bufs, offsets);

			// set 1
			vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 1, 1, &draw.descriptor_set, 0, nullptr);

			int offs = 0;
			vkCmdPushConstants(cmds, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int), &offs);

			vkCmdDrawIndirect(cmds, draw.draw_data.buf, 0, draw.opaque_draw_count, sizeof(VkDrawIndirectCommand));
		}
	}

	// Transparent draws
	vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, transparent_pipeline);

	for (int i=0; i<(int)frame.indirect_draw.size(); ++i) {
		auto& draw = frame.indirect_draw[i];
		if (draw.transparent_draw_count > 0) {
			GPU_TRACE(ctx, cmds, "draw alloc");
	
			VkBuffer vertex_bufs[] = { allocs[i].mesh_data.buf };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(cmds, 0, 1, vertex_bufs, offsets);
	
			// set 1
			vkCmdBindDescriptorSets(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 1, 1, &draw.descriptor_set, 0, nullptr);
			
			int offs = draw.opaque_draw_count;
			vkCmdPushConstants(cmds, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int), &offs);

			vkCmdDrawIndirect(cmds, draw.draw_data.buf, draw.opaque_draw_count * sizeof(VkDrawIndirectCommand),
				draw.transparent_draw_count, sizeof(VkDrawIndirectCommand));
		}
	}
}

VkPipeline create_pipeline (VkDevice dev, Shader* shader, VkRenderPass renderpass, VkPipelineLayout layout, int msaa, VertexAttributes attribs,
		bool blendEnable) {
	if (!shader->valid)
		return VK_NULL_HANDLE;

	VkPipelineShaderStageCreateInfo shader_stages[16] = {};

	for (int i=0; i<(int)shader->stages.size(); ++i) {
		shader_stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stages[i].stage = SHADERC_STAGE_BITS_MAP[ shader->stages[i].stage ];
		shader_stages[i].module = shader->stages[i].module;
		shader_stages[i].pName = "main";
	}

	VkPipelineVertexInputStateCreateInfo vertex_input = {};
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input.vertexBindingDescriptionCount = 1;
	vertex_input.pVertexBindingDescriptions = &attribs.descr;
	vertex_input.vertexAttributeDescriptionCount = (uint32_t)attribs.attribs.size();
	vertex_input.pVertexAttributeDescriptions = attribs.attribs.data();

	VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly.primitiveRestartEnable = VK_FALSE;

	// fake viewport and scissor, actually set dynamically
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width  = (float)1920;//cur_size.x;
	viewport.height = (float)1080;//cur_size.y;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = { 1920, 1080 };//swap_chain.extent;

	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = (VkSampleCountFlagBits)msaa;
	multisampling.minSampleShading = 1.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_TRUE;
	depth_stencil.depthWriteEnable = VK_TRUE;
	depth_stencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; // use reverse depth
	depth_stencil.depthBoundsTestEnable = VK_FALSE;
	depth_stencil.minDepthBounds = 0.0f;
	depth_stencil.maxDepthBounds = 1.0f;
	depth_stencil.stencilTestEnable = VK_FALSE;
	depth_stencil.front = {};
	depth_stencil.back = {};

	VkPipelineColorBlendAttachmentState color_blend_attachment = {};
	color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	color_blend_attachment.blendEnable = blendEnable;

	if (blendEnable) {
		color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	} else {
		color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}

	VkPipelineColorBlendStateCreateInfo color_blending = {};
	color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blending.logicOpEnable = VK_FALSE;
	color_blending.logicOp = VK_LOGIC_OP_COPY;
	color_blending.attachmentCount = 1;
	color_blending.pAttachments = &color_blend_attachment;
	color_blending.blendConstants[0] = 0.0f;
	color_blending.blendConstants[1] = 0.0f;
	color_blending.blendConstants[2] = 0.0f;
	color_blending.blendConstants[3] = 0.0f;

	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dynamic_state = {};
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = ARRLEN(dynamic_states);
	dynamic_state.pDynamicStates = dynamic_states;

	VkGraphicsPipelineCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.stageCount				= (uint32_t)shader->stages.size();
	info.pStages				= shader_stages;
	info.pVertexInputState		= &vertex_input;
	info.pInputAssemblyState	= &input_assembly;
	info.pViewportState			= &viewport_state;
	info.pRasterizationState	= &rasterizer;
	info.pMultisampleState		= &multisampling;
	info.pDepthStencilState		= &depth_stencil;
	info.pColorBlendState		= &color_blending;
	info.pDynamicState			= &dynamic_state;
	info.layout					= layout;
	info.renderPass				= renderpass;
	info.subpass				= 0;
	info.basePipelineHandle		= VK_NULL_HANDLE;
	info.basePipelineIndex		= -1;

	VkPipeline pipeline = VK_NULL_HANDLE;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline));
	return pipeline;
}

void ChunkRenderer::create (VulkanWindowContext& ctx, ShaderManager& shaders, VkRenderPass main_renderpass, VkDescriptorSetLayout common, int frames_in_flight) {
	frames.resize(frames_in_flight);

	{ // descriptor_pool
		VkDescriptorPoolSize pool_sizes[1] = {};
		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		pool_sizes[0].descriptorCount = MAX_ALLOCS * frames_in_flight; // for per draw data ubo

		VkDescriptorPoolCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		info.poolSizeCount = ARRLEN(pool_sizes);
		info.pPoolSizes = pool_sizes;
		info.maxSets = (uint32_t)(MAX_ALLOCS * frames_in_flight); // for per draw data ubo

		VK_CHECK_RESULT(vkCreateDescriptorPool(ctx.dev, &info, nullptr, &descriptor_pool));
		GPU_DBG_NAME(ctx, descriptor_pool, "ChunkRenderer.descriptor_pool");
	}

	{ // descriptor_layout
		VkDescriptorSetLayoutBinding bindings[1] = {};
		bindings[0].binding = 0;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[0].descriptorCount = 1;
		bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.bindingCount = ARRLEN(bindings);
		info.pBindings = bindings;

		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(ctx.dev, &info, nullptr, &descriptor_layout));
		GPU_DBG_NAME(ctx, descriptor_layout, "ChunkRenderer.descriptor_layout");
	}

	pipeline_layout = create_pipeline_layout(ctx.dev,
		{ common, descriptor_layout }, {{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int) }});
	GPU_DBG_NAME(ctx, pipeline_layout, "ChunkRenderer.pipeline_layout");

	opaque_pipeline      = create_pipeline(ctx.dev, shaders.get(ctx.dev, "chunks", {{"ALPHA_TEST", "1"}}),
		main_renderpass, pipeline_layout, 1, make_attribs<BlockMeshInstance>(), false);
	GPU_DBG_NAME(ctx, opaque_pipeline, "ChunkRenderer.opaque_pipeline");

	transparent_pipeline = create_pipeline(ctx.dev, shaders.get(ctx.dev, "chunks", {{"ALPHA_TEST", "0"}}),
		main_renderpass, pipeline_layout, 1, make_attribs<BlockMeshInstance>(), true);
	GPU_DBG_NAME(ctx, transparent_pipeline, "ChunkRenderer.transparent_pipeline");
}

} // namespace vk
