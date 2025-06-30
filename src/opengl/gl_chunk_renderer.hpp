#pragma once
#include "common.hpp"
#include "chunks.hpp"
#include "chunk_mesher.hpp"
#include "world_generator.hpp"
#include "opengl_renderer.hpp"
#include "gl_raytracer.hpp" // for gpu voxel data

namespace gl {
// Rasterized chunk renderer
struct ChunkRenderer {
	SERIALIZE(ChunkRenderer, _draw_chunks)

	static constexpr int SLICES_PER_ALLOC = 1024;
	static constexpr size_t ALLOC_SIZE = SLICES_PER_ALLOC * CHUNK_SLICE_SIZE; // size of vram allocations

	enum DrawType { DT_OPAQUE=0, DT_TRANSPARENT=1 };

	struct AllocBlock {
		Vao vao;
		Vbo vbo;

		AllocBlock () {
			ZoneScopedC(tracy::Color::Crimson);
			OGL_TRACE("AllocBlock()");

			vbo = Vbo("ChunkRenderer.AllocBlock.vbo");
			vao = setup_vao<BlockMeshInstance>("ChunkRenderer.vao", vbo);

			//
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, ALLOC_SIZE, nullptr, GL_STREAM_DRAW);
		}

		struct DrawList {
			struct DrawSlice {
				uint16_t	vertex_count;
				uint16_t	slice_idx; // in alloc (not global slice id)
				chunk_id	chunk;
			};
			int				count;
			DrawSlice		slices[SLICES_PER_ALLOC];
		};

		DrawList		draw_lists[2];
	};

	std::vector<AllocBlock>	allocs;

	Shader* shad_opaque;
	Shader* shad_transparent;

	PipelineState state_opaque;
	PipelineState state_transparant;

	ChunkRenderer (Shaders& shaders) {
		shad_opaque			= shaders.compile("chunks", {{"ALPHA_TEST", "1"}});
		shad_transparent	= shaders.compile("chunks", {{"ALPHA_TEST", "0"}});

		state_opaque.depth_test		= true;
		state_opaque.depth_write	= true;
		state_opaque.blend_enable	= false;

		state_transparant.depth_test	= true;
		state_transparant.depth_write	= true;
		state_transparant.blend_enable	= true;
	}

	int drawcount_opaque = 0;
	int drawcount_transparent = 0;
	size_t draw_instances = 0;

	bool _draw_chunks = true; // allow disabling for debugging

	float detail_draw_dist = 200;

	float water_scrolling_t = 0;
	
	void imgui (Chunks& chunks) {

		size_t vertices = 0;
		size_t slices_total = 0;

		for (chunk_id cid=0; cid<chunks.end(); ++cid) {
			if (chunks[cid].flags == 0) continue;

			vertices += chunks[cid].opaque_mesh_vertex_count;
			vertices += chunks[cid].transp_mesh_vertex_count;

			slices_total += _slices_count(chunks[cid].opaque_mesh_vertex_count);
			slices_total += _slices_count(chunks[cid].transp_mesh_vertex_count);
		}

		size_t draw_vertices = draw_instances * BlockMeshes::MERGE_INSTANCE_FACTOR;

		ImGui::Separator();

		ImGui::Text("Drawcalls: opaque: %3d  transparent: %3d (%3d / %3d slices - %3.0f%%)",
			drawcount_opaque, drawcount_transparent, drawcount_opaque + drawcount_transparent,
			slices_total, (float)(drawcount_opaque + drawcount_transparent) / slices_total * 100);

		ImGui::Text("Vertex workload : drawn instances: %12s (vertices: %12s)",
			format_thousands(draw_instances).c_str(), format_thousands(draw_vertices).c_str());

		ImGui::Text("Mesh allocs: %2d  slices: %5d  vertices: %12s",
			allocs.size(), slices_total, format_thousands(vertices).c_str());
		ImGui::Text("Mesh VRAM: used: %7.3f MB  commited: %7.3f MB (%6.2f%% usage)",
			(float)(vertices * sizeof(BlockMeshInstance)) / 1024 / 1024,
			(float)(allocs.size() * ALLOC_SIZE) / 1024 / 1024,
			(float)(vertices * sizeof(BlockMeshInstance)) / (float)(allocs.size() * ALLOC_SIZE) * 100);

		if (ImGui::TreeNode("slices alloc")) {
			print_bitset_allocator(chunks.slices.slots, CHUNK_SLICE_SIZE, ALLOC_SIZE);
			ImGui::TreePop();
		}
		
		ImGui::DragFloat("detail_draw_dist", &detail_draw_dist);
	}

	void upload_remeshed (Chunks& chunks) {
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

	void draw_chunks (OpenglRenderer& r) {
		ZoneScoped;
		OGL_TRACE("draw_chunks");
	
		auto& chunks = *g->chunks;

		auto& cull_view = chunks.debug_frustrum_culling ? g->player_view : g->view;

		{
			ZoneScopedN("chunk culling pass");

			for (auto& a : allocs) {
				a.draw_lists[0].count = 0;
				a.draw_lists[1].count = 0;
			}

			auto push_draw_slices = [&] (chunk_id cid, uint32_t remain_count, slice_id slices, DrawType type) {
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

				chunks.visualize_chunk(cid, chunk, empty, culled);

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
			r.debug_draw.wireframe_able_shader(shader);
			r.state.set(state);
			r.state.bind_textures(shader, {
				{"tile_textures", r.tile_textures, r.pixelated_sampler},

				{"voxel_tex", r.raytracer->voxel_tex.tex}, // Take this from the raytracer for testing
				{"water_displ_tex", r.water_displ},
			});

			{
				auto& block = g->player->selected_block;
				shader->set_uniform("damage",        block.is_selected ? block.damage : 0.0f);
				shader->set_uniform("damaged_block", block.is_selected ? block.hit.pos : int3(0));

				shader->set_uniform("damage_tiles_first", (float)r.damage_tiles.first);
				shader->set_uniform("damage_tiles_count", (float)r.damage_tiles.count);

				shader->set_uniform("detail_draw_dist", detail_draw_dist);
			
				shader->set_uniform("water_scrolling_t", water_scrolling_t);

				shader->set_uniform("fog_col", r.fog.fog_col);
				shader->set_uniform("fog_dens", r.fog.fog_dens * 0.001f);
				shader->set_uniform("water_fog_col", r.fog.water_fog_col);
				shader->set_uniform("water_fog_dens", r.fog.water_fog_dens * 0.001f);
				shader->set_uniform("water_z", (float)g->world_gen->water_level);
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

		water_scrolling_t += g->input.real_dt * 0.01f;
		water_scrolling_t = fmodf(water_scrolling_t, 1.0f);
	}

};

} // namespace gl
