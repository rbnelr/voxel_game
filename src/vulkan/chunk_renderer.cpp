#pragma once
#include "common.hpp"
#include "chunk_renderer.hpp"
#include "vulkan_renderer.hpp"
#include "world.hpp"

namespace vk {

void ChunkRenderer::queue_remeshing (Renderer& r, RenderData& data) {
	ZoneScoped;

	std::vector<std::unique_ptr<RemeshChunkJob>> remesh_jobs;

	{
		ZoneScopedN("chunks_to_remesh iterate all chunks");
		auto should_remesh = Chunk::REMESH|Chunk::LOADED|Chunk::ALLOCATED;
		for (chunk_id id = 0; id < data.world.chunks.max_id; ++id) {
			data.world.chunks[id]._validate_flags();
			if ((data.world.chunks[id].flags & should_remesh) != should_remesh) continue;
			
			auto job = std::make_unique<RemeshChunkJob>(
				&data.world.chunks[id],
				data.world.chunks,
				r.assets,
				data.world.world_gen);
			remesh_jobs.emplace_back(std::move(job));
		}
	}

	// remesh all chunks in parallel
	parallelism_threadpool.jobs.push_n(remesh_jobs.data(), remesh_jobs.size());

	remesh_chunks_count = (int)remesh_jobs.size();
}

void ChunkRenderer::upload_remeshed (Renderer& r, Chunks& chunks, VkCommandBuffer cmds, int cur_frame) {
	ZoneScoped;

	auto& frame = frames[cur_frame];
	int upload_count = 0;

	// upload remeshed slices and register them in chunk mesh
	auto upload_slices = [&] (ChunkMeshData& remeshed, ChunkMesh& mesh) {
		ZoneScopedN("upload chunk slices");

		mesh.vertex_count = remeshed.vertex_count();
		uint32_t remain_vertices = mesh.vertex_count;

		int slice = 0;
		while (remain_vertices > 0) {
			if (mesh.slices[slice] == U16_NULL)
				mesh.slices[slice] = alloc_slice(r.ctx, chunks);

			uint32_t count = std::min(remain_vertices, (uint32_t)CHUNK_SLICE_LENGTH);

			uint32_t alloci = mesh.slices[slice] / (uint32_t)SLICES_PER_ALLOC;
			uint32_t slicei = mesh.slices[slice] % (uint32_t)SLICES_PER_ALLOC;

			r.staging.staged_copy(r.ctx, cmds, cur_frame,
				remeshed.slices[slice]->verts, count * sizeof(BlockMeshInstance),
				allocs[alloci].mesh_data.buf, slicei * CHUNK_SLICE_LENGTH * sizeof(BlockMeshInstance));
			
			ChunkMeshData::free_slice(remeshed.slices[slice]);

			remain_vertices -= count;

			upload_count++;
			slice++;
		}

		// free potentially remaining slices no longer needed
		for (; slice<MAX_CHUNK_SLICES; ++slice) {
			if (mesh.slices[slice] != U16_NULL)
				chunks.slices_alloc.free(mesh.slices[slice]);
			mesh.slices[slice] = U16_NULL;
		}
	};

	{
		ZoneScopedN("upload_slices");
		for (size_t result_count = 0; result_count < remesh_chunks_count; ) {
			std::unique_ptr<RemeshChunkJob> results[64];
			size_t count = parallelism_threadpool.results.pop_n_wait(results, 1, ARRLEN(results));

			for (size_t i = 0; i < count; ++i) {
				auto res = std::move(results[i]);

				upload_slices(res->mesh.opaque_vertices, res->chunk->opaque_mesh);
				upload_slices(res->mesh.tranparent_vertices, res->chunk->transparent_mesh);

				res->chunk->flags &= ~Chunk::REMESH;
			}

			result_count += count;
		}
	}

	{ // free allocation blocks if they are no longer needed by any of the frames in flight
		frame.slices_end = chunks.slices_alloc.alloc_end;

		int slices_end = 0;
		for (auto& f : frames)
			slices_end = max(slices_end, f.slices_end);

		while (slices_end < ((int)allocs.size()-1) * (int)SLICES_PER_ALLOC) {
			free_alloc(r.ctx.dev, allocs.back());
			allocs.pop_back();
		}
	}

	if (upload_count > 0) {
		VkMemoryBarrier mem = {};
		mem.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		mem.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
		mem.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		vkCmdPipelineBarrier(cmds,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			0, 1, &mem, 0, nullptr, 0, nullptr);
	}
}

void ChunkRenderer::draw_chunks (VulkanWindowContext& ctx, VkCommandBuffer cmds, RenderData& data, bool debug_frustrum_culling, int cur_frame) {
	ZoneScoped;

	auto& frame = frames[cur_frame];
	auto& chunks = data.world.chunks;

	auto& cull_view = debug_frustrum_culling ? data.player_view : data.view;
	
	{
		ZoneScopedN("chunk culling pass");

		for (auto& a : allocs) {
			a.draw_lists[0].count = 0;
			a.draw_lists[1].count = 0;
		}

		auto push_draw_slices = [this] (chunk_id cid, ChunkMesh& mesh, DrawType type) {
			uint32_t remain_count = mesh.vertex_count;
			int i = 0;
			while (remain_count > 0) {
				slice_id sid = mesh.slices[i++];

				uint16_t alloci = sid / (uint32_t)SLICES_PER_ALLOC;
				uint16_t slicei = sid % (uint32_t)SLICES_PER_ALLOC;

				auto& draw_list = allocs[alloci].draw_lists[type];

				auto draw_vertex_count = (uint16_t)std::min(remain_count, (uint32_t)CHUNK_SLICE_LENGTH);
				draw_list.slices[draw_list.count++] = { draw_vertex_count, slicei, cid };

				remain_count -= draw_vertex_count;
			}
		};

		auto col = srgba(0, 0, 255, 255);
		auto col2 = srgba(255, 0, 0, 180);

		if (debug_frustrum_culling)
			g_debugdraw.wire_frustrum(cull_view, srgba(141,41,234));

		for (chunk_id cid=0; cid<chunks.max_id; ++cid) {
			auto& chunk = chunks[cid];
			if ((chunk.flags & Chunk::LOADED) == 0) continue;

			if (chunk.opaque_mesh.vertex_count == 0 && chunk.transparent_mesh.vertex_count == 0)
				continue;

			float3 lo = (float3)(chunk.pos * CHUNK_SIZE);
			float3 hi = (float3)((chunk.pos + 1) * CHUNK_SIZE);

			bool culled = frustrum_cull_aabb(cull_view.frustrum, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z);
			
			if (debug_frustrum_culling)
				g_debugdraw.wire_cube(((float3)chunks[cid].pos + 0.5f) * CHUNK_SIZE, (float3)CHUNK_SIZE * 0.997f, culled ? col2 : col);

			if (culled) continue;

			push_draw_slices(cid, chunk.opaque_mesh, DT_OPAQUE);
			push_draw_slices(cid, chunk.transparent_mesh, DT_TRANSPARENT);
		}
	}

	auto draw_slices = [&] (VkPipeline pipeline, DrawType type, int& drawcount) {
		vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		drawcount = 0;

		for (auto& alloc : allocs) {
			auto& draw_list = alloc.draw_lists[type];
			if (draw_list.count > 0) {

				VkBuffer vertex_bufs[] = { alloc.mesh_data.buf };
				VkDeviceSize offsets[] = { 0 };
				vkCmdBindVertexBuffers(cmds, 0, 1, vertex_bufs, offsets);

				for (int i=0; i<draw_list.count; ++i) {
					auto& draw = draw_list.slices[i];

					float3 chunk_pos = (float3)(chunks.chunks[draw.chunk].pos * CHUNK_SIZE);
					vkCmdPushConstants(cmds, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float3), &chunk_pos);

					vkCmdDraw(cmds, BlockMeshes::MERGE_INSTANCE_FACTOR, draw.vertex_count, 0, draw.slice_idx * CHUNK_SLICE_LENGTH);
				}

				drawcount += draw_list.count;
			}
		}

		ZoneValue(drawcount);
	};

	{
		ZoneScopedN("chunk draw opaque");
		GPU_TRACE(ctx, cmds, "chunk draw opaque");
		draw_slices(opaque_pipeline->pipeline, DT_OPAQUE, drawcount_opaque);
	}
	{
		ZoneScopedN("chunk draw transparent");
		GPU_TRACE(ctx, cmds, "chunk draw transparent");
		draw_slices(transparent_pipeline->pipeline, DT_TRANSPARENT, drawcount_transparent);
	}
}

} // namespace vk
