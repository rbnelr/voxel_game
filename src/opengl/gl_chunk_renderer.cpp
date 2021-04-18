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

		r.state.set(state);
		r.state.bind_textures(shader, {
			{"tile_textures", r.tile_textures, r.tile_sampler}
		});

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
void Raytracer::upload_changes (OpenglRenderer& r, Game& game, Input& I) {
	ZoneScoped;
	
	if (I.buttons[KEY_R].went_down)
		enable = !enable;

	if (I.buttons[KEY_T].went_down) {
		update_debug_rays = !update_debug_rays;
	}
	if (clear_debug_rays) { //  || update_debug_rays
		r.debug_draw.indirect_lines.clear();
		clear_debug_rays = false;
	}

	// lazy init these to allow json changes to affect the macros
	if (!shad)			shad = r.shaders.compile("raytracer", get_macros(), {{ COMPUTE_SHADER }});
	
	voxels_tex.resize(game.chunks.subchunks.slots.alloc_end);

	if (game.chunks.upload_voxels.empty())
		return;

	OGL_TRACE("raytracer upload changes");
		
	std::vector<int3> chunks;
	for (auto cid : game.chunks.upload_voxels) {
		auto& chunk = game.chunks.chunks[cid];

		auto& vox = game.chunks.chunk_voxels[cid];

		int3 pos = chunk.pos + GPU_WORLD_SIZE_CHUNKS/2;
		if ( (unsigned)(pos.x) < GPU_WORLD_SIZE_CHUNKS &&
				(unsigned)(pos.y) < GPU_WORLD_SIZE_CHUNKS &&
				(unsigned)(pos.z) < GPU_WORLD_SIZE_CHUNKS ) {
			OGL_TRACE("upload chunk data");

			glBindTexture(GL_TEXTURE_3D, subchunks_tex.tex);

			glTexSubImage3D(GL_TEXTURE_3D, 0,
				pos.x*SUBCHUNK_COUNT, pos.y*SUBCHUNK_COUNT, pos.z*SUBCHUNK_COUNT,
				SUBCHUNK_COUNT, SUBCHUNK_COUNT, SUBCHUNK_COUNT,
				GL_RED_INTEGER, GL_UNSIGNED_INT, vox.subchunks);

			glBindTexture(GL_TEXTURE_3D, voxels_tex.tex);

			for (uint32_t i=0; i<CHUNK_SUBCHUNK_COUNT; ++i) {
				auto subc = vox.subchunks[i];
				if ((subc & SUBC_SPARSE_BIT) == 0) {
					voxels_tex.upload(subc, game.chunks.subchunks[subc].voxels);
				}
			}

			chunks.push_back(pos);
		}
	}

	octree.recompute_mips(r, chunks);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glBindTexture(GL_TEXTURE_3D, 0);
}

void ChunkOctrees::recompute_mips (OpenglRenderer& r, std::vector<int3> const& chunks) {
	if (chunks.empty())
		return;

	OGL_TRACE("recompute_mips");

	static constexpr int BATCHSIZE = 16;
	int3 offsets[BATCHSIZE];

	// Each mip get's generated by (multiple) compute shader dispatches
	// if multiple chunks are uploaded they are batched into groups of 16,
	// so usually only one dispatch per mip is required

	{ // layer 0, generate binary octree from voxel data in voxel textures

		glUseProgram(octree_filter_mip0->prog);
		r.state.bind_textures(octree_filter_mip0, {
			{"subchunks_tex", r.raytracer.subchunks_tex.tex},
			{"voxels_tex", r.raytracer.voxels_tex.tex},
		});

		int size = CHUNK_SIZE;
		octree_filter_mip0->set_uniform("size", (GLuint)size);

		glBindImageTexture(4, tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8UI);

		for (int i=0; i<(int)chunks.size(); i+=BATCHSIZE) {
			int remain_count = min(BATCHSIZE, (int)chunks.size() - i);

			memset(offsets, 0, sizeof(offsets));
			for (int j=0; j<remain_count; ++j)
				offsets[j] = chunks[i+j] * CHUNK_SIZE;
			glUniform3uiv(octree_filter_mip0->get_uniform_location("offsets[0]"), ARRLEN(offsets), (GLuint*)offsets);

			int dispatch_size = (size + COMPUTE_FILTER_LOCAL_SIZE-1) / COMPUTE_FILTER_LOCAL_SIZE;
			glDispatchCompute(dispatch_size, dispatch_size, dispatch_size * remain_count);
		}

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
	}

	glUseProgram(octree_filter->prog);

	r.state.bind_textures(octree_filter, {
		{"octree", tex}
	});

	// filter only octree texels for each chunk (up to 4x4x4 work groups)
	for (int layer=1; layer<OCTREE_FILTER_CHUNK_MIPS; ++layer) {
		int size = CHUNK_SIZE >> layer;
		octree_filter->set_uniform("read_mip", layer-1);
		octree_filter->set_uniform("size", (GLuint)size);

		glBindImageTexture(4, tex, layer  , GL_FALSE, 0, GL_WRITE_ONLY, GL_R8UI);

		for (int i=0; i<(int)chunks.size(); i+=BATCHSIZE) {
			int remain_count = min(BATCHSIZE, (int)chunks.size() - i);

			memset(offsets, 0, sizeof(offsets));
			for (int j=0; j<remain_count; ++j)
				offsets[j] = (chunks[i+j] * CHUNK_SIZE) >> layer;
			glUniform3uiv(octree_filter->get_uniform_location("offsets[0]"), ARRLEN(offsets), (GLuint*)offsets);

			int dispatch_size = (size + COMPUTE_FILTER_LOCAL_SIZE-1) / COMPUTE_FILTER_LOCAL_SIZE;
			glDispatchCompute(dispatch_size, dispatch_size, dispatch_size * remain_count);
		}

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
	}

	// filter whole world octree again for remaining mips
	memset(offsets, 0, sizeof(offsets));
	glUniform3uiv(octree_filter->get_uniform_location("offsets[0]"), ARRLEN(offsets), (GLuint*)offsets);

	for (int layer=OCTREE_FILTER_CHUNK_MIPS; layer<OCTREE_MIPS; ++layer) {
		int size = TEX_WIDTH >> layer;
		octree_filter->set_uniform("read_mip", layer-1);
		octree_filter->set_uniform("size", (GLuint)size);

		glBindImageTexture(4, tex, layer  , GL_FALSE, 0, GL_WRITE_ONLY, GL_R8UI);

		int dispatch_size = (size + COMPUTE_FILTER_LOCAL_SIZE-1) / COMPUTE_FILTER_LOCAL_SIZE;
		glDispatchCompute(dispatch_size, dispatch_size, dispatch_size);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
	}
}

void Raytracer::draw (OpenglRenderer& r, Game& game) {
	ZoneScoped;
	OGL_TRACE("raytracer.draw");

	if (!shad->prog) return;
	
	glUseProgram(shad->prog);

	shad->set_uniform("taa_alpha",          taa_alpha);

	shad->set_uniform("max_iterations",     max_iterations);
	shad->set_uniform("rand_frame_index",   rand_seed_time ? (uint32_t)g_window.frame_counter : 0);

	shad->set_uniform("sunlight_enable",    sunlight_enable);
	shad->set_uniform("sunlight_dist",      sunlight_dist);
	shad->set_uniform("sunlight_col",       sunlight_col);

	float3 sun_dir = rotate3_Z(sunlight_ang.x) * rotate3_X(sunlight_ang.y) * float3(0,+1,0);

	shad->set_uniform("sun_pos",            sunlight_pos);
	shad->set_uniform("sun_pos_size",       sun_pos_size);
	shad->set_uniform("sun_dir",            sun_dir);
	shad->set_uniform("sun_dir_rand",       sun_dir_rand);

	shad->set_uniform("bounces_enable",     bounces_enable);
	shad->set_uniform("bounces_max_dist",   bounces_max_dist);
	shad->set_uniform("bounces_max_count",  bounces_max_count);

	//shad->set_uniform("rays",               rays);
	shad->set_uniform("visualize_light",    visualize_light);

	static float time = 0;
	time += g_window.input.dt;
	//time = fmodf(time, 100.0f); // shader can't handle modded value, should be possible with correctly chosen values everywhere

	shad->set_uniform("water_F0",           water_F0);
	shad->set_uniform("water_normal_time",  time);

	if (taa_enable)
		history.resize(r.framebuffer.size);

	GLuint prev_color  = history.colors[history.cur ^ 1];
	GLuint cur_color   = history.colors[history.cur];
	GLuint prev_posage = history.posage[history.cur ^ 1];
	GLuint cur_posage  = history.posage[history.cur];

	if (taa_enable)
		history.cur ^= 1;
	
	r.state.bind_textures(shad, {
		{"subchunks_tex", subchunks_tex.tex},
		{"voxels_tex", voxels_tex.tex},
		{"octree", octree.tex},

		{"tile_textures", r.tile_textures, r.tile_sampler},
		{"water_N_A", r.water_N_A, r.normal_sampler_wrap},
		{"water_N_B", r.water_N_B, r.normal_sampler_wrap},

		{"textures_A", r.textures_A, r.normal_sampler_wrap},
		{"textures_N", r.textures_N, r.normal_sampler_wrap},
		{"textures2_A", r.textures2_A, r.normal_sampler_wrap},
		{"textures2_N", r.textures2_N, r.normal_sampler_wrap},

		(taa_enable ? StateManager::TextureBind{"taa_history_color", {GL_TEXTURE_2D, prev_color}, history.sampler} : StateManager::TextureBind{}),
		(taa_enable ? StateManager::TextureBind{"taa_history_posage", {GL_TEXTURE_2D, prev_posage}, history.sampler_int} : StateManager::TextureBind{}),
		
		{"heat_gradient", r.gradient, r.normal_sampler},
	});

	glBindImageTexture(4, r.framebuffer.color, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
	if (taa_enable) {
		glBindImageTexture(5, cur_color , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		glBindImageTexture(6, cur_posage, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16UI );
	}

	int2 dispatch_size;
	dispatch_size.x = (r.framebuffer.size.x + (compute_local_size.x -1)) / compute_local_size.x;
	dispatch_size.y = (r.framebuffer.size.y + (compute_local_size.y -1)) / compute_local_size.y;

	float4x4 world2clip = game.view.cam_to_clip * (float4x4)game.view.world_to_cam;

	shad->set_uniform("update_debug_rays",  update_debug_rays);

	shad->set_uniform("dispatch_size", dispatch_size);
	shad->set_uniform("prev_world2clip", init ? world2clip : prev_world2clip);

	prev_world2clip = world2clip;
	init = false;

	glDispatchCompute(dispatch_size.x, dispatch_size.y, 1);

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

} // namespace gl
