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
		for (chunk_id id = 0; id < data.world.chunks.max_id; ++id) {
			data.world.chunks[id]._validate_flags();
			if ((data.world.chunks[id].flags & should_remesh) != should_remesh) continue;
			
			RemeshChunkJob job;
			job.chunk = &data.world.chunks[id];
			job.chunks = &data.world.chunks;
			job.assets = &r.assets;
			job.wg = &data.world.world_gen;
			remesh_jobs.emplace_back(std::move(job));
		}
	}

	// remesh all chunks in parallel
	parallelism_threadpool.jobs.push_n(remesh_jobs.data(), remesh_jobs.size());

	remesh_chunks_count = remesh_jobs.size();
}

void ChunkRenderer::upload_slices (Chunks& chunks, chunk_id chunkid, slice_id* chunk_slices, uint16_t type, MeshData& mesh, Renderer& r) {
	ZoneScoped;

	slice_id prev_id = U16_NULL; // use index with null for first case instead of ptr to slice_id since ptrs get invalidated by alloc_slice
	slice_id slice_id = *chunk_slices;

	// overwrite slices with new vertex counts (and allocate new slices when needed)
	for (int slice = 0; slice < mesh.used_slices; ++slice) {
		assert(prev_id == U16_NULL || chunks.slices[prev_id].next == slice_id);
		
		if (slice_id == U16_NULL) {
			slice_id = chunks.alloc_slice(chunkid, type);
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

			chunk_id cid = (chunk_id)(res.chunk - chunks.chunks);

			upload_slices(chunks, cid, &res.chunk->opaque_slices, 0, res.mesh.opaque_vertices, r);
			upload_slices(chunks, cid, &res.chunk->transparent_slices, 1, res.mesh.tranparent_vertices, r);

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

void ChunkRenderer::draw_chunks (VulkanWindowContext& ctx, VkCommandBuffer cmds, RenderData& data, bool debug_frustrum_culling, int cur_frame) {
	ZoneScoped;

	auto& frame = frames[cur_frame];
	auto& chunks = data.world.chunks;

	auto& cull_view = debug_frustrum_culling ? data.player_view : data.view;
	
	{
		ZoneScopedN("chunk culling pass");

		for (chunk_id cid=0; cid<chunks.max_id; ++cid) {
			auto& chunk = chunks[cid];
			if ((chunk.flags & Chunk::LOADED) == 0) continue;

			bool any_mesh = chunk.opaque_slices == U16_NULL && chunk.opaque_slices == U16_NULL;

			float3 chunk_pos = (float3)(chunk.pos * CHUNK_SIZE);
			bool culled = any_mesh || frustrum_cull_aabb(cull_view.frustrum,
				chunk_pos.x, chunk_pos.y, chunk_pos.z,
				chunk_pos.x + (float)CHUNK_SIZE, chunk_pos.y + (float)CHUNK_SIZE, chunk_pos.z + (float)CHUNK_SIZE);
			
			chunk.flags &= ~Chunk::CULLED;
			if (culled)
				chunk.flags |= Chunk::CULLED;
		}
	}

	if (debug_frustrum_culling) {
		auto col = srgba(0, 0, 255, 255);
		auto col2 = srgba(255, 0, 0, 180);
		
		g_debugdraw.wire_frustrum(cull_view, srgba(141,41,234));
		
		for (chunk_id cid=0; cid<chunks.max_id; ++cid) {
			if ((chunks[cid].flags & Chunk::LOADED) == 0) continue;
			bool culled = chunks[cid].flags & Chunk::CULLED;
			g_debugdraw.wire_cube(((float3)chunks[cid].pos + 0.5f) * CHUNK_SIZE, (float3)CHUNK_SIZE * 0.997f, culled ? col2 : col);
		}
	}

	drawcount_opaque = 0;

	{
		ZoneScopedN("chunk draw opaque");
		GPU_TRACE(ctx, cmds, "chunk draw opaque");

		vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, opaque_pipeline->pipeline);

		slice_id sid = 0;
		for (int alloci=0; alloci<(int)allocs.size(); ++alloci) {
			VkBuffer vertex_bufs[] = { allocs[alloci].mesh_data.buf };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(cmds, 0, 1, vertex_bufs, offsets);

			for (int slicei=0; slicei<SLICES_PER_ALLOC && sid < (slice_id)chunks.slices.size(); ++slicei, ++sid) {
				auto slice = chunks.slices[sid];

				if (slice.type == 0 && slice.vertex_count > 0) {
					auto& chunk = chunks.chunks[slice.chunkid];
					if ((chunk.flags & Chunk::CULLED) == 0) {
						float3 chunk_pos = (float3)(chunk.pos * CHUNK_SIZE);
						vkCmdPushConstants(cmds, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float3), &chunk_pos);

						vkCmdDraw(cmds, BlockMeshes::MERGE_INSTANCE_FACTOR, slice.vertex_count, 0, slicei * CHUNK_SLICE_LENGTH);

						drawcount_opaque++;
					}
				}
			}
		}
	}

	drawcount_transparent = 0;

	{
		ZoneScopedN("chunk draw transparent");
		GPU_TRACE(ctx, cmds, "chunk draw transparent");

		vkCmdBindPipeline(cmds, VK_PIPELINE_BIND_POINT_GRAPHICS, transparent_pipeline->pipeline);

		slice_id sid = 0;
		for (int alloci=0; alloci<(int)allocs.size(); ++alloci) {
			VkBuffer vertex_bufs[] = { allocs[alloci].mesh_data.buf };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(cmds, 0, 1, vertex_bufs, offsets);

			for (int slicei=0; slicei<SLICES_PER_ALLOC && sid < (slice_id)chunks.slices.size(); ++slicei, ++sid) {
				auto slice = chunks.slices[sid];

				if (slice.type == 1 && slice.vertex_count > 0) {
					auto& chunk = chunks.chunks[slice.chunkid];
					if ((chunk.flags & Chunk::CULLED) == 0) {
						float3 chunk_pos = (float3)(chunk.pos * CHUNK_SIZE);
						vkCmdPushConstants(cmds, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float3), &chunk_pos);
						
						vkCmdDraw(cmds, BlockMeshes::MERGE_INSTANCE_FACTOR, slice.vertex_count, 0, slicei * CHUNK_SLICE_LENGTH);

						drawcount_transparent++;
					}
				}
			}
		}
	}
}

void ChunkRenderer::create (VulkanWindowContext& ctx, PipelineManager& pipelines, VkRenderPass main_renderpass, VkDescriptorSetLayout common, int frames_in_flight) {
	frames.resize(frames_in_flight);

	pipeline_layout = create_pipeline_layout(ctx.dev, { common }, {{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float3) }});
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
