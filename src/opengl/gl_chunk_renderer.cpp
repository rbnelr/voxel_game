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
lrgba cols[] = {
	{1,0,0,1},
	{0,1,0,1},
	{0,0,1,1},
	{1,1,0,1},
	{1,0,1,1},
	{0,1,1,1},
};
void Raytracer::vct_conedev (OpenglRenderer& r, Game& game) {
	static bool draw_cones=false, draw_boxes=false;
	static float start_dist = 0.16f;
	
	ImGui::Checkbox("draw cones", &draw_cones);
	ImGui::SameLine();
	ImGui::Checkbox("draw boxes", &draw_boxes);
	ImGui::SliderFloat("start_dist", &start_dist, 0.05f, 2);

	struct Set {
		int count = 8;
		float cone_ang = 40.1f;

		float start_azim = 22.5f;
		float elev_offs = 2.1f;

		float weight = 1.0f;
	};
	static std::vector<Set> sets = {
		{ 8, 40.1f, 22.5f, 2.1f, 0.25f },
		{ 4, 38.9f, 45.0f, 40.2f, 1.0f },
	};

	int set_count = (int)sets.size();
	ImGui::DragInt("sets", &set_count, 0.01f);
	sets.resize(set_count);

	cone_data.count = 0;

	float total_weight = 0;

	int j=0;
	for (auto& s : sets) {
		ImGui::TreeNodeEx(&s, ImGuiTreeNodeFlags_DefaultOpen, "Set");

		ImGui::SliderInt("count", &s.count, 0, 16);
		ImGui::DragFloat("cone_ang", &s.cone_ang, 0.1f, 0, 180);

		ImGui::DragFloat("start_azim", &s.start_azim, 0.1f);
		ImGui::DragFloat("elev_offs", &s.elev_offs, 0.1f);

		ImGui::DragFloat("weight", &s.weight, 0.01f);

		float ang = deg(s.cone_ang);

		for (int i=0; i<s.count; ++i) {
			float3 cone_pos = game.player.pos;

			float3x3 rot = rotate3_Z((float)(i-1) / s.count * deg(360) + deg(s.start_azim)) *
					rotate3_Y(deg(90) - ang * 0.5f - deg(s.elev_offs));

			auto& col = cols[i % ARRLEN(cols)];
			if (draw_cones) g_debugdraw.wire_cone(cone_pos, ang, 30, rot, col, 32, 4);

			float3 cone_dir = rot * float3(0,0,1);
			float cone_slope = tan(ang * 0.5f);
			float dist = start_dist;

			cone_data.cones[j++] = { cone_dir, cone_slope, s.weight, 0 };
		
			int j=0;
			while (j++ < 100 && dist < 100.0f) {
				float3 pos = cone_pos + cone_dir * dist;
				float r = cone_slope * dist;
				//g_debugdraw.wire_sphere(pos, r, col, 16, 4);
				if (draw_boxes) g_debugdraw.wire_cube(pos, r*2, col);

				dist = (dist + r) / (1.0f - cone_slope);
			}

			total_weight += s.weight;
		}

		cone_data.count += s.count;

		ImGui::TreePop();
	}

	// normalize weights
	j=0;
	for (auto& s : sets) {
		for (int i=0; i<s.count; ++i) {
			cone_data.cones[j++].weight /= total_weight;
		}
	}
}

void Raytracer::upload_changes (OpenglRenderer& r, Game& game, Input& I) {
	ZoneScoped;
	
	vct_conedev(r, game);

	bool macro_change = false;

	if (I.buttons[KEY_R].went_down)
		enable = !enable;
	if (I.buttons[KEY_V].went_down) {
		enable_vct = !enable_vct;
		macro_change = true;
	}

	if (I.buttons[KEY_T].went_down) {
		update_debugdraw = !update_debugdraw;
	}
	if (clear_debugdraw || update_debugdraw) {
		r.debug_draw.clear_indirect();
		clear_debugdraw = false;
	}

	if (macro_change && rt_shad) {
		rt_shad->macros = get_rt_macros();
		rt_shad->recompile("macro_change", false);
	}
	if (macro_change && vct_shad) {
		vct_shad->macros = get_vct_macros();
		vct_shad->recompile("macro_change", false);
	}

	if (vct_visualize_sparse)
		vct_data.visualize_sparse(r);

	// lazy init these to allow json changes to affect the macros
	if (!rt_shad)	rt_shad  = r.shaders.compile("rt_gbufgen", get_rt_macros(), {{ COMPUTE_SHADER }});
	if (!vct_shad)	vct_shad = r.shaders.compile("vct_shade", get_vct_macros(), {{ COMPUTE_SHADER }});
	
	voxels_tex.resize(game.chunks.subchunks.slots.alloc_end);

	std::vector<int3> chunks;

	if (!game.chunks.upload_voxels.empty()) {
		OGL_TRACE("raytracer upload changes");
		
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

		octree.recompute_mips(r, game, chunks);
		vct_data.recompute_mips(r, game, chunks);
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glBindTexture(GL_TEXTURE_3D, 0);
}

void ChunkOctrees::recompute_mips (OpenglRenderer& r, Game& game, std::vector<int3> const& chunks) {
	if (chunks.empty())
		return;
	ZoneScoped;

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

void VCT_Data::recompute_mips (OpenglRenderer& r, Game& game, std::vector<int3> const& chunks) {
	if (chunks.empty())
		return;
	ZoneScoped;

	OGL_TRACE("vct.recompute_mips");

	{ // commit sparse pages
		ZoneScopedN("commit sparse pages");

		assert(all(sparse_size > SUBCHUNK_SIZE));

		int3 pages_per_chunk = CHUNK_SIZE / sparse_size;
		int3 subchunks_per_page = sparse_size / SUBCHUNK_SIZE;
		auto air = SUBC_SPARSE_BIT | g_assets.block_types.map_id("air");

		for (auto& chunk_pos : chunks) {
			auto chunk_id = game.chunks.query_chunk(chunk_pos - GPU_WORLD_SIZE_CHUNKS/2);
			auto& vox = game.chunks.chunk_voxels[chunk_id];

			// for page in texture for chunk
			for (int pz=0; pz<pages_per_chunk.z; ++pz)
			for (int py=0; py<pages_per_chunk.y; ++py)
			for (int px=0; px<pages_per_chunk.x; ++px) {

				int3 idx = chunk_pos * pages_per_chunk + int3(px,py,pz);
				bool page_sparse = sparse_page_state[idx];

				if (page_sparse) {
					int3 offs = idx * sparse_size;
					glTexturePageCommitmentEXT(basetex.tex, 0, offs.x,offs.y,offs.z, sparse_size.x,sparse_size.y,sparse_size.z, true);
				}
			}
		}
	}

	// Each mip gets generated by (multiple) compute shader dispatches
	// if multiple chunks are uploaded they are batched into groups of 16,
	// so usually only one dispatch per mip is required

	static constexpr int BATCHSIZE = 16;
	int3 offsets[BATCHSIZE];

	auto dispatch_chunked = [&] (int layer, Shader* shad) {
		int size = CHUNK_SIZE >> layer;
		shad->set_uniform("size", (GLuint)size);

		if (layer == 0) {
			glBindImageTexture(0, basetex.texview, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8UI);
		} else {
			shad->set_uniform("read_mip", layer-2);

			glBindImageTexture(0, preint.texview, layer-1, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8UI);
		}

		for (int i=0; i<(int)chunks.size(); i+=BATCHSIZE) {
			int remain_count = min(BATCHSIZE, (int)chunks.size() - i);

			memset(offsets, 0, sizeof(offsets));
			for (int j=0; j<remain_count; ++j)
				offsets[j] = (chunks[i+j] * CHUNK_SIZE) >> layer;
			glUniform3uiv(shad->get_uniform_location("offsets[0]"), ARRLEN(offsets), (GLuint*)offsets);

			int dispatch_size = (size + COMPUTE_FILTER_LOCAL_SIZE-1) / COMPUTE_FILTER_LOCAL_SIZE;
			glDispatchCompute(dispatch_size, dispatch_size, dispatch_size * remain_count);
		}

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
	};
	auto dispatch_whole = [&] (int layer, Shader* shad) {
		int size = TEX_WIDTH >> layer;
		shad->set_uniform("size", (GLuint)size);
		shad->set_uniform("read_mip", layer-2);

		glBindImageTexture(0, preint.texview, layer-1, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8UI);

		memset(offsets, 0, sizeof(offsets));
		glUniform3uiv(shad->get_uniform_location("offsets[0]"), ARRLEN(offsets), (GLuint*)offsets);

		int dispatch_size = (size + COMPUTE_FILTER_LOCAL_SIZE-1) / COMPUTE_FILTER_LOCAL_SIZE;
		glDispatchCompute(dispatch_size, dispatch_size, dispatch_size);

		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
	};

	{ // layer 0, generate texture from voxel data in voxel textures
		glUseProgram(filter_mip0->prog);
		r.state.bind_textures(filter_mip0, {
			{"subchunks_tex", r.raytracer.subchunks_tex.tex},
			{"voxels_tex", r.raytracer.voxels_tex.tex},
			{"tile_textures", r.tile_textures, r.tile_sampler},
		});

		dispatch_chunked(0, filter_mip0);
	}

	{ // layer 1+
		glUseProgram(filter->prog);

		r.state.bind_textures(filter, {
			{"vct_basetex", basetex.tex, filter_sampler},
			{"vct_preint", preint.tex, filter_sampler},
		});

		// filter only texels for each chunk (up to 4x4x4 work groups)
		for (int layer=1; layer<FILTER_CHUNK_MIPS; ++layer)
			dispatch_chunked(layer, filter);
	}

	{ // filter whole texture again for remaining mips
		for (int layer=FILTER_CHUNK_MIPS; layer<MIPS; ++layer)
			dispatch_whole(layer, filter);
	}

	// unbind
	glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

	{ // decommit sparse pages
		ZoneScopedN("decommit sparse pages");

		assert(all(sparse_size > SUBCHUNK_SIZE));

		int3 pages_per_chunk = CHUNK_SIZE / sparse_size;
		int3 subchunks_per_page = sparse_size / SUBCHUNK_SIZE;
		auto air = SUBC_SPARSE_BIT | g_assets.block_types.map_id("air");

		for (auto& chunk_pos : chunks) {
			auto chunk_id = game.chunks.query_chunk(chunk_pos - GPU_WORLD_SIZE_CHUNKS/2);
			auto& vox = game.chunks.chunk_voxels[chunk_id];

			// for page in texture for chunk
			for (int pz=0; pz<pages_per_chunk.z; ++pz)
			for (int py=0; py<pages_per_chunk.y; ++py)
			for (int px=0; px<pages_per_chunk.x; ++px) {

				bool sparse = true;

				// for subchunks in page
				for (int sz=0; sz<subchunks_per_page.z; ++sz)
				for (int sy=0; sy<subchunks_per_page.y; ++sy)
				for (int sx=0; sx<subchunks_per_page.x; ++sx) {

					int3 idx = int3(px,py,pz) * subchunks_per_page + int3(sx,sy,sz);
					auto aubc = vox.subchunks[IDX3D(idx.x,idx.y,idx.z, SUBCHUNK_COUNT)];
					bool is_air = aubc == air;
					if (!is_air) {
						sparse = false;
						goto end;
					}
				} end:;

				int3 idx = chunk_pos * pages_per_chunk + int3(px,py,pz);
				bool& page_sparse = sparse_page_state[idx];
				page_sparse = sparse;
				
				if (sparse) {
					int3 offs = idx * sparse_size;
					glTexturePageCommitmentEXT(basetex.tex, 0, offs.x,offs.y,offs.z, sparse_size.x,sparse_size.y,sparse_size.z, false);
				}
			}
		}
	}
}
void VCT_Data::visualize_sparse (OpenglRenderer& r) {
	auto col = srgba(255, 255,   0, 255);

	for (int pz=0; pz<sparse_page_state.size.z; pz++)
	for (int py=0; py<sparse_page_state.size.y; py++)
	for (int px=0; px<sparse_page_state.size.x; px++) {
		bool page_sparse = sparse_page_state.get(px,py,pz);
		if (page_sparse) {
			float3 pos = (float3)(int3(px,py,pz) * sparse_size - GPU_WORLD_SIZE/2);
			g_debugdraw.wire_cube(pos + (float3)(sparse_size/2), (float3)sparse_size * 0.997f, col);
		}
	}
}

void Raytracer::draw (OpenglRenderer& r, Game& game) {
	ZoneScoped;

	if (!rt_shad->prog || !vct_shad->prog) return;

	if (!test) {
		gbuf.resize(r.framebuffer.size);
		{
			ZoneScopedN("rt_gbufgen");
			OGL_TRACE("rt_gbufgen");

			glUseProgram(rt_shad->prog);

			r.state.bind_textures(rt_shad, {
				{"subchunks_tex", subchunks_tex.tex},
				{"voxels_tex", voxels_tex.tex},
				{"octree", octree.tex},

				{"tile_textures", r.tile_textures, r.tile_sampler},
			});

			glBindImageTexture(0, gbuf.pos  , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
			glBindImageTexture(1, gbuf.col  , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glBindImageTexture(2, gbuf.norm , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			glBindImageTexture(3, gbuf.tang , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		
			int2 dispatch_size;
			dispatch_size.x = (r.framebuffer.size.x + (_group_size.x -1)) / _group_size.x;
			dispatch_size.y = (r.framebuffer.size.y + (_group_size.y -1)) / _group_size.y;
		
			glDispatchCompute(dispatch_size.x, dispatch_size.y, 1);

			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}
	}

	{
		ZoneScopedN("vct_shade");
		OGL_TRACE("vct_shade");

		glUseProgram(vct_shad->prog);

		vct_shad->set_uniform("visualize_light",    visualize_light);

		vct_shad->set_uniform("vct_start_dist",     vct_start_dist);
		vct_shad->set_uniform("vct_stepsize",       vct_stepsize);
		vct_shad->set_uniform("vct_test",           vct_test);

		vct_shad->set_uniform("update_debugdraw",  update_debugdraw);

		r.state.bind_textures(vct_shad, {
			{"subchunks_tex", subchunks_tex.tex},
			{"voxels_tex", voxels_tex.tex},
			{"octree", octree.tex},

			{"gbuf_pos" , gbuf.pos },
			{"gbuf_col" , gbuf.col },
			{"gbuf_norm", gbuf.norm},
			{"gbuf_tang", gbuf.tang},

			{"vct_basetex", vct_data.basetex.tex, vct_data.sampler},
			{"vct_preint", vct_data.preint.tex, vct_data.sampler},

			{"tile_textures", r.tile_textures, r.tile_sampler},
			
			{"heat_gradient", r.gradient, r.normal_sampler},
		});

		upload_bind_ubo(cones_ubo, 4, &cone_data, sizeof(cone_data));

		glBindImageTexture(0, r.framebuffer.color, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

		int2 dispatch_size;
		dispatch_size.x = (r.framebuffer.size.x + (_group_size.x -1)) / _group_size.x;
		dispatch_size.y = (r.framebuffer.size.y + (_group_size.y -1)) / _group_size.y;

		glDispatchCompute(dispatch_size.x, dispatch_size.y, 1);
	}
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// unbind
	glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glBindImageTexture(1, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glBindImageTexture(2, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glBindImageTexture(3, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
}

} // namespace gl
