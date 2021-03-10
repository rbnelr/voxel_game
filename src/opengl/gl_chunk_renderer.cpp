#include "common.hpp"
#include "gl_chunk_renderer.hpp"
#include "chunks.hpp"
#include "chunk_mesher.hpp"
#include "opengl_renderer.hpp"

#include "engine/window.hpp" // for frame_counter hack

namespace gl {

void ChunkRenderer::upload_remeshed (Chunks& chunks) {
	ZoneScoped;
	OGL_TRACE("chunks upload remeshed");

	for (auto& slice : chunks.upload_slices) {
		uint32_t alloci = slice.sliceid / (uint32_t)SLICES_PER_ALLOC;
		uint32_t slicei = slice.sliceid % (uint32_t)SLICES_PER_ALLOC;

		while (alloci >= (uint32_t)allocs.size())
			allocs.emplace_back();

		ZoneScopedN("upload slice");
		OGL_TRACE("upload slice");

		size_t unpadded_size = CHUNK_SLICE_LENGTH * sizeof(BlockMeshInstance); // size of slice data without padding

		glBindBuffer(GL_ARRAY_BUFFER, allocs[alloci].vbo);
		glBufferSubData(GL_ARRAY_BUFFER, slicei * unpadded_size, unpadded_size, slice.data->verts);

		ChunkMeshData::free_slice(slice.data);
	}
	
	// free allocation blocks
	while ((int)chunks.slices.slots.alloc_end < ((int)allocs.size()-1) * (int)SLICES_PER_ALLOC) {
		allocs.pop_back();
	}
}

void ChunkRenderer::draw_chunks (OpenglRenderer& r, Game& game) {
	ZoneScoped;
	OGL_TRACE("draw_chunks");
	
	auto& chunks = game.chunks;

	auto& cull_view = chunks.debug_frustrum_culling ? game.player_view : game.view;

	{
		ZoneScopedN("chunk culling pass");

		for (auto& a : allocs) {
			a.draw_lists[0].count = 0;
			a.draw_lists[1].count = 0;
		}

		auto push_draw_slices = [&] (chunk_id cid, uint32_t vertex_count, slice_id slices, DrawType type) {
			uint32_t remain_count = vertex_count;
			slice_id sliceid = slices;
			while (sliceid != U16_NULL) {
				uint16_t alloci = sliceid / (uint32_t)SLICES_PER_ALLOC;
				uint16_t slicei = sliceid % (uint32_t)SLICES_PER_ALLOC;

				auto& draw_list = allocs[alloci].draw_lists[type];

				auto draw_vertex_count = (uint16_t)std::min(remain_count, (uint32_t)CHUNK_SLICE_LENGTH);
				draw_list.slices[draw_list.count++] = { draw_vertex_count, slicei, cid };

				remain_count -= draw_vertex_count;
				sliceid = chunks.slices[sliceid].next;
			}
		};

		lrgba cols[] = {
			srgba(0, 0, 255, 255),
			srgba(0, 0, 200, 20),
			srgba(255, 0, 0, 180),
		};

		if (chunks.debug_frustrum_culling)
			g_debugdraw.wire_frustrum(cull_view, srgba(141,41,234));

		for (chunk_id cid=0; cid < chunks.end(); ++cid) {
			auto& chunk = chunks[cid];
			if (chunk.flags == 0) continue;

			bool empty = chunk.opaque_mesh_vertex_count == 0 && chunk.transp_mesh_vertex_count == 0;
			
			float3 lo = (float3)(chunk.pos * CHUNK_SIZE);
			float3 hi = (float3)((chunk.pos + 1) * CHUNK_SIZE);

			bool culled = empty || frustrum_cull_aabb(cull_view.frustrum, lo.x, lo.y, lo.z, hi.x, hi.y, hi.z);

			chunks.visualize_chunk(chunk, empty, culled);

			if (!culled) {
				push_draw_slices(cid, chunk.opaque_mesh_vertex_count, chunk.opaque_mesh_slices, DT_OPAQUE);
				push_draw_slices(cid, chunk.transp_mesh_vertex_count, chunk.transp_mesh_slices, DT_TRANSPARENT);
			}
		}
	}

	draw_instances = 0;

	auto draw_slices = [&] (Shader* shader, PipelineState& state, DrawType type, int& drawcount) {
		if (!shader) return;

		glUseProgram(shader->prog);
		r.state.set(state);

		glUniform1i(shader->get_uniform_location("tile_textures"), OpenglRenderer::TILE_TEXTURES);

		{
			auto& block = game.player.selected_block;
			shader->set_uniform("damage",        block.is_selected ? block.damage : 0.0f);
			shader->set_uniform("damaged_block", block.is_selected ? block.hit.pos : int3(0));

			shader->set_uniform("damage_tiles_first", (float)r.damage_tiles.first);
			shader->set_uniform("damage_tiles_count", (float)r.damage_tiles.count);
		}

		auto chunk_pos_loc = shader->get_uniform_location("chunk_pos");

		drawcount = 0;

		for (auto& alloc : allocs) {
			auto& draw_list = alloc.draw_lists[type];
			if (draw_list.count > 0) {

				glBindVertexArray(alloc.vao);

				for (int i=0; i<draw_list.count; ++i) {
					auto& draw = draw_list.slices[i];

					float3 chunk_pos = (float3)(chunks.chunks[draw.chunk].pos * CHUNK_SIZE);
					glUniform3fv(chunk_pos_loc, 1, &chunk_pos.x);

					glDrawArraysInstancedBaseInstance(GL_TRIANGLES,
						0, BlockMeshes::MERGE_INSTANCE_FACTOR,
						draw.vertex_count, draw.slice_idx * CHUNK_SLICE_LENGTH);

					draw_instances += draw.vertex_count;
				}

				drawcount += draw_list.count;
			}
		}

		ZoneValue(drawcount);
	};

	if (_draw_chunks) {
		{
			ZoneScopedN("chunk draw opaque");
			OGL_TRACE("chunk draw opaque");
			draw_slices(shad_opaque, state_opaque, DT_OPAQUE, drawcount_opaque);
		}
		{
			ZoneScopedN("chunk draw transparent");
			OGL_TRACE("chunk draw transparent");
			draw_slices(shad_transparent, state_transparant, DT_TRANSPARENT, drawcount_transparent);
		}
	}
}

//
void Raytracer::upload_changes (OpenglRenderer& r, Game& game) {
	ZoneScoped;
	OGL_TRACE("raytracer upload changes");

	auto& chunks = game.chunks;

	size_t size;

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, chunks_ssbo.ssbo);
	
	size = align_up(game.chunks.chunks.commit_size(), 16 * KB);
	if (init || size != chunks_ssbo.alloc_size) {
		chunks_ssbo.alloc_size = size;
		clog("chunks_ssbo realloc");
		glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, GL_STREAM_DRAW);
	}

	// stream chunks data structure every frame, because reacting to changes is hard an its small enough
	glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, chunks.chunks.commit_size(), chunks.chunks.arr);

	size = align_up(game.chunks.dense_chunks.commit_size(), 4 * MB);
	if (init || size != dense_chunks_ssbo.alloc_size) {
		dense_chunks_ssbo.alloc_size = size;
		clog("dense_chunks_ssbo realloc");
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, dense_chunks_ssbo.ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, GL_STREAM_DRAW);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, chunks.dense_chunks.commit_size(), chunks.dense_chunks.arr);
	}

	size = align_up(game.chunks.dense_subchunks.commit_size(), 8 * MB);
	if (init || size != dense_subchunks_ssbo.alloc_size) {
		dense_subchunks_ssbo.alloc_size = size;
		clog("dense_subchunks_ssbo realloc");
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, dense_subchunks_ssbo.ssbo);
		glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, GL_STREAM_DRAW);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, chunks.dense_subchunks.commit_size(), chunks.dense_subchunks.arr);
	}

	if (!init) { // only upload individual changes if we did not already reupload all data
		for (auto cid : chunks.upload_voxels) {
			auto& chunk = chunks.chunks[cid];

			if (chunk.flags & Chunk::SPARSE_VOXELS) continue;

			auto& dc = chunks.dense_chunks[chunk.voxel_data];
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, dense_chunks_ssbo.ssbo);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, (char*)&dc - (char*)chunks.dense_chunks.arr, sizeof(dc), &dc);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, dense_subchunks_ssbo.ssbo);
			for (uint32_t i=0; i<CHUNK_SUBCHUNK_COUNT; ++i) {
				if (!dc.is_subchunk_sparse(i)) {
					auto& subc = chunks.dense_subchunks[dc.sparse_data[i]];
					glBufferSubData(GL_SHADER_STORAGE_BUFFER, (char*)&subc - (char*)chunks.dense_subchunks.arr, sizeof(subc), &subc);
				}
			}
		}
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	init = false;
}

void Raytracer::draw (OpenglRenderer& r, Game& game) {
	ZoneScoped;
	OGL_TRACE("raytracer_test");
	
	if (!shad) return;

	glUseProgram(shad->prog);

	chunk_id camera_chunk;
	{
		int3 pos = floori((float3)(game.view.cam_to_world * float4(0,0,0,1)) / (float)CHUNK_SIZE);
		camera_chunk = game.chunks.query_chunk(pos);
		if (camera_chunk == U16_NULL)
			camera_chunk = 0; // ugh what do?
	}

	static float t = 0.0f;
	t += g_window.input.real_dt;
	if (t > 60.0f)
		t -= 60.0f;

	shad->set_uniform("camera_chunk", (uint32_t)camera_chunk);
	shad->set_uniform("max_iterations", max_iterations);
	shad->set_uniform("time", t);

	shad->set_uniform("sunlight_enable", sunlight_enable);
	shad->set_uniform("sunlight_rays",   sunlight_rays);
	shad->set_uniform("sunlight_dist",   sunlight_dist);
	shad->set_uniform("sunlight_col",    sunlight_col);

	shad->set_uniform("ambient_enable", ambient_enable);
	shad->set_uniform("ambient_rays",   ambient_rays);
	shad->set_uniform("ambient_dist",   ambient_dist);
	shad->set_uniform("ambient_col",    ambient_col);

	shad->set_uniform("bouncelight_enable", bouncelight_enable);
	shad->set_uniform("bouncelight_rays",   bouncelight_rays);
	shad->set_uniform("bouncelight_dist",   bouncelight_dist);

	shad->set_uniform("visualize_light", visualize_light);

	glUniform1i(shad->get_uniform_location("tile_textures"), OpenglRenderer::TILE_TEXTURES);
	glUniform1i(shad->get_uniform_location("heat_gradient"), OpenglRenderer::HEAT_GRADIENT);
		
	glBindImageTexture(3, r.framebuffer.color, 0, GL_FALSE, 0, GL_WRITE_ONLY, r.framebuffer.color_format);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, chunks_ssbo.ssbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, dense_chunks_ssbo.ssbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, dense_subchunks_ssbo.ssbo);

	int szx = (r.framebuffer.size.x + (compute_local_size.x -1)) / compute_local_size.x;
	int szy = (r.framebuffer.size.y + (compute_local_size.y -1)) / compute_local_size.y;
	glDispatchCompute(szx, szy, 1);

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

} // namespace gl
