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
		for (chunk_id id = 0; id < data.chunks.max_id; ++id) {
			if ((data.chunks[id].flags & Chunk::REMESH) == 0) continue;
			remesh_jobs.push_back(std::make_unique<RemeshChunkJob>(&data.chunks[id], data.chunks, data.assets, data.wg, r));
		}
	}

	// remesh all chunks in parallel
	parallelism_threadpool.jobs.push_n(remesh_jobs.data(), remesh_jobs.size());

	remesh_chunks_count = remesh_jobs.size();
}

void ChunkRenderer::upload_slices (Chunks& chunks, Chunk* chunk, MeshData& mesh, Renderer& r) {
	ZoneScoped;

	for (auto slice_id : chunk->mesh_slices)
		chunks.slices_alloc.free(slice_id);
	chunk->mesh_slices.clear();

	for (int slice = 0; slice < mesh.used_slices; ++slice) {
		auto count = mesh.get_vertex_count(slice);

		auto slice_id = chunks.slices_alloc.alloc();
		chunk->mesh_slices.push_back(slice_id);

		uploads.push_back({ slice_id, mesh.slices[slice], count });
		mesh.slices[slice] = nullptr;

		slices.push_back({ count });
	}
}

void RemeshChunkJob::finalize () {
	mesh.opaque_vertices.free_preallocated();
	mesh.tranparent_vertices.free_preallocated();

	renderer.chunk_renderer.upload_slices(chunks, chunk, mesh.opaque_vertices, renderer);

	chunk->flags &= ~Chunk::REMESH;
}

void ChunkRenderer::upload_remeshed (VkDevice dev, VkPhysicalDevice pdev, int cur_frame, VkCommandBuffer cmds) {
	ZoneScoped;

	for (size_t result_count = 0; result_count < remesh_chunks_count; ) {
		std::unique_ptr<ThreadingJob> results[64];
		size_t count = parallelism_threadpool.results.pop_n_wait(results, 1, ARRLEN(results));

		for (size_t i = 0; i < count; ++i)
			results[i]->finalize();

		result_count += count;
	}

	{
		auto& frame = frames[cur_frame];

		int buf = 0; // staging buffer
		int slicei = 0;

		// for staging buffers
		for (; slicei < (int)uploads.size(); ++buf) {

			if (buf >= (int)frame.staging_bufs.size())
				frame.staging_bufs.push_back( new_staging_buffer(dev, pdev) );
			auto& staging_buf = frame.staging_bufs[buf];

			char* ptr;
			vkMapMemory(dev, staging_buf.mem, 0, ALLOC_SIZE, 0, (void**)&ptr);

			size_t offs = 0;

			// for staging buffers
			for (; slicei < (int)uploads.size(); ++slicei) {
				auto& upload = uploads[slicei];

				auto* vertices = upload.data->verts;
				size_t size = sizeof(ChunkVertex) * upload.vertex_count;

				if (offs + size > ALLOC_SIZE)
					break; // staging buffer would overflow, put this into next one

				memcpy(ptr + offs, vertices, size);

				MeshData::free_slice(upload.data);

				if (upload.slice_id >= (uint32_t)allocs.size() * SLICES_PER_ALLOC)
					allocs.push_back( new_alloc(dev, pdev) );

				uint32_t dst_offset;
				VkBuffer dst_buf = calc_slice_buf(upload.slice_id, &dst_offset);

				VkBufferCopy copy_region = {};
				copy_region.srcOffset = offs;
				copy_region.dstOffset = dst_offset * sizeof(ChunkVertex);
				copy_region.size = size;
				vkCmdCopyBuffer(cmds, staging_buf.buf, dst_buf, 1, &copy_region);

				offs += size;
			}

			vkUnmapMemory(dev, staging_buf.mem);
		}

		uploads.clear();
	}
}

void ChunkRenderer::draw_chunks (VkCommandBuffer cmds, Chunks& chunks, VkPipeline pipeline, VkPipelineLayout layout) {
	ZoneScoped;
	TracyVkZone(ctx.tracy_ctx, cmds, "draw chunks");

	vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	for (chunk_id cid=0; cid<chunks.max_id; ++cid) {
		if ((chunks[cid].flags & Chunk::LOADED) == 0) continue;

		for (auto& sid : chunks[cid].mesh_slices) {
			uint32_t offset;
			VkBuffer buf = calc_slice_buf(sid, &offset);

			VkBuffer vertex_bufs[] = { buf };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(cmds, 0, 1, vertex_bufs, offsets);

			float3 chunk_pos = (float3)(chunks[cid].pos * CHUNK_SIZE);

			vkCmdPushConstants(cmds, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float3), &chunk_pos);
			
			vkCmdDraw(cmds, slices[cid].vertex_count, 1, offset, 0);
		}
	}
}

} // namespace vk
