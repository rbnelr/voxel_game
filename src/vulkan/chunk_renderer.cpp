#pragma once
#include "common.hpp"
#include "chunk_renderer.hpp"
#include "vulkan_renderer.hpp"
#include "world.hpp"
#include "chunk_mesher.hpp"

namespace vk {

void ChunkRenderer::upload_remeshed (VulkanRenderer& r, Chunks& chunks, VkCommandBuffer cmds, int cur_frame) {
	ZoneScoped;

	auto& frame = frames[cur_frame];

	for (auto& slice : chunks.upload_slices) {
		uint32_t alloci = slice.sliceid / (uint32_t)SLICES_PER_ALLOC;
		uint32_t slicei = slice.sliceid % (uint32_t)SLICES_PER_ALLOC;

		while (alloci >= (uint32_t)allocs.size()) {
			new_alloc(r.ctx);
		}

		size_t unpadded_size = CHUNK_SLICE_LENGTH * sizeof(BlockMeshInstance); // size of slice data without padding

		r.staging.staged_copy(r.ctx, cmds, cur_frame,
			slice.data->verts, unpadded_size,
			allocs[alloci].mesh_data.buf, slicei * unpadded_size); // important to use unpadded_size here, can't use vertex-sized offsets when rendering

		ChunkMeshData::free_slice(slice.data);
	}

	chunks.upload_slices.clear();
	chunks.upload_slices.shrink_to_fit();

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

	if (chunks.upload_slices.size() > 0) {
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

void ChunkRenderer::draw_chunks (VulkanWindowContext& ctx, VkCommandBuffer cmds, Game& game, int cur_frame) {
	ZoneScoped;

	auto& frame = frames[cur_frame];
	auto& chunks = game.world->chunks;

	auto& cull_view = chunks.debug_frustrum_culling ? game.player_view : game.view;
	
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

		lrgba cols[] = {
			srgba(0, 0, 255, 255),
			srgba(0, 0, 200, 20),
			srgba(255, 0, 0, 180),
		};

		if (chunks.debug_frustrum_culling)
			g_debugdraw.wire_frustrum(cull_view, srgba(141,41,234));

		for (chunk_id cid=0; cid<chunks.max_id; ++cid) {
			auto& chunk = chunks[cid];
			if ((chunk.flags & Chunk::LOADED) == 0) continue;

			bool empty = chunk.opaque_mesh.vertex_count == 0 && chunk.transparent_mesh.vertex_count == 0;

			float3 lo = (float3)(chunk.pos * CHUNK_SIZE);
			float3 hi = (float3)((chunk.pos + 1) * CHUNK_SIZE);

			bool culled = false;
			if (!empty)
				culled = frustrum_cull_aabb(cull_view.frustrum, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z);
			
			if (chunks.debug_frustrum_culling) {
				if (!empty)
					g_debugdraw.wire_cube(((float3)chunks[cid].pos + 0.5f) * CHUNK_SIZE, (float3)CHUNK_SIZE * 0.997f, cols[culled ? 2 : 0]);
			} else if (chunks.visualize_chunks) {
				g_debugdraw.wire_cube(((float3)chunks[cid].pos + 0.5f) * CHUNK_SIZE, (float3)CHUNK_SIZE * 0.997f, cols[chunk.voxels.is_sparse() ? 1 : 0]);
			}

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
		VK_TRACE(ctx, cmds, "chunk draw opaque");
		draw_slices(opaque_pipeline->pipeline, DT_OPAQUE, drawcount_opaque);
	}
	{
		ZoneScopedN("chunk draw transparent");
		VK_TRACE(ctx, cmds, "chunk draw transparent");
		draw_slices(transparent_pipeline->pipeline, DT_TRANSPARENT, drawcount_transparent);
	}
}

} // namespace vk
