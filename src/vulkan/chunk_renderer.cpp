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

	if (stop)
		return;

	for (auto slice_id : chunk->mesh_slices)
		chunks.slices_alloc.free(slice_id);
	chunk->mesh_slices.clear();

	std::vector<UploadSlice> uploads;

	for (int slice = 0; slice < mesh.used_slices; ++slice) {
		auto count = mesh.get_vertex_count(slice);

		auto slice_id = chunks.slices_alloc.alloc();
		chunk->mesh_slices.push_back(slice_id);

		uploads.push_back({ mesh.slices[slice], count });
		mesh.slices[slice] = nullptr;

		slices.push_back({ count });

		stop = true;
		break;
	}

	if (uploads.size() > 0) {
		auto& frame = frames[r.cur_frame];
		auto& cmds = r.frame_data[r.cur_frame].command_buffer;

		auto* vertices = uploads[0].data->verts;
		size_t size = sizeof(vertices[0]) * uploads[0].vertex_count;

		void* ptr;
		vkMapMemory(r.ctx.dev, frame.staging_buf.mem, 0, ALLOC_SIZE, 0, &ptr);
		memcpy(ptr, vertices, size);
		vkUnmapMemory(r.ctx.dev, frame.staging_buf.mem);

		MeshData::free_slice(uploads[0].data);

		VkBufferCopy copy_region = {};
		copy_region.srcOffset = 0;
		copy_region.dstOffset = 0;
		copy_region.size = size;
		vkCmdCopyBuffer(cmds, frame.staging_buf.buf, allocs[0].buf, 1, &copy_region);
	}
}

void RemeshChunkJob::finalize () {
	mesh.opaque_vertices.free_preallocated();
	mesh.tranparent_vertices.free_preallocated();

	renderer.chunk_renderer.upload_slices(chunks, chunk, mesh.opaque_vertices, renderer);

	chunk->flags &= ~Chunk::REMESH;
}

void ChunkRenderer::upload_remeshed () {
	ZoneScoped;

	//stop = false;

	for (size_t result_count = 0; result_count < remesh_chunks_count; ) {
		std::unique_ptr<ThreadingJob> results[64];
		size_t count = parallelism_threadpool.results.pop_n_wait(results, 1, ARRLEN(results));

		for (size_t i = 0; i < count; ++i)
			results[i]->finalize();

		result_count += count;
	}
}

void ChunkRenderer::draw_chunks (VkCommandBuffer cmds) {
	ZoneScoped;

	for (uint32_t id=0; id < (uint32_t)slices.size(); ++id) {

		VkBuffer vertex_bufs[] = { allocs[0].buf };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(cmds, 0, 1, vertex_bufs, offsets);

		vkCmdDraw(cmds, slices[id].vertex_count, 1, 0, 0);
	}
}

} // namespace vk
