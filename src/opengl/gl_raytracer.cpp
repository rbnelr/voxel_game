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

					for (int sz=0; sz<SUBCHUNK_COUNT; ++sz)
					for (int sy=0; sy<SUBCHUNK_COUNT; ++sy)
					for (int sx=0; sx<SUBCHUNK_COUNT; ++sx) {
						int coordx = pos.x * CHUNK_SIZE + sx*SUBCHUNK_SIZE;
						int coordy = pos.y * CHUNK_SIZE + sy*SUBCHUNK_SIZE;
						int coordz = pos.z * CHUNK_SIZE + sz*SUBCHUNK_SIZE;

						auto subc = vox.subchunks[IDX3D(sx,sy,sz, SUBCHUNK_SIZE)];
						if (subc & SUBC_SPARSE_BIT) {
							block_id val = (block_id)(subc & ~SUBC_SPARSE_BIT);

							glClearTexSubImage(voxel_tex.tex, 0,
								coordx, coordy, coordz, SUBCHUNK_SIZE, SUBCHUNK_SIZE, SUBCHUNK_SIZE,
								GL_RED_INTEGER, GL_UNSIGNED_SHORT, &val);

						} else {
							auto* data = &game.chunks.subchunks[subc].voxels;

							glTextureSubImage3D(voxel_tex.tex, 0,
								coordx, coordy, coordz, SUBCHUNK_SIZE, SUBCHUNK_SIZE, SUBCHUNK_SIZE,
								GL_RED_INTEGER, GL_UNSIGNED_SHORT, data);
						}
					}

					chunks.push_back(pos);
				}
			}
		}
	}

	void Raytracer::draw (OpenglRenderer& r, Game& game) {
		ZoneScoped;
		if (!rt_forward->prog) return;

		{
			ZoneScopedN("rt_gbufgen");
			OGL_TRACE("rt_gbufgen");

			glUseProgram(rt_forward->prog);

			//rt_forward->set_uniform("dispatch_size", (float2)r.framebuffer.size);
			rt_forward->set_uniform("max_iterations", max_iterations);

			r.state.bind_textures(rt_forward, {
				{"voxel_tex", voxel_tex.tex},
				//{"voxels_tex", voxels_tex.tex},

				//{"gbuf_pos" , gbuf.pos },
				//{"gbuf_col" , gbuf.col },
				//{"gbuf_norm", gbuf.norm},

				{"tile_textures", r.tile_textures, r.tile_sampler},

				{"heat_gradient", r.gradient, r.normal_sampler},
			});

			glBindImageTexture(0, r.framebuffer.color, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

			int2 dispatch_size;
			dispatch_size.x = (r.framebuffer.size.x + (rt_groupsz.size.x -1)) / rt_groupsz.size.x;
			dispatch_size.y = (r.framebuffer.size.y + (rt_groupsz.size.y -1)) / rt_groupsz.size.y;

			glDispatchCompute(dispatch_size.x, dispatch_size.y, 1);
		}
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

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
