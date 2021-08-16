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
		if (!rt_forward) rt_forward = r.shaders.compile("rt_forward", get_forward_macros(), {{ COMPUTE_SHADER }});

		//

		if (I.buttons[KEY_R].went_down)
			enable = !enable;

		if (macro_change && rt_forward) {
			rt_forward->macros = get_forward_macros();
			rt_forward->recompile("macro_change", false);
		}
		macro_change = false;

		upload_changes(r, game);
	}

	void Raytracer::upload_changes (OpenglRenderer& r, Game& game) {
		ZoneScoped;

	#if 0
		{
			ImGui::Begin("DFTest");

			static int3 coord = int3(-21,-85,219);
			static int radius = 5;
			ImGui::DragInt3("coord", &coord.x, 0.1f);
			ImGui::DragInt("radius", &radius, 0.1f);

			array3D<int2> grid0 (int3(16,16,1));
			array3D<int2> grid1 (int3(16,16,1));
			array3D<int2> grid_test (int3(16,16,1));

			g_debugdraw.wire_cube((float3)coord + (float3)grid0.size/2, (float3)grid0.size, lrgba(1,0,0,1));

			int iNULL = 999;

			for (int y=0; y<grid0.size.y; ++y)
			for (int x=0; x<grid0.size.x; ++x) {

				auto bid = game.chunks.read_block(coord.x + x, coord.y + y, coord.z);

				grid0.get(x,y,0) = bid > 1 ? 0 : iNULL;
			}


			for (int x=0; x<grid0.size.x; ++x) {
				for (int y=0; y<grid0.size.y; ++y) {
					int2 a = grid0.get_or_default(x-1, y-1, 0, iNULL);
					int2 b = grid0.get_or_default(x-1, y  , 0, iNULL);
					int2 c = grid0.get_or_default(x-1, y+1, 0, iNULL);

					int2 d = grid0.get_or_default(x, y, 0, iNULL);

					if (a.x > 0) a = iNULL;
					if (b.x > 0) b = iNULL;
					if (c.x > 0) c = iNULL;

					if (a.x != iNULL) a += int2(-1, -1);
					if (b.x != iNULL) b += int2(-1,  0);
					if (c.x != iNULL) c += int2(-1, +1);

					int ad = a.x*a.x + a.y*a.y;
					int bd = b.x*b.x + b.y*b.y;
					int cd = c.x*c.x + c.y*c.y;
					int dd = d.x*d.x + d.y*d.y;

					int dist = min(min(ad, bd), min(cd, dd));

					int2 val;
					if      (ad == dist) val = a;
					else if (bd == dist) val = b;
					else if (cd == dist) val = c;
					else                 val = d;

					grid0.get(x,y,0) = val;
				}
			}

			for (int x=grid0.size.x-1; x>=0; --x) {
				for (int y=0; y<grid0.size.y; ++y) {
					int2 a = grid0.get_or_default(x+1, y-1, 0, iNULL);
					int2 b = grid0.get_or_default(x+1, y  , 0, iNULL);
					int2 c = grid0.get_or_default(x+1, y+1, 0, iNULL);

					int2 d = grid0.get_or_default(x, y, 0, iNULL);
					
					if (a.x < 0) a = iNULL;
					if (b.x < 0) b = iNULL;
					if (c.x < 0) c = iNULL;

					if (a.x != iNULL) a += int2(+1, -1);
					if (b.x != iNULL) b += int2(+1,  0);
					if (c.x != iNULL) c += int2(+1, +1);

					int ad = a.x*a.x + a.y*a.y;
					int bd = b.x*b.x + b.y*b.y;
					int cd = c.x*c.x + c.y*c.y;
					int dd = d.x*d.x + d.y*d.y;
			
					int dist = min(min(ad, bd), min(cd, dd));
			
					int2 val;
					if      (ad == dist) val = a;
					else if (bd == dist) val = b;
					else if (cd == dist) val = c;
					else                 val = d;
			
					grid0.get(x,y,0) = val;
				}
			}

			for (int y=0; y<grid0.size.y; ++y)
			for (int x=0; x<grid0.size.x; ++x) {
				grid1.get(x,y,0) = grid0.get(x,y,0);
			}

			for (int y=0; y<grid1.size.y; ++y) {
				for (int x=0; x<grid1.size.x; ++x) {
					int2 a = grid1.get_or_default(x-1, y-1, 0, iNULL);
					int2 b = grid1.get_or_default(x  , y-1, 0, iNULL);
					int2 c = grid1.get_or_default(x+1, y-1, 0, iNULL);

					int2 d = grid1.get_or_default(x, y, 0, iNULL);

					if (a.y > 0) a = iNULL;
					if (b.y > 0) b = iNULL;
					if (c.y > 0) c = iNULL;

					if (a.x != iNULL) a += int2(-1, -1);
					if (b.x != iNULL) b += int2( 0, -1);
					if (c.x != iNULL) c += int2(+1, -1);

					int ad = a.x*a.x + a.y*a.y;
					int bd = b.x*b.x + b.y*b.y;
					int cd = c.x*c.x + c.y*c.y;
					int dd = d.x*d.x + d.y*d.y;

					int dist = min(min(ad, bd), min(cd, dd));

					int2 val;
					if      (ad == dist) val = a;
					else if (bd == dist) val = b;
					else if (cd == dist) val = c;
					else                 val = d;

					grid1.get(x,y,0) = val;
				}
			}

			for (int y=grid1.size.y-1; y>=0; --y) {
				for (int x=0; x<grid0.size.x; ++x) {
					int2 a = grid1.get_or_default(x-1, y+1, 0, iNULL);
					int2 b = grid1.get_or_default(x  , y+1, 0, iNULL);
					int2 c = grid1.get_or_default(x+1, y+1, 0, iNULL);
			
					int2 d = grid1.get_or_default(x, y, 0, iNULL);
					
					if (a.y < 0) a = iNULL;
					if (b.y < 0) b = iNULL;
					if (c.y < 0) c = iNULL;

					if (a.x != iNULL) a += int2(-1, +1);
					if (b.x != iNULL) b += int2( 0, +1);
					if (c.x != iNULL) c += int2(+1, +1);

					int ad = a.x*a.x + a.y*a.y;
					int bd = b.x*b.x + b.y*b.y;
					int cd = c.x*c.x + c.y*c.y;
					int dd = d.x*d.x + d.y*d.y;

					int dist = min(min(ad, bd), min(cd, dd));

					int2 val;
					if      (ad == dist) val = a;
					else if (bd == dist) val = b;
					else if (cd == dist) val = c;
					else                 val = d;
			
					grid1.get(x,y,0) = val;
				}
			}


			// Brute force version
			for (int y=0; y<grid_test.size.y; ++y)
			for (int x=0; x<grid_test.size.x; ++x) {

				int2 min_offs = iNULL;
				int min_dist_sqr = iNULL;
				
				for (int cy=0; cy<grid_test.size.y; ++cy)
				for (int cx=0; cx<grid_test.size.x; ++cx) {
					auto bid = game.chunks.read_block(coord.x + cx, coord.y + cy, coord.z);
				
					if (bid > 1) {
						int2 offs = int2(cx-x, cy-y);
						int dist_sqr = offs.x*offs.x + offs.y*offs.y;
						if (dist_sqr <= min_dist_sqr) {
							min_dist_sqr = dist_sqr;
							min_offs = offs;
						}
					}
				}

				grid_test.get(x,y,0) = min_offs;
			}

			ImGui::Text("X-Pass");
			if (ImGui::BeginTable("X-Pass", grid0.size.x, ImGuiTableFlags_Borders)) {
				for (int y=grid0.size.y-1; y>=0; --y) {
					ImGui::TableNextRow();
					for (int x=0; x<grid0.size.x; ++x) {
						int2 offs = grid0.get(x,y,0);

						ImGui::TableNextColumn();
						ImGui::Text(offs.x >= iNULL ? "":"%d,%d", offs.x,offs.y);

						float dist = length((float2)offs);
						if (offs != iNULL) {
							ImU32 red32   = ImGui::GetColorU32(ImVec4(1.0f - dist / ((float)radius*1.44f), 0, 0, 1));
							ImU32 solid32 = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
							ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, offs != 0 ? red32 : solid32);
						}
					}
				}
				ImGui::EndTable();
			}

			ImGui::Text("Y-Pass");
			if (ImGui::BeginTable("Y-Pass", grid1.size.x, ImGuiTableFlags_Borders)) {
				for (int y=grid1.size.y-1; y>=0; --y) {
					ImGui::TableNextRow();
					for (int x=0; x<grid1.size.x; ++x) {
						int2 offs = grid1.get(x,y,0);
			
						ImGui::TableNextColumn();
						ImGui::Text(offs.x >= iNULL ? "":"%d,%d", offs.x,offs.y);
						
						float dist = length((float2)offs);
						if (offs != iNULL) {
							ImU32 red32   = ImGui::GetColorU32(ImVec4(1.0f - dist / ((float)radius*1.44f), 0, 0, 1));
							ImU32 solid32 = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
							ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, offs != 0 ? red32 : solid32);
						}
					}
				}
				ImGui::EndTable();
			}

			ImGui::Text("Result");
			if (ImGui::BeginTable("Result", grid1.size.x, ImGuiTableFlags_Borders)) {
				for (int y=grid1.size.y-1; y>=0; --y) {
					ImGui::TableNextRow();
					for (int x=0; x<grid1.size.x; ++x) {
						int2 offs = grid1.get(x,y,0);
						float dist = length((float2)offs);

						ImGui::TableNextColumn();
						ImGui::Text(offs.x >= iNULL ? "":"%.2f", dist);

						if (offs != iNULL) {
							ImU32 red32   = ImGui::GetColorU32(ImVec4(1.0f - dist / ((float)radius*1.44f), 0, 0, 1));
							ImU32 solid32 = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
							ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, offs != 0 ? red32 : solid32);
						}
					}
				}
				ImGui::EndTable();
			}

			ImGui::Text("Brute Force Version");
			if (ImGui::BeginTable("Brute Force Version", grid_test.size.x, ImGuiTableFlags_Borders)) {
				for (int y=grid_test.size.y-1; y>=0; --y) {
					ImGui::TableNextRow();
					for (int x=0; x<grid_test.size.x; ++x) {
						int2 offs = grid_test.get(x,y,0);
						int2 offs2 = grid1.get(x,y,0);

						float dist = length((float2)offs);
						float dist2 = length((float2)offs2);

						bool error = abs(dist - dist2) > 0.001f;

						ImGui::TableNextColumn();
						ImGui::Text(offs.x >= iNULL ? "":"%.2f", dist);

						if (offs != iNULL) {
							ImU32 red32   = ImGui::GetColorU32(ImVec4(1.0f - dist / ((float)radius*1.44f), 0, 0, 1));
							ImU32 solid32 = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
							ImU32 error32 = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 1.0f, 1.0f));
							ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, error ? error32 : (offs != 0 ? red32 : solid32));
						}
					}
				}
				ImGui::EndTable();
			}

			ImGui::End();
		}
	#endif

		std::vector<int3> chunks;

		if (!game.chunks.upload_voxels.empty()) {
			OGL_TRACE("raytracer upload changes");

			block_id buffer[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE]; // temp buffer to 'decompress' the data and enable uploading it in a single glTextureSubImage3D

			for (auto cid : game.chunks.upload_voxels) {
				auto& chunk = game.chunks.chunks[cid];
				auto& vox = game.chunks.chunk_voxels[cid];

				int3 pos = chunk.pos + GPU_WORLD_SIZE_CHUNKS/2;
				if ( (unsigned)(pos.x) < GPU_WORLD_SIZE_CHUNKS &&
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

			if (!chunks.empty()) {
				ZoneScopedN("rt_df_gen");
				OGL_TRACE("rt_df_gen");

				int count = (int)chunks.size();

				for (int pass=0; pass<3; ++pass) {
					Shader* shad = df_tex.shad_pass[pass];

					glUseProgram(shad->prog);

					r.state.bind_textures(shad, {
						{"voxel_tex", voxel_tex.tex},
					});
					glBindImageTexture(4, df_tex.tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R8UI);

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

	void Raytracer::draw (OpenglRenderer& r, Game& game) {
		ZoneScoped;
		if (!rt_forward->prog) return;

		{
			ZoneScopedN("rt_gbufgen");
			OGL_TRACE("rt_gbufgen");

			glUseProgram(rt_forward->prog);

			rt_forward->set_uniform("dispatch_size", r.framebuffer.size);
			rt_forward->set_uniform("update_debugdraw", r.debug_draw.update_indirect);

			rt_forward->set_uniform("max_iterations", max_iterations);

			r.state.bind_textures(rt_forward, {
				{"voxel_tex", voxel_tex.tex},
				{"df_tex", df_tex.tex},

				//{"gbuf_pos" , gbuf.pos },
				//{"gbuf_col" , gbuf.col },
				//{"gbuf_norm", gbuf.norm},

				{"tile_textures", r.tile_textures, r.tile_sampler},

				{"heat_gradient", r.gradient, r.normal_sampler},
			});

			glBindImageTexture(0, r.framebuffer.color, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			int2 dispatch_size;
			dispatch_size.x = (r.framebuffer.size.x + rt_groupsz.size.x -1) / rt_groupsz.size.x;
			dispatch_size.y = (r.framebuffer.size.y + rt_groupsz.size.y -1) / rt_groupsz.size.y;

			glDispatchCompute(dispatch_size.x, dispatch_size.y, 1);
		}
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);

		//{
		//	glBindFramebuffer(GL_FRAMEBUFFER, gbuf.fbo);
		//	glClear(GL_DEPTH_BUFFER_BIT);
		//
		//	test_renderer.draw(r);
		//}

		// unbind
		glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

	}

} // namespace gl
