#include "common.hpp"
#include "gl_chunk_renderer.hpp"
#include "chunks.hpp"
#include "chunk_mesher.hpp"
#include "opengl_renderer.hpp"

namespace gl {

void ChunkRenderer::upload_remeshed (Chunks& chunks) {
	ZoneScoped;

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

	chunks.upload_slices.clear();
	chunks.upload_slices.shrink_to_fit();

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

	auto draw_slices = [&] (Shader* shader, PipelineState& state, DrawType type, int& drawcount) {
		glUseProgram(shader->prog);
		r.state.set(state);

		glUniform1i(shader->get_uniform_location("tile_textures"), 0);

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
				}

				drawcount += draw_list.count;
			}
		}

		ZoneValue(drawcount);
	};

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

//
void Raytracer::draw (OpenglRenderer& r, Game& game) {
	ZoneScoped;
	OGL_TRACE("raytracer_test");

	glUseProgram(shad_test->prog);

	//shad_test->set_uniform("img_size", (float2)r.framebuffer.size);
	
	//{
	//	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	//	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float4) * cols.size(), cols.data(), GL_STREAM_DRAW);
	//	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
	//}

	glBindImageTexture(4, r.framebuffer.color, 0, GL_FALSE, 0, GL_WRITE_ONLY, r.framebuffer.color_format);

	glDispatchCompute(r.framebuffer.size.x, r.framebuffer.size.y, 1);

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// TODO: how to unbind framebuffer??
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

} // namespace gl
