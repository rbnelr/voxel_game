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
	if (!shad_lighting)	shad_lighting = r.shaders.compile("rt_lighting", get_lighting_macros(), {{ COMPUTE_SHADER }});

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

			//octree.rebuild_chunk(game.chunks, cid, pos);
			chunks.push_back(pos);
		}
	}

	octree.recompute_mips(r, chunks);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glBindTexture(GL_TEXTURE_3D, 0);
}

void ChunkOctrees::rebuild_chunk (Chunks& chunks, chunk_id cid, int3 const& cpos) {
	ZoneScoped;

	{
		ZoneScopedN("compute mip 0");
		auto bid_air = g_assets.block_types.map_id("air");

		for (int z=0; z<CHUNK_SIZE/2; z++)
		for (int y=0; y<CHUNK_SIZE/2; y++)
		for (int x=0; x<CHUNK_SIZE/2; x++) {
			uint8_t mask = 0;
			for (int i=0; i<8; ++i) {
				auto vox = chunks.read_block(x*2 + (i&1), y*2 + ((i>>1)&1), z*2 + (i>>2), cid);
				mask |= (vox != bid_air ? 1:0) << i;
			}
			temp_buffer[z][y][x] = mask;
		}
	}

	glBindTexture(GL_TEXTURE_3D, tex);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // required for the higher mip leves
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	{
		int size = CHUNK_SIZE/2;
		glTexSubImage3D(GL_TEXTURE_3D, 0,
			cpos.x*size, cpos.y*size, cpos.z*size,
			size, size, size,
			GL_RED_INTEGER, GL_UNSIGNED_BYTE, &temp_buffer);
	}

	glTextureParameteri(tex, GL_TEXTURE_BASE_LEVEL, 0);
	glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, OCTREE_LAYERS-1);

	glBindTexture(GL_TEXTURE_3D, 0);
}

void ChunkOctrees::recompute_mips (OpenglRenderer& r, std::vector<int3> const& chunks) {
	if (chunks.empty())
		return;

	OGL_TRACE("recompute_mips");


	int3 offsets[16];

	// Each mip get's generated by (multiple) compute shader dispatches
	// if multiple chunks are uploaded they are batched into groups of 16,
	// so usually only one dispatch per mip is required

	{ // layer 0, generate binary octree from voxel data in voxel textures

		glUseProgram(octree_filter_mip0->prog);
		r.raytracer.bind_voxel_textures(r, octree_filter_mip0);

		glBindImageTexture(4, tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8UI);

		int size = CHUNK_SIZE/2;
		
		octree_filter_mip0->set_uniform("size", (GLuint)size);

		for (int i=0; i<(int)chunks.size(); i+=16) {
			int remain_count = (int)chunks.size() - i;

			memset(offsets, 0, sizeof(offsets));
			for (int j=0; j<remain_count; ++j)
				offsets[j] = chunks[i+j] * (CHUNK_SIZE/2);
			glUniform3uiv(octree_filter_mip0->get_uniform_location("offsets[0]"), 16, (GLuint*)&offsets[0].x);

			int dispatch_size = (size + OCTREE_FILTER_LOCAL_SIZE-1) / OCTREE_FILTER_LOCAL_SIZE;
			glDispatchCompute(dispatch_size, dispatch_size, dispatch_size * remain_count);
		}

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
	}

	glUseProgram(octree_filter->prog);

	glActiveTexture(GL_TEXTURE0 +OpenglRenderer::OCTREE_TEX);
	glBindTexture(GL_TEXTURE_3D, tex);
	glUniform1i(octree_filter->get_uniform_location("octree"), OpenglRenderer::OCTREE_TEX);

	// filter only octree texels for each chunk (up to 4x4x4 work groups)
	for (int layer=1; layer<OCTREE_FILTER_CHUNK_LAYERS; ++layer) {
		int size = (CHUNK_SIZE/2) >> layer;

		glBindImageTexture(4, tex, layer  , GL_FALSE, 0, GL_WRITE_ONLY, GL_R8UI);

		octree_filter->set_uniform("read_mip", layer-1);
		octree_filter->set_uniform("size", (GLuint)size);

		for (int i=0; i<(int)chunks.size(); i+=16) {
			int remain_count = (int)chunks.size() - i;

			memset(offsets, 0, sizeof(offsets));
			for (int j=0; j<remain_count; ++j)
				offsets[j] = (chunks[i+j] * (CHUNK_SIZE/2)) >> layer;
			glUniform3uiv(octree_filter->get_uniform_location("offsets[0]"), 16, (GLuint*)&offsets[0].x);

			int dispatch_size = (size + OCTREE_FILTER_LOCAL_SIZE-1) / OCTREE_FILTER_LOCAL_SIZE;
			glDispatchCompute(dispatch_size, dispatch_size, dispatch_size * remain_count);
		}

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
	}

	// filter whole world octree again for remaining mips
	memset(offsets, 0, sizeof(offsets));
	glUniform3uiv(octree_filter->get_uniform_location("offsets[0]"), 16, (GLuint*)&offsets[0].x);

	for (int layer=OCTREE_FILTER_CHUNK_LAYERS; layer<OCTREE_LAYERS; ++layer) {
		int size = TEX_WIDTH >> layer;

		glBindImageTexture(4, tex, layer  , GL_FALSE, 0, GL_WRITE_ONLY, GL_R8UI);

		octree_filter->set_uniform("read_mip", layer-1);
		octree_filter->set_uniform("size", (GLuint)size);

		int dispatch_size = (size + OCTREE_FILTER_LOCAL_SIZE-1) / OCTREE_FILTER_LOCAL_SIZE;
		glDispatchCompute(dispatch_size, dispatch_size, dispatch_size);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
	}
}

void Raytracer::bind_voxel_textures (OpenglRenderer& r, Shader* shad) {
	// no samplers because these need to be texelFetch'ed anyway

	glActiveTexture(GL_TEXTURE0 +OpenglRenderer::SUBCHUNKS_TEX);
	glBindTexture(GL_TEXTURE_3D, subchunks_tex.tex);

	glActiveTexture(GL_TEXTURE0 +OpenglRenderer::VOXELS_TEX);
	glBindTexture(GL_TEXTURE_3D, voxels_tex.tex);

	GLint tex_units[2] = { OpenglRenderer::SUBCHUNKS_TEX, OpenglRenderer::VOXELS_TEX };
	glUniform1iv(shad->get_uniform_location("voxels[0]"), 2, tex_units);

	glActiveTexture(GL_TEXTURE0 +OpenglRenderer::OCTREE_TEX);
	glBindTexture(GL_TEXTURE_3D, octree.tex);
	glUniform1i(shad->get_uniform_location("octree"), OpenglRenderer::OCTREE_TEX);
}
// setup common state for rt_util.glsl
void Raytracer::setup_shader (OpenglRenderer& r, Shader* shad, bool rt_light) {
	glUseProgram(shad->prog);

	shad->set_uniform("taa_alpha",          rt_light ? taa_alpha_rt_light : taa_alpha);

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

	shad->set_uniform("water_F0",           water_F0);


	glUniform1i(shad->get_uniform_location("tile_textures"), OpenglRenderer::TILE_TEXTURES);
	glUniform1i(shad->get_uniform_location("heat_gradient"), OpenglRenderer::GRADIENT);

	bind_voxel_textures(r, shad);
}

void Raytracer::draw (OpenglRenderer& r, Game& game) {
	ZoneScoped;
	OGL_TRACE("raytracer.draw");

	if (!shad->prog) return;

	setup_shader(r, shad, false);

	//
	FramebufferTex& prev_img = framebuffers[cur_frambuffer ^ 1];
	FramebufferTex& curr_img = framebuffers[cur_frambuffer];

	glActiveTexture(GL_TEXTURE0 +OpenglRenderer::PREV_FRAMEBUFFER);
	curr_img.resize(r.framebuffer.size, cur_frambuffer);

	cur_frambuffer ^= 1;

	glBindTexture(GL_TEXTURE_2D, prev_img.tex);
	glBindSampler(OpenglRenderer::PREV_FRAMEBUFFER, r.normal_sampler);
	glUniform1i(shad->get_uniform_location("prev_framebuffer"), OpenglRenderer::PREV_FRAMEBUFFER);

	glBindImageTexture(4, curr_img.tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
	

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

	int2 sz = r.framebuffer.size;
	glBlitNamedFramebuffer(curr_img.fbo, r.framebuffer.fbo, 0,0,sz.x,sz.y, 0,0,sz.x,sz.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

void Raytracer::compute_lighting (OpenglRenderer& r, Game& game) {
	ZoneScoped;
	OGL_TRACE("raytracer.compute_lighting");

	if (!shad_lighting->prog) return;

	setup_shader(r, shad_lighting, true);

	size_t faces_computed = 0;

	shad_lighting->set_uniform("_dbg_ray_pos", game.player.selected_block.hit.pos);

	auto compute_slice = [&] (chunk_id cid, uint16_t alloci, uint16_t slicei, uint32_t vertex_count) {
		if (vertex_count > 0) {
			auto& alloc = r.chunk_renderer.allocs[alloci];

			glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 4, alloc.vbo, slicei * CHUNK_SLICE_SIZE, CHUNK_SLICE_SIZE);
			glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 5, alloc.lighting_vbo, slicei * ChunkRenderer::LIGHTING_VBO_SLICE_SIZE, ChunkRenderer::LIGHTING_VBO_SLICE_SIZE);

			shad_lighting->set_uniform("vertex_count", vertex_count);
			shad_lighting->set_uniform("update_debug_rays", update_debug_rays); // only draw for first slice

			int dispatch_size = (vertex_count + (lighting_workgroup_size -1)) / lighting_workgroup_size;
			glDispatchCompute(dispatch_size, 1, 1);

			faces_computed += vertex_count;
		}
	};

	auto compute_slices = [&] (chunk_id cid, uint32_t remain_count, slice_id slices) {
		slice_id sliceid = slices;
		while (sliceid != U16_NULL) {
			uint16_t alloci = sliceid / (uint32_t)r.chunk_renderer.SLICES_PER_ALLOC;
			uint16_t slicei = sliceid % (uint32_t)r.chunk_renderer.SLICES_PER_ALLOC;

			uint32_t vertex_count = std::min(remain_count, (uint32_t)CHUNK_SLICE_LENGTH);

			compute_slice(cid, alloci, slicei, vertex_count);

			remain_count -= vertex_count;
			sliceid = game.chunks.slices[sliceid].next;
		}
	};

	int3 floored = floori(game.player.pos / CHUNK_SIZE);
	int3 start = floored - lighting_update_r;
	int3 end   = floored + lighting_update_r;

	shad_lighting->set_uniform("samples", lighting_samples);

	for (int z=start.z; z<=end.z; ++z)
	for (int y=start.y; y<=end.y; ++y)
	for (int x=start.x; x<=end.x; ++x) {
		chunk_id cid = game.chunks.query_chunk(int3(x,y,z));

		shad_lighting->set_uniform("chunk_pos", (float3)int3(x,y,z) * CHUNK_SIZE);

		if (cid != U16_NULL && game.chunks[cid].flags != 0) {
			auto& chunk = game.chunks[cid];

			bool empty = chunk.opaque_mesh_vertex_count == 0 && chunk.transp_mesh_vertex_count == 0;
			if (!empty) {
				compute_slices(cid, chunk.opaque_mesh_vertex_count, chunk.opaque_mesh_slices);
				compute_slices(cid, chunk.transp_mesh_vertex_count, chunk.transp_mesh_slices);
			}
		}
	}

	ImGui::Text("faces_computed: %d", faces_computed);

	glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
}

} // namespace gl
