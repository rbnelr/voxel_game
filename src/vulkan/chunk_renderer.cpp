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
			remesh_jobs.push_back(std::make_unique<RemeshChunkJob>(&data.chunks[id], data.chunks, r.assets, data.wg, r));
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
		auto slice_id = chunks.slices_alloc.alloc();
		chunk->mesh_slices.push_back(slice_id);

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

	renderer.chunk_renderer.upload_slices(chunks, chunk, mesh.opaque_vertices, renderer);

	chunk->flags &= ~Chunk::REMESH;
}

void ChunkRenderer::upload_remeshed (VulkanWindowContext& ctx, int cur_frame, VkCommandBuffer cmds) {
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
<<<<<<< HEAD
		for (auto& upload : uploads) {
			
			auto* vertices = upload.data->verts;
			size_t size = sizeof(BlockMeshInstance) * upload.vertex_count;
=======
		for (; slicei < (int)uploads.size(); ++buf) {

			// alloc new staging buffer as required
			if (buf >= (int)frame.staging_bufs.size())
				frame.staging_bufs.push_back( new_staging_buffer(ctx, cur_frame) );
			auto& staging_buf = frame.staging_bufs[buf];

			char* ptr;
			vkMapMemory(ctx.dev, staging_buf.mem, 0, ALLOC_SIZE, 0, (void**)&ptr);

			size_t offs = 0;
>>>>>>> e23da88abbef911d650508a418b94af69ab8ad7e

			if (used_sbufs == 0 || offs_in_sbuf + size > ALLOC_SIZE) {
				// staging buffer would overflow, switch to next one
				++used_sbufs;
				offs_in_sbuf = 0;

				// alloc new staging buffer as required
				if (used_sbufs > (int)frame.staging_bufs.size())
					frame.staging_bufs.push_back( new_staging_buffer(ctx, cur_frame) );
			}
			auto& staging_buf = frame.staging_bufs[used_sbufs-1];

			memcpy((char*)staging_buf.mapped_ptr + offs_in_sbuf, vertices, size);

			MeshData::free_slice(upload.data);

			if (upload.slice_id >= (uint32_t)allocs.size() * SLICES_PER_ALLOC)
				allocs.push_back( new_alloc(ctx) );

<<<<<<< HEAD
			uint32_t dst_offset;
			VkBuffer dst_buf = calc_slice_buf(upload.slice_id, &dst_offset);
=======
				if (upload.slice_id >= (uint32_t)allocs.size() * SLICES_PER_ALLOC)
					allocs.push_back( new_alloc(ctx) );
>>>>>>> e23da88abbef911d650508a418b94af69ab8ad7e

			VkBufferCopy copy_region = {};
			copy_region.srcOffset = offs_in_sbuf;
			copy_region.dstOffset = dst_offset * sizeof(BlockMeshInstance);
			copy_region.size = size;
			vkCmdCopyBuffer(cmds, staging_buf.buf.buf, dst_buf, 1, &copy_region);

			offs_in_sbuf += size;
		}

		// free staging buffers when no longer needed
		int min_staging_bufs = 0;
		while ((int)frame.staging_bufs.size() > max(used_sbufs, min_staging_bufs)) {
			free_staging_buffer(ctx.dev, frame.staging_bufs.back());
			frame.staging_bufs.pop_back();
		}

<<<<<<< HEAD
		if (uploads.size() > 0) {
			VkMemoryBarrier mem = {};
			mem.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
			mem.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
			mem.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
			vkCmdPipelineBarrier(cmds,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				0, 1, &mem, 0, nullptr, 0, nullptr);
=======
			vkUnmapMemory(ctx.dev, staging_buf.mem);
		}

		if (uploads.size() > 0) {
			vkCmdPipelineBarrier(cmds,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				0, 0, nullptr, 0, nullptr, 0, nullptr);
>>>>>>> e23da88abbef911d650508a418b94af69ab8ad7e
		}

		uploads.clear();
	}
}

void ChunkRenderer::draw_chunks (VkCommandBuffer cmds, Chunks& chunks, VkPipeline pipeline, VkPipelineLayout layout) {
	ZoneScoped;
	
	vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	for (chunk_id cid=0; cid<chunks.max_id; ++cid) {
		auto& chunk = chunks[cid];
		if ((chunk.flags & Chunk::LOADED) == 0) continue;

		for (auto& sid : chunk.mesh_slices) {
			uint32_t offset;
			VkBuffer buf = calc_slice_buf(sid, &offset);

			VkBuffer vertex_bufs[] = { buf };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(cmds, 0, 1, vertex_bufs, offsets);

			float3 chunk_pos = (float3)(chunk.pos * CHUNK_SIZE);

			vkCmdPushConstants(cmds, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float3), &chunk_pos);
			
			assert(slices[sid].vertex_count > 0);

			vkCmdDraw(cmds,
				BlockMeshes::MERGE_INSTANCE_FACTOR, // repeat MERGE_INSTANCE_FACTOR vertices
				slices[sid].vertex_count, // for each instance in the mesh
				0, offset);
		}
	}
}

} // namespace vk
