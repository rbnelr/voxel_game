#include "common.hpp"
#include "gl_raytracer.hpp"
#include "chunks.hpp"
#include "chunk_mesher.hpp"
#include "opengl_renderer.hpp"

#include "engine/window.hpp" // for frame_counter hack

namespace gl {
	
	//
	lrgba cols[] = {
		{1,0,0,1},
		{0,1,0,1},
		{0,0,1,1},
		{1,1,0,1},
		{1,0,1,1},
		{0,1,1,1},
	};
	bool Raytracer::vct_conedev (OpenglRenderer& r, Game& game) {
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

		bool count_changed = false;

		int j=0;
		for (auto& s : sets) {
			ImGui::TreeNodeEx(&s, ImGuiTreeNodeFlags_DefaultOpen, "Set");

			count_changed = ImGui::SliderInt("count", &s.count, 0, 16) || count_changed;
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

		return count_changed;
	}


	void Raytracer::update (OpenglRenderer& r, Game& game, Input& I) {
		ZoneScoped;

		if (renderscale.update(r.render_size)) {
			taa.resize(renderscale.size);
			gbuf.resize(renderscale.size);
			framebuf.resize(renderscale.size, renderscale.nearest);
		}

		macro_change |= vct_conedev(r, game);
		upload_bind_ubo(cones_ubo, 4, &cone_data, sizeof(cone_data));


		// lazy init these (instead of doing it in ctor) to allow json changes to affect the macros
		// this would not be needed in a sane programming language (reflection support)
		if (!rt_forward ) rt_forward  = r.shaders.compile("rt_forward",  get_macros(), {{ COMPUTE_SHADER }});
		if (!rt_lighting) rt_lighting = r.shaders.compile("rt_lighting", get_macros(), {{ COMPUTE_SHADER }});
		if (!rt_post    ) rt_post     = r.shaders.compile("rt_post");

		//
		
		if (I.buttons[KEY_R].went_down)
			enable = !enable;

		if (I.buttons[KEY_V].went_down) {
			lighting.vct = !lighting.vct;
			macro_change = true;
		}

		if (macro_change && rt_forward) {
			rt_forward->macros = get_macros();
			rt_forward->recompile("macro_change", false);
		}
		if (macro_change && rt_lighting) {
			rt_lighting->macros = get_macros();
			rt_lighting->recompile("macro_change", false);
		}
		if (macro_change && rt_post) {
			rt_post->recompile("macro_change", false);
		}
		macro_change = false;

		upload_changes(r, game);
	}


	void VCT_Data::recompute_mips (OpenglRenderer& r, Game& game, std::vector<int3> const& chunks) {
		if (chunks.empty())
			return;
		ZoneScoped;

		OGL_TRACE("vct.recompute_mips");

		//if (0) { // commit sparse pages
		//	ZoneScopedN("commit sparse pages");
		//
		//	assert(all(sparse_size > SUBCHUNK_SIZE));
		//
		//	int3 pages_per_chunk = CHUNK_SIZE / sparse_size;
		//	int3 subchunks_per_page = sparse_size / SUBCHUNK_SIZE;
		//	auto air = SUBC_SPARSE_BIT | g_assets.block_types.map_id("air");
		//
		//	for (auto& chunk_pos : chunks) {
		//		auto chunk_id = game.chunks.query_chunk(chunk_pos - GPU_WORLD_SIZE_CHUNKS/2);
		//		auto& vox = game.chunks.chunk_voxels[chunk_id];
		//
		//		// for page in texture for chunk
		//		for (int pz=0; pz<pages_per_chunk.z; ++pz)
		//		for (int py=0; py<pages_per_chunk.y; ++py)
		//		for (int px=0; px<pages_per_chunk.x; ++px) {
		//
		//			int3 idx = chunk_pos * pages_per_chunk + int3(px,py,pz);
		//			bool page_sparse = sparse_page_state[idx];
		//
		//			if (page_sparse) {
		//				int3 offs = idx * sparse_size;
		//				for (int dir=0; dir<6; ++dir)
		//					glTexturePageCommitmentEXT(textures[dir].tex, 0, offs.x,offs.y,offs.z, sparse_size.x,sparse_size.y,sparse_size.z, true);
		//			}
		//		}
		//	}
		//}

		// Each mip gets generated by (multiple) compute shader dispatches
		// if multiple chunks are uploaded they are batched into groups of 16,
		// so usually only one dispatch per mip is required

		static constexpr int BATCHSIZE = 16;
		int3 offsets[BATCHSIZE];

		auto dispatch_chunked = [&] (int layer, Shader* shad) {
			int size = CHUNK_SIZE >> layer;
			shad->set_uniform("size", (GLuint)size);

			for (int dir=0; dir<6; ++dir)
				glBindImageTexture(dir, textures[dir].texview, layer, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8UI);

			if (layer >= 0)
				shad->set_uniform("read_mip", layer-1);

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
			shad->set_uniform("read_mip", layer-1);

			for (int dir=0; dir<6; ++dir)
				glBindImageTexture(dir, textures[dir].texview, layer, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8UI);

			memset(offsets, 0, sizeof(offsets));
			glUniform3uiv(shad->get_uniform_location("offsets[0]"), ARRLEN(offsets), (GLuint*)offsets);

			int dispatch_size = (size + COMPUTE_FILTER_LOCAL_SIZE-1) / COMPUTE_FILTER_LOCAL_SIZE;
			glDispatchCompute(dispatch_size, dispatch_size, dispatch_size);

			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
		};

		{ // layer 0, generate texture from voxel data in voxel textures
			glUseProgram(filter_mip0->prog);
			r.state.bind_textures(filter_mip0, {
				{"voxel_tex", r.raytracer.voxel_tex.tex},
				//{"df_tex",    r.raytracer.df_tex.tex},
				{"tile_textures", r.tile_textures, r.pixelated_sampler},
			});

			dispatch_chunked(0, filter_mip0);
		}

		{ // layer 1+
			glUseProgram(filter->prog);

			r.state.bind_textures(filter, {
				{"vct_texNX", textures[0].tex, sampler },
				{"vct_texPX", textures[1].tex, sampler },
				{"vct_texNY", textures[2].tex, sampler },
				{"vct_texPY", textures[3].tex, sampler },
				{"vct_texNZ", textures[4].tex, sampler },
				{"vct_texPZ", textures[5].tex, sampler },
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
		for (int dir=0; dir<6; ++dir)
			glBindImageTexture(dir, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);

		//if (0) { // decommit sparse pages
		//	ZoneScopedN("decommit sparse pages");
		//
		//	assert(all(sparse_size > SUBCHUNK_SIZE));
		//
		//	int3 pages_per_chunk = CHUNK_SIZE / sparse_size;
		//	int3 subchunks_per_page = sparse_size / SUBCHUNK_SIZE;
		//	auto air = SUBC_SPARSE_BIT | g_assets.block_types.map_id("air");
		//
		//	for (auto& chunk_pos : chunks) {
		//		auto chunk_id = game.chunks.query_chunk(chunk_pos - GPU_WORLD_SIZE_CHUNKS/2);
		//		auto& vox = game.chunks.chunk_voxels[chunk_id];
		//
		//		// for page in texture for chunk
		//		for (int pz=0; pz<pages_per_chunk.z; ++pz)
		//		for (int py=0; py<pages_per_chunk.y; ++py)
		//		for (int px=0; px<pages_per_chunk.x; ++px) {
		//
		//			bool sparse = true;
		//
		//			// for subchunks in page
		//			for (int sz=0; sz<subchunks_per_page.z; ++sz)
		//			for (int sy=0; sy<subchunks_per_page.y; ++sy)
		//			for (int sx=0; sx<subchunks_per_page.x; ++sx) {
		//
		//				int3 idx = int3(px,py,pz) * subchunks_per_page + int3(sx,sy,sz);
		//				auto aubc = vox.subchunks[IDX3D(idx.x,idx.y,idx.z, SUBCHUNK_COUNT)];
		//				bool is_air = aubc == air;
		//				if (!is_air) {
		//					sparse = false;
		//					goto end;
		//				}
		//			} end:;
		//
		//			int3 idx = chunk_pos * pages_per_chunk + int3(px,py,pz);
		//			bool& page_sparse = sparse_page_state[idx];
		//			page_sparse = sparse;
		//
		//			if (sparse) {
		//				int3 offs = idx * sparse_size;
		//				for (int dir=0; dir<6; ++dir)
		//					glTexturePageCommitmentEXT(textures[dir].tex, 0, offs.x,offs.y,offs.z, sparse_size.x,sparse_size.y,sparse_size.z, false);
		//			}
		//		}
		//	}
		//}
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

	void Raytracer::upload_changes (OpenglRenderer& r, Game& game) {
		ZoneScoped;

		std::vector<int3> chunks;

		if (!game.chunks.upload_voxels.empty()) {
			OGL_TRACE("raytracer upload changes");

			// temp buffer to 'decompress' my sparse subchunks and enable uploading them in a single glTextureSubImage3D per chunk
			block_id buffer[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
			
			for (auto cid : game.chunks.upload_voxels) {
				auto& chunk = game.chunks.chunks[cid];
				auto& vox = game.chunks.chunk_voxels[cid];

				int3 pos = chunk.pos + GPU_WORLD_SIZE_CHUNKS/2;
				if ( (unsigned)(pos.x) < GPU_WORLD_SIZE_CHUNKS && // use 3 unsigned comparisons instead of 6 signed ones
					 (unsigned)(pos.y) < GPU_WORLD_SIZE_CHUNKS &&
					 (unsigned)(pos.z) < GPU_WORLD_SIZE_CHUNKS ) {
					//OGL_TRACE("upload chunk data");

					{
						ZoneScopedN("decompress");

						for (int sz=0; sz<SUBCHUNK_COUNT; ++sz)
						for (int sy=0; sy<SUBCHUNK_COUNT; ++sy)
						for (int sx=0; sx<SUBCHUNK_COUNT; ++sx) {

							auto subc = vox.subchunks[IDX3D(sx,sy,sz, SUBCHUNK_SIZE)];
							if (subc & SUBC_SPARSE_BIT) {
								block_id val = (block_id)(subc & ~SUBC_SPARSE_BIT);
							
								block_id val_packed[SUBCHUNK_SIZE];
								for (int i=0; i<SUBCHUNK_SIZE; ++i)
									val_packed[i] = val;
							
								for (int z=0; z<SUBCHUNK_SIZE; ++z)
								for (int y=0; y<SUBCHUNK_SIZE; ++y) {
									auto* dst = &buffer[sz*SUBCHUNK_SIZE + z][sy*SUBCHUNK_SIZE + y][sx*SUBCHUNK_SIZE + 0];
									memcpy(dst, val_packed, sizeof(block_id)*SUBCHUNK_SIZE);
								}
							
							} else {
								auto* data = game.chunks.subchunks[subc].voxels;
							
								for (int z=0; z<SUBCHUNK_SIZE; ++z)
								for (int y=0; y<SUBCHUNK_SIZE; ++y) {
									auto* dst = &buffer[sz*SUBCHUNK_SIZE + z][sy*SUBCHUNK_SIZE + y][sx*SUBCHUNK_SIZE + 0];
									auto* src = &data[IDX3D(0,y,z, SUBCHUNK_SIZE)];
									memcpy(dst, src, sizeof(block_id)*SUBCHUNK_SIZE);
								}
							}
						}
					}

					{
						ZoneScopedN("glTextureSubImage3D");

						glTextureSubImage3D(voxel_tex.tex, 0,
							pos.x*CHUNK_SIZE, pos.y*CHUNK_SIZE, pos.z*CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE,
							GL_RED_INTEGER, GL_UNSIGNED_SHORT, buffer);
					}

					chunks.push_back(pos);
				}
			}
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);

		}
		{
			//chunks.clear(); // for profiling df gen
			//for (int y=0; y<4; ++y)
			//for (int x=0; x<4; ++x) {
			//	chunks.push_back({x,y,7});
			//}

			if (!chunks.empty()) {
				ZoneScopedN("rt_df_gen");
				OGL_TRACE("rt_df_gen");
				OGL_TIMER_ZONE(timer_df_init.timer);

				int count = (int)chunks.size();

				glBindImageTexture(4, df_tex.tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R8I);

				{
					glUseProgram(df_tex.shad_init->prog);

					r.state.bind_textures(df_tex.shad_init, {
						{"voxel_tex", voxel_tex.tex},
					});

					static constexpr int BATCHSIZE = 32;
					for (int i=0; i<count; i+=BATCHSIZE) {
						int subcount = min(count - i, BATCHSIZE);

						int3 offsets[BATCHSIZE] = {};
						for (int j=0; j<subcount; ++j)
							offsets[j] = chunks[i+j] * CHUNK_SIZE;

						df_tex.shad_init->set_uniform_array("offsets[0]", offsets, BATCHSIZE);

						constexpr int REGION = 8;
						constexpr int CORE = REGION -2;
						constexpr int CHUNK_WGROUPS = (CHUNK_SIZE + CORE-1) / CORE; // round up

						glDispatchCompute(CHUNK_WGROUPS, CHUNK_WGROUPS, CHUNK_WGROUPS * subcount);
					}

					glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
				}
				for (int pass=0; pass<3; ++pass) {
					Shader* shad = df_tex.shad_pass[pass];
				
					glUseProgram(shad->prog);
					r.state.bind_textures(shad, {});
				
					static constexpr int BATCHSIZE = 32;
					for (int i=0; i<count; i+=BATCHSIZE) {
						int subcount = min(count - i, BATCHSIZE);
				
						int3 offsets[BATCHSIZE] = {};
						for (int j=0; j<subcount; ++j)
							offsets[j] = chunks[i+j] * CHUNK_SIZE;
				
						shad->set_uniform_array("offsets[0]", offsets, BATCHSIZE);
				
						int dispatch_size = (CHUNK_SIZE + DFTexture::COMPUTE_GROUPSZ -1) / DFTexture::COMPUTE_GROUPSZ;
						glDispatchCompute(dispatch_size, dispatch_size, subcount);
					}
				
					glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
				}
			}

			glBindImageTexture(4, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8); // unbind
		}
		vct_data.recompute_mips(r, game, chunks);
	}

	void Raytracer::set_uniforms (OpenglRenderer& r, Game& game, Shader* shad) {
		shad->set_uniform("rand_seed_time", rand_seed_time ? g_window.frame_counter : 0);

		shad->set_uniform("framebuf_size", renderscale.size);
		shad->set_uniform("update_debugdraw", r.debug_draw.update_indirect);

		float base_px_size;
		{
			// frustrum_size (world-space size of near plane) / clip_near -> world-space width of frustrum at 1m z dist
			// pixels / (frustrum_size / clip_near) -> pixels that a ~1m object has at 1m
			// divide this by any object's z-distance to get a pixel size for LOD purposes
			base_px_size = (float)renderscale.size.x / game.view.frustrum_size.x * game.view.clip_near;
		}
		shad->set_uniform("base_px_size", base_px_size);

		shad->set_uniform("max_iterations", max_iterations);

		shad->set_uniform("visualize_mult", visualize_mult);

		shad->set_uniform("show_light", lighting.show_light);
		shad->set_uniform("show_normals", lighting.show_normals);

		shad->set_uniform("bounce_max_dist", lighting.bounce_max_dist);
		shad->set_uniform("bounce_max_count", lighting.bounce_max_count);
		shad->set_uniform("bounce_samples", lighting.bounce_samples);

		shad->set_uniform("roughness", lighting.roughness);

		shad->set_uniform("vct_primary_cone_width", lighting.vct_primary_cone_width);
		shad->set_uniform("vct_min_start_dist", lighting.vct_min_start_dist);

		shad->set_uniform("test", lighting.test);
	}
	void Raytracer::draw (OpenglRenderer& r, Game& game) {
		ZoneScoped;
		if (!rt_forward->prog || !rt_lighting->prog || !rt_post->prog) return;
		OGL_TIMER_ZONE(timer_rt_total.timer);
		
		r.update_view(game.view, renderscale.size);

		{ // forward pass -> writes to gbuf
			
			{ // forward voxel raycast pass
				ZoneScopedN("rt_shad");
				OGL_TRACE("rt_shad");
				OGL_TIMER_ZONE(timer_rt_forward.timer);
				
				glUseProgram(rt_forward->prog);
				
				set_uniforms(r, game, rt_forward);
				
				r.state.bind_textures(rt_forward, {
					{"voxel_tex", voxel_tex.tex},
					{"df_tex", df_tex.tex},
					
					{"tile_textures", r.tile_textures, r.pixelated_sampler},
					
					{"heat_gradient", r.gradient, r.smooth_sampler},
				});
				
				glBindImageTexture(0, gbuf.pos , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
				glBindImageTexture(1, gbuf.col , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
				glBindImageTexture(2, gbuf.norm, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

				int2 dispatch_size;
				dispatch_size.x = (renderscale.size.x + rt_groupsz.size.x -1) / rt_groupsz.size.x;
				dispatch_size.y = (renderscale.size.y + rt_groupsz.size.y -1) / rt_groupsz.size.y;
				glDispatchCompute(dispatch_size.x, dispatch_size.y, 1);
				
				for (int i=0; i<3; ++i)
					glBindImageTexture(i, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
			}
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
			
			//// normal rasterized drawing pass for gbuf testing
			//glClear(GL_DEPTH_BUFFER_BIT);
			//test_renderer.draw(r);
		}

		{ // deferred lighting pass  gbuf -> lit image
			ZoneScopedN("rt_lighting");
			OGL_TRACE("rt_lighting");
			OGL_TIMER_ZONE(timer_rt_lighting.timer);
		
			glUseProgram(rt_lighting->prog);
		
			set_uniforms(r, game, rt_lighting);
		
			glBindImageTexture(0, framebuf.col, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		
			// TAA
			GLuint prev_color  = taa.colors[taa.cur ^ 1];
			GLuint prev_posage = taa.posage[taa.cur ^ 1];
			GLuint cur_color   = taa.colors[taa.cur];
			GLuint cur_posage  = taa.posage[taa.cur];
		
			if (taa.enable) {
				rt_lighting->set_uniform("prev_world2clip", taa.prev_world2clip); // invalid on first frame, should be ok since history age = 0
				rt_lighting->set_uniform("taa_max_age", taa.max_age);
		
				glBindImageTexture(1, cur_color , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F );
				glBindImageTexture(2, cur_posage, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16UI);
		
				taa.prev_world2clip = game.view.cam_to_clip * (float4x4)game.view.world_to_cam;
				taa.cur ^= 1;
			}
		
			r.state.bind_textures(rt_lighting, {
				{"voxel_tex", voxel_tex.tex},
				{"df_tex", df_tex.tex},

				{"vct_texNX", vct_data.textures[0].tex, vct_data.sampler },
				{"vct_texPX", vct_data.textures[1].tex, vct_data.sampler },
				{"vct_texNY", vct_data.textures[2].tex, vct_data.sampler },
				{"vct_texPY", vct_data.textures[3].tex, vct_data.sampler },
				{"vct_texNZ", vct_data.textures[4].tex, vct_data.sampler },
				{"vct_texPZ", vct_data.textures[5].tex, vct_data.sampler },
		
				{"gbuf_pos" ,  gbuf.pos  },
				{"gbuf_col" ,  gbuf.col  },
				{"gbuf_norm",  gbuf.norm },
				
				(taa.enable ? StateManager::TextureBind{"taa_history_color", {GL_TEXTURE_2D, prev_color}, taa.sampler} : StateManager::TextureBind{}),
				(taa.enable ? StateManager::TextureBind{"taa_history_posage", {GL_TEXTURE_2D, prev_posage}, taa.sampler_int} : StateManager::TextureBind{}),
		
				{"tile_textures", r.tile_textures, r.pixelated_sampler},
				
				{"heat_gradient", r.gradient, r.smooth_sampler},
			});
		
			int2 dispatch_size;
			dispatch_size.x = (renderscale.size.x + rt_groupsz.size.x -1) / rt_groupsz.size.x;
			dispatch_size.y = (renderscale.size.y + rt_groupsz.size.y -1) / rt_groupsz.size.y;
		
			glDispatchCompute(dispatch_size.x, dispatch_size.y, 1);

			for (int i=0; i<3; ++i)
				glBindImageTexture(i, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		}
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
			
		r.update_view(game.view, r.render_size);

		{ // rescale image
			OGL_TIMER_ZONE(timer_rt_post.timer);
			ZoneScopedN("rt_post");
			OGL_TRACE("rt_post");
			
			PipelineState s;
			s.depth_test = false;
			s.depth_write = false;
			s.blend_enable = false;
			r.state.set(s);

			glUseProgram(rt_post->prog);

			set_uniforms(r, game, rt_post);
			rt_post->set_uniform("exposure", lighting.post_exposure);

			r.state.bind_textures(rt_post, {
				{"gbuf_col", framebuf.col, framebuf.sampler},
			});

			glBindVertexArray(r.dummy_vao);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}

		//// try to all potentially bound textures
		//for (int i=0; i<3; ++i)
		//	glBindImageTexture(i, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	}

} // namespace gl
