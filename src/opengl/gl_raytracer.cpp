#include "common.hpp"
#include "gl_raytracer.hpp"
#include "chunks.hpp"
#include "chunk_mesher.hpp"
#include "opengl_renderer.hpp"

#include "engine/window.hpp" // for frame_counter hack

namespace gl {

	void TestRenderer::draw (OpenglRenderer& r) {
		ZoneScoped;
		OGL_TRACE("TestRenderer draw");

		PipelineState s;
		s.depth_test = true;
		s.blend_enable = false;
		r.state.set(s);

		glUseProgram(shad->prog);
		r.state.bind_textures(shad, {});

		float4x4 mat = (float4x4)translate(pos) * (float4x4)(rotate3_Z(rot.x) * rotate3_X(rot.y)) * (float4x4)scale((float3)size);
		shad->set_uniform("model2world", mat);

		glBindVertexArray(mesh.ib.vao);
		glDrawElements(GL_TRIANGLES, mesh.index_count, GL_UNSIGNED_INT, nullptr);
	}

////
	void Raytracer::update (OpenglRenderer& r, Game& game, Input& I) {
		ZoneScoped;

		// lazy init these (instead of doing it in ctor) to allow json changes to affect the macros
		// this would not be needed in a sane programming language (reflection support)
		if (!rt_forward ) rt_forward  = r.shaders.compile("rt_forward" , get_macros(true ), {{ COMPUTE_SHADER }});
		if (!rt_lighting) rt_lighting = r.shaders.compile("rt_lighting", get_macros(false), {{ COMPUTE_SHADER }});

		//

		if (I.buttons[KEY_R].went_down)
			enable = !enable;

		if (macro_change && rt_forward) {
			rt_forward ->macros = get_macros(true );
			rt_forward ->recompile("macro_change", false);
		}
		if (macro_change && rt_lighting) {
			rt_lighting->macros = get_macros(false);
			rt_lighting->recompile("macro_change", false);
		}
		macro_change = false;

		upload_changes(r, game);
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
	}

	void Raytracer::set_uniforms (OpenglRenderer& r, Game& game, Shader* shad) {
		shad->set_uniform("rand_seed_time", rand_seed_time ? g_window.frame_counter : 0);

		shad->set_uniform("framebuf_size", r.render_size);
		shad->set_uniform("update_debugdraw", r.debug_draw.update_indirect);

		float near_px_size;
		{
			// compute size of pixel in world space while on near plane (for knowing ray widths for AA)
			float4 a = game.view.clip_to_cam * float4(0,0,0,1); // center of screen in cam space
			float4 b = game.view.clip_to_cam * float4(float2(1.0f / (float2)r.render_size),0,1); // pixel one to the up/right in cam space
			near_px_size = b.x - a.x;
		}
		shad->set_uniform("near_px_size", near_px_size);

		shad->set_uniform("max_iterations", max_iterations);

		shad->set_uniform("visualize_mult", visualize_mult);

		shad->set_uniform("show_light", lighting.show_light);
		shad->set_uniform("show_normals", lighting.show_normals);

		shad->set_uniform("bounce_max_dist", lighting.bounce_max_dist);
		shad->set_uniform("bounce_max_count", lighting.bounce_max_count);
		shad->set_uniform("bounce_samples", lighting.bounce_samples);

		shad->set_uniform("roughness", lighting.roughness);

		shad->set_uniform("parallax_zstep",    lighting.parallax_zstep);
		shad->set_uniform("parallax_max_step", lighting.parallax_max_step);
		shad->set_uniform("parallax_scale",    lighting.parallax_scale);
	}
	void Raytracer::draw (OpenglRenderer& r, Game& game) {
		ZoneScoped;
		if (!rt_forward->prog) return;

		if (taa.enable) taa.resize(r.render_size);

		//{ // 'forward' gbuf pass
		//	OGL_TIMER_ZONE(timer_rt_total.timer);
		//
		//	{ // forward voxel raycast pass
		//		ZoneScopedN("rt_forward");
		//		OGL_TRACE("rt_forward");
		//		OGL_TIMER_ZONE(timer_rt_forward.timer);
		//
		//		glUseProgram(rt_forward->prog);
		//
		//		set_uniforms(r, game, rt_forward);
		//
		//		r.state.bind_textures(rt_forward, {
		//			{"voxel_tex", voxel_tex.tex},
		//			{"df_tex", df_tex.tex},
		//
		//			{"tile_textures", r.tile_textures, r.tile_sampler},
		//
		//			{"test_cubeN", r.test_cubeN, r.normal_sampler_wrap},
		//			{"test_cubeH", r.test_cubeH, r.normal_sampler_wrap},
		//
		//			{"heat_gradient", r.gradient, r.normal_sampler},
		//		});
		//
		//		//glBindImageTexture(0, gbuf.depth.tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		//		glBindImageTexture(1, gbuf.pos  , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		//		glBindImageTexture(2, gbuf.col  , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		//		glBindImageTexture(3, gbuf.norm , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		//
		//		int2 dispatch_size;
		//		dispatch_size.x = (r.framebuffer.size.x + rt_groupsz.size.x -1) / rt_groupsz.size.x;
		//		dispatch_size.y = (r.framebuffer.size.y + rt_groupsz.size.y -1) / rt_groupsz.size.y;
		//		glDispatchCompute(dispatch_size.x, dispatch_size.y, 1);
		//	}
		//	//glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT);
		//	//glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
		//	glMemoryBarrier(GL_ALL_BARRIER_BITS);
		//
		//	// normal rasterized drawing pass for gbuf testing
		//	glBindFramebuffer(GL_FRAMEBUFFER, gbuf.fbo);
		//	glClear(GL_DEPTH_BUFFER_BIT);
		//	test_renderer.draw(r);
		//
		//	glBindFramebuffer(GL_FRAMEBUFFER, 0);
		//}

		//{ // deferred lighting pass
		//	ZoneScopedN("rt_lighting");
		//	OGL_TRACE("rt_lighting");
		//	OGL_TIMER_ZONE(timer_rt_lighting.timer);
		//
		//	glUseProgram(rt_lighting->prog);
		//
		//	set_uniforms(r, game, rt_lighting);
		//
		//	glBindImageTexture(0, r.framebuffer.color, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		//
		//	// TAA
		//	GLuint prev_color  = taa.colors[taa.cur ^ 1];
		//	GLuint prev_posage = taa.posage[taa.cur ^ 1];
		//	GLuint cur_color   = taa.colors[taa.cur];
		//	GLuint cur_posage  = taa.posage[taa.cur];
		//
		//	if (taa.enable) {
		//		rt_forward->set_uniform("prev_world2clip", taa.prev_world2clip); // invalid on first frame, should be ok since history age = 0
		//		rt_forward->set_uniform("taa_max_age", taa.max_age);
		//
		//		glBindImageTexture(1, cur_color , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F );
		//		glBindImageTexture(2, cur_posage, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16UI);
		//
		//		taa.prev_world2clip = game.view.cam_to_clip * (float4x4)game.view.world_to_cam;
		//		taa.cur ^= 1;
		//	}
		//
		//	r.state.bind_textures(rt_lighting, {
		//		{"voxel_tex", voxel_tex.tex},
		//		{"df_tex", df_tex.tex},
		//
		//		{"gbuf_depth", gbuf.depth, gbuf.sampler},
		//		{"gbuf_pos" ,  gbuf.pos  , gbuf.sampler},
		//		{"gbuf_col" ,  gbuf.col  , gbuf.sampler},
		//		{"gbuf_norm",  gbuf.norm , gbuf.sampler},
		//
		//		(taa.enable ? StateManager::TextureBind{"taa_history_color", {GL_TEXTURE_2D, prev_color}, taa.sampler} : StateManager::TextureBind{}),
		//		(taa.enable ? StateManager::TextureBind{"taa_history_posage", {GL_TEXTURE_2D, prev_posage}, taa.sampler_int} : StateManager::TextureBind{}),
		//
		//		{"tile_textures", r.tile_textures, r.tile_sampler},
		//
		//		{"test_cubeN", r.test_cubeN, r.normal_sampler_wrap},
		//		{"test_cubeH", r.test_cubeH, r.normal_sampler_wrap},
		//
		//		{"heat_gradient", r.gradient, r.normal_sampler},
		//	});
		//
		//	int2 dispatch_size;
		//	dispatch_size.x = (r.framebuffer.size.x + rt_groupsz.size.x -1) / rt_groupsz.size.x;
		//	dispatch_size.y = (r.framebuffer.size.y + rt_groupsz.size.y -1) / rt_groupsz.size.y;
		//
		//	glDispatchCompute(dispatch_size.x, dispatch_size.y, 1);
		//}
		////glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT
		////	|GL_COMMAND_BARRIER_BIT); // for indirect draw
		//glMemoryBarrier(GL_ALL_BARRIER_BITS);
		//
		//// unbind
		//for (int i=0; i<4; ++i)
		//	glBindImageTexture(i, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	}

} // namespace gl
