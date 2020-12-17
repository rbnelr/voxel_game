#pragma once
#include "common.hpp"
#include "chunk_renderer.hpp"
#include "vulkan_renderer.hpp"

namespace vk {

void ChunkRenderer::queue_remeshing (Renderer& r, RenderData& data) {
	ZoneScoped;

	std::vector<RemeshChunkJob> remesh_jobs;

	{
		ZoneScopedN("chunks_to_remesh iterate all chunks");
		auto should_remesh = Chunk::REMESH|Chunk::LOADED|Chunk::ALLOCATED;
		for (chunk_id id = 0; id < data.chunks.max_id; ++id) {
			data.chunks[id]._validate_flags();
			if ((data.chunks[id].flags & should_remesh) != should_remesh) continue;
			
			RemeshChunkJob job;
			job.chunk = &data.chunks[id];
			job.chunks = &data.chunks;
			job.assets = &r.assets;
			job.wg = &data.wg;
			remesh_jobs.emplace_back(std::move(job));
		}
	}

	// remesh all chunks in parallel
	parallelism_threadpool.jobs.push_n(remesh_jobs.data(), remesh_jobs.size());

	remesh_chunks_count = remesh_jobs.size();
}

void ChunkRenderer::upload_slices (Chunks& chunks, slice_id* chunk_slices, MeshData& mesh, Renderer& r) {
	ZoneScoped;

	slice_id prev_id = U16_NULL; // use index with null for first case instead of ptr to slice_id since ptrs get invalidated by alloc_slice
	slice_id slice_id = *chunk_slices;

	// overwrite slices with new vertex counts (and allocate new slices when needed)
	for (int slice = 0; slice < mesh.used_slices; ++slice) {
		assert(prev_id == U16_NULL || chunks.slices[prev_id].next == slice_id);
		
		if (slice_id == U16_NULL) {
			slice_id = chunks.alloc_slice();
			*(prev_id != U16_NULL ? &chunks.slices[prev_id].next : chunk_slices) = slice_id;
		}

		auto count = mesh.get_vertex_count(slice);

		uploads.push_back({ slice_id, mesh.slices[slice], count });
		mesh.slices[slice] = nullptr;

		chunks.slices[slice_id].vertex_count = count;

		prev_id = slice_id;
		slice_id = chunks.slices[prev_id].next;
	}

	// free potentially remaining slices no longer needed
	*(prev_id != U16_NULL ? &chunks.slices[prev_id].next : chunk_slices) = slice_id;
	chunks.free_slices(slice_id);
}

void ChunkRenderer::upload_remeshed (VulkanWindowContext& ctx, Renderer& r, VkCommandBuffer cmds, Chunks& chunks, int cur_frame) {
	ZoneScoped;

	for (size_t result_count = 0; result_count < remesh_chunks_count; ) {
		RemeshChunkJob results[64];
		size_t count = parallelism_threadpool.results.pop_n_wait(results, 1, ARRLEN(results));

		for (size_t i = 0; i < count; ++i) {
			auto& res = results[i];

			res.mesh.opaque_vertices.free_preallocated();
			res.mesh.tranparent_vertices.free_preallocated();

			upload_slices(chunks, &res.chunk->opaque_slices, res.mesh.opaque_vertices, r);
			upload_slices(chunks, &res.chunk->transparent_slices, res.mesh.tranparent_vertices, r);

			res.chunk->flags &= ~Chunk::REMESH;
		}

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
			int min_staging_bufs = 0;
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

		while (frame.indirect_draw.size() > allocs.size()) {
			free_indirect_draw_buffer(ctx.dev, frame.indirect_draw.back());
			frame.indirect_draw.pop_back();
		}
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

		slice_id slice = chunks[cid].opaque_slices;
		while (slice != U16_NULL) {
			assert(chunks.slices[slice].vertex_count > 0);

			uint32_t alloci =  slice / SLICES_PER_ALLOC;
			uint32_t slicei = (slice % SLICES_PER_ALLOC);

			VkDrawIndirectCommand draw_data;
			draw_data.vertexCount = BlockMeshes::MERGE_INSTANCE_FACTOR; // repeat MERGE_INSTANCE_FACTOR vertices
			draw_data.instanceCount = chunks.slices[slice].vertex_count; // for each instance in the mesh
			draw_data.firstVertex = 0;
			draw_data.firstInstance = slicei * CHUNK_SLICE_LENGTH;

			auto& draw = frame.indirect_draw[alloci];

			draw.draw_data_ptr[draw.opaque_draw_count] = draw_data;
			draw.per_draw_ubo_ptr[draw.opaque_draw_count] = { float4(chunk_pos, 1.0f) };

			draw.opaque_draw_count++;

			slice = chunks.slices[slice].next;
		}
	}

	// Transparent draws
	for (chunk_id cid=0; cid<chunks.max_id; ++cid) {
		if ((chunks[cid].flags & Chunk::LOADED) == 0) continue;
	
		float3 chunk_pos = (float3)(chunks[cid].pos * CHUNK_SIZE);
	
		slice_id slice = chunks[cid].transparent_slices;
		while (slice != U16_NULL) {
			assert(chunks.slices[slice].vertex_count > 0);
	
			uint32_t alloci =  slice / SLICES_PER_ALLOC;
			uint32_t slicei = (slice % SLICES_PER_ALLOC);
	
			VkDrawIndirectCommand draw_data;
			draw_data.vertexCount = BlockMeshes::MERGE_INSTANCE_FACTOR; // repeat MERGE_INSTANCE_FACTOR vertices
			draw_data.instanceCount = chunks.slices[slice].vertex_count; // for each instance in the mesh
			draw_data.firstVertex = 0;
			draw_data.firstInstance = slicei * CHUNK_SLICE_LENGTH;
	
			auto& draw = frame.indirect_draw[alloci];
	
			draw.draw_data_ptr[draw.opaque_draw_count + draw.transparent_draw_count] = draw_data;
			draw.per_draw_ubo_ptr[draw.opaque_draw_count + draw.transparent_draw_count] = { float4(chunk_pos, 1.0f) };
	
			draw.transparent_draw_count++;

			slice = chunks.slices[slice].next;
		}
	}

	// Opaque draws
	vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, opaque_pipeline->pipeline);

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
	vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, transparent_pipeline->pipeline);

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

void ChunkRenderer::create (VulkanWindowContext& ctx, PipelineManager& pipelines, VkRenderPass main_renderpass, VkDescriptorSetLayout common, int frames_in_flight) {
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

	auto attribs = make_attribs<BlockMeshInstance>();

	PipelineOptions opt;
	opt.alpha_blend = false;
	opt.depth_test = true;
	auto cfg = PipelineConfig("chunks", pipeline_layout, main_renderpass, 0, opt, attribs, {{"ALPHA_TEST", "1"}});
	opaque_pipeline = pipelines.create_pipeline(ctx, "ChunkRenderer.opaque_pipeline", cfg);

	opt.alpha_blend = true;
	cfg = PipelineConfig("chunks", pipeline_layout, main_renderpass, 0, opt, attribs, {{"ALPHA_TEST", "0"}});
	transparent_pipeline = pipelines.create_pipeline(ctx, "ChunkRenderer.transparent_pipeline", cfg);
}

void RemeshChunkJob::execute () {
	mesh_chunk(*assets, *wg, *chunks, chunk, &mesh);
}

} // namespace vk
