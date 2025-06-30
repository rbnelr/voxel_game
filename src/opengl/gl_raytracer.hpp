#pragma once
#include "common.hpp"
#include "opengl_renderer.hpp"

namespace gl {
	struct Gbuffer {
		Texture2D pos    = {};
		Texture2D faceid = {};
		Texture2D col    = {};
		Texture2D norm   = {};

		void resize (int2 size) {
			glActiveTexture(GL_TEXTURE0);

			pos    = {"gbuf.pos"    }; // only depth -> reconstruct position
			faceid = {"gbuf.faceid" }; // u16 
			col    = {"gbuf.col"    }; // rgb albedo + emissive multiplier
			norm   = {"gbuf.norm"   }; // rgb normal

			glTextureStorage2D (pos , 1, GL_R32F, size.x, size.y);
			glTextureParameteri(pos, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(pos, GL_TEXTURE_MAX_LEVEL, 0);

			glTextureStorage2D (faceid , 1, GL_R16UI, size.x, size.y);
			glTextureParameteri(faceid, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(faceid, GL_TEXTURE_MAX_LEVEL, 0);

			glTextureStorage2D (col , 1, GL_RGBA16F, size.x, size.y);
			glTextureParameteri(col, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(col, GL_TEXTURE_MAX_LEVEL, 0);

			glTextureStorage2D (norm, 1, GL_RGBA16F, size.x, size.y);
			glTextureParameteri(norm, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(norm, GL_TEXTURE_MAX_LEVEL, 0);
		}
	};
	struct Framebuffer {
		Texture2D col  = {};
		Fbo fbo = {};

		Sampler sampler = {"RTBuf.sampler"};

		void resize (int2 size, bool nearest) {
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, nearest ? GL_NEAREST : GL_LINEAR);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glActiveTexture(GL_TEXTURE0);

			fbo   = {"RTBuf.fbo" };
			col   = {"RTBuf.col" };

			glTextureStorage2D(col , 1, GL_RGBA16F, size.x, size.y);
			glTextureParameteri(col, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(col, GL_TEXTURE_MAX_LEVEL, 0);
			
			glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, col, 0);
		
			//GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			//if (status != GL_FRAMEBUFFER_COMPLETE) {
			//	fprintf(stderr, "glCheckFramebufferStatus: %x\n", status);
			//}
		}
	};
	struct TemporalAA {
		SERIALIZE(TemporalAA, enable, max_age)

		Texture2D colors[2] = {};
		Texture2D posage[2] = {};
		int2   size = 0;
		int    cur = 0;

		float4x4 prev_world2clip = (float4x4)translate(float3(NAN)); // make prev matrix invalid on first frame

		Sampler sampler = {"TAA.sampler"};
		Sampler sampler_int = {"TAA.sampler_int"};

		bool enable = true;
		int max_age = 16;

		TemporalAA () {
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glSamplerParameteri(sampler_int, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glSamplerParameteri(sampler_int, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glSamplerParameteri(sampler_int, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(sampler_int, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}

		void resize (int2 new_size) {
			glActiveTexture(GL_TEXTURE0);

			size = new_size;

			// create new (textures created with glTexStorage2D cannot be resized)
			colors[0] = {"RT.taa_color0"};
			colors[1] = {"RT.taa_color1"};
			posage[0] = {"RT.taa_posage0"};
			posage[1] = {"RT.taa_posage1"};

			for (auto& buf : colors) {
				glTextureStorage2D(buf, 1, GL_RGBA16F, size.x, size.y);
				glTextureParameteri(buf, GL_TEXTURE_BASE_LEVEL, 0);
				glTextureParameteri(buf, GL_TEXTURE_MAX_LEVEL, 0);
			}

			for (auto& buf : posage) {
				glTextureStorage2D(buf, 1, GL_RG16UI, size.x, size.y);
				glTextureParameteri(buf, GL_TEXTURE_BASE_LEVEL, 0);
				glTextureParameteri(buf, GL_TEXTURE_MAX_LEVEL, 0);
			}

			// clear textures to be read on first frame
			float3 col = float3(0,0,0);
			glClearTexImage(colors[0], 0, GL_RGB, GL_FLOAT, &col.x);

			uint32_t pos[4] = { 0u, 0xffffu };
			glClearTexImage(posage[0], 0, GL_RG_INTEGER, GL_UNSIGNED_INT, pos);

			cur = 1;
		}
	};

	static constexpr int GPU_WORLD_SIZE_CHUNKS = 8;
	static constexpr int GPU_WORLD_SIZE = GPU_WORLD_SIZE_CHUNKS * CHUNK_SIZE;

#if 1
	// Render arbitrary meshses into gbuffer to test combining rasterized and raytraced objects
	struct TestRenderer {
		Shader*		shad;
		IndexedMesh	mesh;

		float3 pos = float3(-42,83,-101);
		float2 rot = float3(0,0,0);
		float size = 5;

		TestRenderer (Shaders& shaders) {
			shad = shaders.compile("test", {{"WORLD_SIZE_CHUNKS", prints("%d", GPU_WORLD_SIZE_CHUNKS)}});

			auto& m = g->assets->stock_models;
			mesh = upload_mesh("stock_mesh", m.vertices.data(), m.vertices.size(), m.indices.data(), m.indices.size());
		}
		void imgui () {
			g_debugdraw.movable("Stock_mesh", &pos, 0.4f, lrgba(0.7f,0,0.7f,1));

			ImGui::DragFloat3("Stock_mesh pos", &pos.x, 0.1f);
			ImGui::DragFloat2("Stock_mesh rot", &rot.x, 0.1f);
			ImGui::DragFloat("Stock_mesh size", &size, 0.1f);
		}
		void draw (OpenglRenderer& r) {
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
	};
#endif
	struct ComputeGroupSize {
		int2 _size;
		int2 size;

		ComputeGroupSize (int2 const& s): _size{s}, size{s} {}

		bool imgui (char const* lbl) {
			ImGui::PushID(lbl);

			ImGui::InputInt2(lbl, &_size.x);
			ImGui::SameLine(); 

			bool changed = ImGui::Button("Update");
			if (changed)
				size = _size;

			ImGui::PopID();
			return changed;
		}
	};

////
	struct VoxelTexture {
		Texture3D	tex;

		VoxelTexture () {
			tex = {"RT.voxels"};

			glTextureStorage3D(tex, 1, GL_R16UI, GPU_WORLD_SIZE, GPU_WORLD_SIZE, GPU_WORLD_SIZE);

			//
			glTextureParameteri(tex, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);

			glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			//glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			//glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			//glTextureParameteri(tex, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
			glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTextureParameteri(tex, GL_TEXTURE_WRAP_R, GL_REPEAT);

			//lrgba color = lrgba(0,0,0,0);
			//glTextureParameterfv(tex, GL_TEXTURE_BORDER_COLOR, &color.x);
			
			//
			glClearTexImage(tex, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, nullptr); // clear texture to B_NULL
		}
	};
	struct DFTexture {
		Texture3D	tex = {"RT.DF.tex"};

		static constexpr int COMPUTE_GROUPSZ = 8;

		Shader* shad_init;
		Shader* shad_pass[3] = {};

		DFTexture (Shaders& shaders) {
			shad_init = shaders.compile("rt_df_init", {}, {{ COMPUTE_SHADER }});
			for (int pass=0; pass<3; ++pass)
				shad_pass[pass] = shaders.compile("rt_df_gen", prints("rt_df_gen%d", pass).c_str(), {
						{"GROUPSZ", prints("%d", COMPUTE_GROUPSZ)},
						{"PASS", prints("%d", pass)},
					}, {{ COMPUTE_SHADER }});

			glTextureStorage3D(tex, 1, GL_R8I, GPU_WORLD_SIZE, GPU_WORLD_SIZE, GPU_WORLD_SIZE);

			//
			glTextureParameteri(tex, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);

			glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			//glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			//glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			//glTextureParameteri(tex, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
			glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTextureParameteri(tex, GL_TEXTURE_WRAP_R, GL_REPEAT);

			//
			int dist = 255; // max dist
			glClearTexImage(tex, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, &dist);
		}
	};

	struct VCT_Data {
		static constexpr int TEX_WIDTH = GPU_WORLD_SIZE;
		static constexpr size_t _size = (sizeof(uint8_t)*4 * TEX_WIDTH*TEX_WIDTH*TEX_WIDTH) / MB;

		static constexpr int COMPUTE_FILTER_LOCAL_SIZE = 4;

		static constexpr int MIPS = get_const_log2((uint32_t)TEX_WIDTH)+1;

		// how many octree layers to filter per uploaded chunk (rest are done for whole world)
		// only compute mips per chunk until dipatch size is 4^3, to not waste dispatches for workgroups with only 1 or 2 active threads
		static constexpr int FILTER_CHUNK_MIPS = get_const_log2((uint32_t)(CHUNK_SIZE/2 / 4))+1;

		// Require glTextureView to allow compute shader to write into srgb texture via imageStore
		struct VctTexture {
			Texture3D tex;

			// texview to allow binding GL_SRGB8_ALPHA8 as GL_RGBA8UI in compute shader (imageStore does not support srgb)
			// this way at least I can manually do the srgb conversion before writing
			GLuint texview;

			VctTexture (std::string_view label, int mipmaps, int3 const& size, bool sparse=false): tex{label} {
				//if (sparse) {
				//	glTextureParameteri(tex, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
				//	glTextureParameteri(tex, GL_VIRTUAL_PAGE_SIZE_INDEX_ARB, 0);
				//}
				glTextureStorage3D(tex, mipmaps, GL_SRGB8_ALPHA8, size.x,size.y,size.z);
				glTextureParameteri(tex, GL_TEXTURE_BASE_LEVEL, 0);
				glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, mipmaps-1);

				lrgba col = srgba(0,0,0,0);
				glClearTexImage(tex, 0, GL_RGBA, GL_FLOAT, &col.x);

				glGenTextures(1, &texview);
				glTextureView(texview, GL_TEXTURE_3D, tex, GL_RGBA8UI, 0,mipmaps, 0,1);

				OGL_DBG_LABEL(GL_TEXTURE, texview, label + ".texview");
			}
			~VctTexture () {
				glDeleteTextures(1, &texview);
			}
		}; 

		VctTexture tex_mip0 = {"VCT.tex_mip0", 1, TEX_WIDTH};
		VctTexture textures[6] = {
			{"VCT.texNX", MIPS-1, TEX_WIDTH/2},
			{"VCT.texPX", MIPS-1, TEX_WIDTH/2},
			{"VCT.texNY", MIPS-1, TEX_WIDTH/2},
			{"VCT.texPY", MIPS-1, TEX_WIDTH/2},
			{"VCT.texNZ", MIPS-1, TEX_WIDTH/2},
			{"VCT.texPZ", MIPS-1, TEX_WIDTH/2},
		};

		Sampler sampler = {"sampler"};
		Sampler filter_sampler = {"filter_sampler"};

		Shader* filter_mip0;
		Shader* filter;

		int3 sparse_size;
		array3D<bool> sparse_page_state;

		int3 get_sparse_texture3d_config (GLenum texel_format) {
			int3 res;
			glGetInternalformativ(GL_TEXTURE_3D, texel_format, GL_VIRTUAL_PAGE_SIZE_X_ARB, 1, &res.x);
			glGetInternalformativ(GL_TEXTURE_3D, texel_format, GL_VIRTUAL_PAGE_SIZE_Y_ARB, 1, &res.y);
			glGetInternalformativ(GL_TEXTURE_3D, texel_format, GL_VIRTUAL_PAGE_SIZE_Z_ARB, 1, &res.z);
			return res;
		}

		VCT_Data (Shaders& shaders) {
			filter_mip0  = shaders.compile("vct_filter", {{"MIP0","1"}}, {COMPUTE_SHADER});
			filter       = shaders.compile("vct_filter", {{"MIP0","0"}}, {COMPUTE_SHADER});

			lrgba color = lrgba(0,0,0,0);

			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // GL_LINEAR_MIPMAP_NEAREST
			glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			//glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			//glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			//glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
			//glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, &color.x);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, GL_REPEAT);

			glSamplerParameteri(filter_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glSamplerParameteri(filter_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glSamplerParameteri(filter_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(filter_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(filter_sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

			sparse_size = get_sparse_texture3d_config(GL_SRGB8_ALPHA8);

			sparse_page_state.resize(int3(GPU_WORLD_SIZE) / sparse_size);
			sparse_page_state.clear(true);

			//assert(GLAD_GL_ARB_sparse_texture && // for sparse texture support
			//	GLAD_GL_ARB_sparse_texture2); // for relying on decommitted texture regions reading as zero
											  //	assert(GLAD_GL_NV_memory_object_sparse);
		}

		void visualize_sparse (OpenglRenderer& r) {
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
		void recompute_mips (OpenglRenderer& r, VoxelTexture& vox_tex, std::vector<int3> const& chunks) {
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
			//	auto air = SUBC_SPARSE_BIT | g->assets.block_types.map_id("air");
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

			auto dispatch_chunked = [&] (int layer, Shader* shad, bool mip0=false) {
				int size = CHUNK_SIZE >> layer;
				shad->set_uniform("size", (GLuint)size);

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

				memset(offsets, 0, sizeof(offsets));
				glUniform3uiv(shad->get_uniform_location("offsets[0]"), ARRLEN(offsets), (GLuint*)offsets);

				int dispatch_size = (size + COMPUTE_FILTER_LOCAL_SIZE-1) / COMPUTE_FILTER_LOCAL_SIZE;
				glDispatchCompute(dispatch_size, dispatch_size, dispatch_size);

				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
			};

			{ // layer 0, generate texture from voxel data in voxel textures
				glUseProgram(filter_mip0->prog);
				r.state.bind_textures(filter_mip0, {
					{"voxel_tex", vox_tex.tex},
					//{"df_tex",    r.raytracer.df_tex.tex},
					{"tile_textures", r.tile_textures, r.pixelated_sampler},
				});
			
				glBindImageTexture(0, tex_mip0.texview, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8UI);
			
				dispatch_chunked(0, filter_mip0);
			}
		
			{ // layer 1+
				glUseProgram(filter->prog);
			
				r.state.bind_textures(filter, {
					{"vct_tex_mip0",  tex_mip0.tex, sampler },
					{"vct_texNX", textures[0].tex, sampler },
					{"vct_texPX", textures[1].tex, sampler },
					{"vct_texNY", textures[2].tex, sampler },
					{"vct_texPY", textures[3].tex, sampler },
					{"vct_texNZ", textures[4].tex, sampler },
					{"vct_texPZ", textures[5].tex, sampler },
				});
			
				// filter only texels for each chunk (up to 4x4x4 work groups)
				for (int layer=1; layer<FILTER_CHUNK_MIPS; ++layer) {
				
					for (int dir=0; dir<6; ++dir)
						glBindImageTexture(dir, textures[dir].texview, layer-1, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8UI);
			
					dispatch_chunked(layer, filter);
				}
			
				// filter whole texture again for remaining mips
				for (int layer=FILTER_CHUNK_MIPS; layer<MIPS; ++layer) {
				
					for (int dir=0; dir<6; ++dir)
						glBindImageTexture(dir, textures[dir].texview, layer-1, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8UI);
				
					dispatch_whole(layer, filter);
				}
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
			//	auto air = SUBC_SPARSE_BIT | g->assets.block_types.map_id("air");
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
	};

	struct Raytracer {
		SERIALIZE(Raytracer, enable, max_iterations, taa, lighting)
		
		Shader* rt_forward   = nullptr;
		Shader* rt_lighting  = nullptr;
		Shader* rt_lightingVCT = nullptr;
		Shader* rt_post0     = nullptr;
		Shader* rt_post1     = nullptr;
		
		RenderScale renderscale;

		VoxelTexture voxel_tex;
		DFTexture df_tex;
		VCT_Data vct_data;

		TestRenderer test_renderer;

		Gbuffer     gbuf;
		Framebuffer framebuf0;
		Framebuffer framebuf1;

		TemporalAA taa;
		
		ComputeGroupSize rt_groupsz = int2(8,8);

		OGL_TIMER_HISTOGRAM(rt_total);
		OGL_TIMER_HISTOGRAM(rt_forward);
		OGL_TIMER_HISTOGRAM(rt_lighting);
		OGL_TIMER_HISTOGRAM(rt_post);
		OGL_TIMER_HISTOGRAM(df_init);

		gl::MacroDefs get_macros () {
			return gl::MacroDefs{{
				{"WORLD_SIZE_CHUNKS", prints("%d", GPU_WORLD_SIZE_CHUNKS)},
				{"WG_PIXELS_X", prints("%d", rt_groupsz.size.x)},
				{"WG_PIXELS_Y", prints("%d", rt_groupsz.size.y)},
				{"WG_CONES", prints("%d", cone_data.count)},
				{"TAA_ENABLE", taa.enable ? "1":"0"},
				{"BEVEL", lighting.bevel ? "1":"0"},
				{"BOUNCE_ENABLE", lighting.bounce_enable ? "1":"0"},
				{"VCT_DBG_PRIMARY", lighting.vct_dbg_primary ? "1":"0"},
				{"VISUALIZE_COST", visualize_cost ? "1":"0"},
				{"VISUALIZE_TIME", visualize_time ? "1":"0"}
			}};
		}
		std::vector<gl::MacroDefinition> get_post_macros (int pass) {
			return { {"PASS", prints("%d", pass)} };
		}

		Raytracer (Shaders& shaders): df_tex(shaders), vct_data(shaders), test_renderer(shaders) {
			#if 0
				int max_sparse_texture_size;
				int max_sparse_3d_texture_size;
				int max_sparse_array_texture_layers;
				int sparse_texture_full_array_cube_mipmaps;

				glGetIntegerv(GL_MAX_SPARSE_TEXTURE_SIZE_ARB                 , &max_sparse_texture_size);
				glGetIntegerv(GL_MAX_SPARSE_3D_TEXTURE_SIZE_ARB              , &max_sparse_3d_texture_size);
				glGetIntegerv(GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS_ARB         , &max_sparse_array_texture_layers);
				glGetIntegerv(GL_SPARSE_TEXTURE_FULL_ARRAY_CUBE_MIPMAPS_ARB  , &sparse_texture_full_array_cube_mipmaps);

				GLint page_sizes;
				glGetInternalformativ(GL_TEXTURE_3D, GL_SRGB8_ALPHA8, GL_NUM_VIRTUAL_PAGE_SIZES_ARB, 1, &page_sizes);

				std::vector<GLint> sizes_x(page_sizes), sizes_y(page_sizes), sizes_z(page_sizes);
				glGetInternalformativ(GL_TEXTURE_3D, GL_SRGB8_ALPHA8, GL_VIRTUAL_PAGE_SIZE_X_ARB, page_sizes, sizes_x.data());
				glGetInternalformativ(GL_TEXTURE_3D, GL_SRGB8_ALPHA8, GL_VIRTUAL_PAGE_SIZE_Y_ARB, page_sizes, sizes_y.data());
				glGetInternalformativ(GL_TEXTURE_3D, GL_SRGB8_ALPHA8, GL_VIRTUAL_PAGE_SIZE_Z_ARB, page_sizes, sizes_z.data());

				Texture3D tex = {"test"};

				glBindTexture(GL_TEXTURE_3D, tex);
				glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
				glTexParameteri(GL_TEXTURE_3D, GL_VIRTUAL_PAGE_SIZE_INDEX_ARB, 0);

				glTextureStorage3D(tex, 1, GL_SRGB8_ALPHA8, 32*SUBCHUNK_COUNT, 32*SUBCHUNK_COUNT, 32*SUBCHUNK_COUNT);
			#endif
			#if 0

				int max_compute_work_group_invocations;
				int3 max_compute_work_group_count;
				int3 max_compute_work_group_size;
				glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &max_compute_work_group_invocations);
				glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &max_compute_work_group_count.x);
				glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &max_compute_work_group_count.y);
				glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &max_compute_work_group_count.z);
				glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &max_compute_work_group_size.x);
				glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &max_compute_work_group_size.y);
				glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &max_compute_work_group_size.z);

			#endif
		}
		
		int3 voxtex_offset = 0;
		
		bool enable = true;

		int max_iterations = 512;

		bool rand_seed_time = true;

		bool visualize_cost = false;
		bool visualize_time = false;
		float visualize_mult = 1;

		float gauss_radius_px = 0.01f;

		struct Lighting {
			SERIALIZE(Lighting,
				bounce_enable, bounce_max_dist, bounce_max_count, bounce_samples, roughness,
				vct)
			
			bool show_light = false;
			bool show_normals = false;

			bool bounce_enable = true;
			float bounce_max_dist = 90.0f;
			int bounce_max_count = 4;
			int bounce_samples = 1;

			float roughness = 0.8f;

			bool bevel = true;

			float post_exposure = 1.0f;


			bool vct = true;
			bool vct_dbg_primary = false;

			float vct_primary_cone_width = 0.01f;
			float vct_min_start_dist = 1.0f / 5; // 1/32
			
			bool vct_diffuse = true;
			bool vct_specular = false;

			float test = 1.0f;

			void imgui (bool& macro_change) {
				if (!ImGui::TreeNodeEx("lighting", ImGuiTreeNodeFlags_DefaultOpen)) return;

				ImGui::Checkbox("show_light", &show_light);
				ImGui::Checkbox("show_normals", &show_normals);

				macro_change |= ImGui::Checkbox("bounce_enable", &bounce_enable);
				ImGui::DragFloat("bounce_max_dist", &bounce_max_dist, 0.1f, 0, 256);
				ImGui::SliderInt("bounce_max_count", &bounce_max_count, 0, 16);
				ImGui::SliderInt("bounce_samples", &bounce_samples, 1, 16);

				ImGui::SliderFloat("roughness", &roughness, 0,1);

				macro_change |= ImGui::Checkbox("bevel", &bevel);

				ImGui::SliderFloat("post_exposure", &post_exposure, 0.0005f, 4.0f);
				
				ImGui::Separator();

				ImGui::Checkbox("Vct [V]", &vct);
				macro_change |= ImGui::Checkbox("vct_dbg_primary", &vct_dbg_primary);
				
				ImGui::SliderFloat("vct_primary_cone_width", &vct_primary_cone_width, 0.0005f, 0.2f);
				ImGui::SliderFloat("vct_min_start_dist", &vct_min_start_dist, 0.001f, 4.0f);
				
				ImGui::Checkbox("vct_diffuse", &vct_diffuse);
				ImGui::Checkbox("vct_specular", &vct_specular);

				ImGui::DragFloat("test", &test, 0.01f);

				ImGui::TreePop();
			}
		} lighting;

		bool macro_change = false; // shader macro change
		void imgui (Input& I) {
			if (!ImGui::TreeNodeEx("Raytracer", ImGuiTreeNodeFlags_DefaultOpen)) return;

			OGL_TIMER_HISTOGRAM_UPDATE(rt_total    , I.dt)
			OGL_TIMER_HISTOGRAM_UPDATE(rt_forward  , I.dt)
			OGL_TIMER_HISTOGRAM_UPDATE(rt_lighting , I.dt)
			OGL_TIMER_HISTOGRAM_UPDATE(rt_post     , I.dt)
			OGL_TIMER_HISTOGRAM_UPDATE(df_init     , I.dt)

			ImGui::Checkbox("enable [R]", &enable);

			renderscale.imgui();

			ImGui::SliderInt("max_iterations", &max_iterations, 1, 1024, "%4d", ImGuiSliderFlags_Logarithmic);

			macro_change |= ImGui::Checkbox("visualize_cost", &visualize_cost);
			ImGui::SameLine();
			macro_change |= ImGui::Checkbox("visualize_time", &visualize_time);
			ImGui::DragFloat("visualize_mult", &visualize_mult, 0.1f, 0, 1000, "%f", ImGuiSliderFlags_Logarithmic);

			macro_change |= rt_groupsz.imgui("rt_groupsz");

			ImGui::Checkbox("rand_seed_time", &rand_seed_time);

			ImGui::SliderInt("max_age", &taa.max_age, 0, 100, "%d", ImGuiSliderFlags_Logarithmic);
			ImGui::SameLine();
			macro_change |= ImGui::Checkbox("TAA", &taa.enable);
			
			ImGui::SliderFloat("gauss_radius_px", &gauss_radius_px, 0.01f, 1000, "%.3f", ImGuiSliderFlags_Logarithmic);

			lighting.imgui(macro_change);

			//ImGui::Separator();
			//test_renderer.imgui();

			//
			ImGui::TreePop();
		}

		void upload_changes (OpenglRenderer& r) {
			ZoneScoped;

			auto& chunks = *g->chunks;

			// These are the base chunk positions of the GPU_WORLD_SIZE_CHUNKS cube of voxels in world space
			int3 offset = roundi(g->lod_center()) / CHUNK_SIZE - GPU_WORLD_SIZE_CHUNKS/2;
			int3 old_offset = voxtex_offset;
			voxtex_offset = offset;
		
			//g_debugdraw.wire_cube((float3)(voxtex_offset+GPU_WORLD_SIZE_CHUNKS/2)*CHUNK_SIZE, GPU_WORLD_SIZE_CHUNKS*CHUNK_SIZE, lrgba(.5f,.5f,.5f,1));

			std::unordered_set<int3> reupload_chunks; // deduplicate due to 3 planar iterations and changed chunks
			std::unordered_set<int3> clear_chunks;
			std::vector<int3> reupload_chunk_flat;
		
			auto chunk_in_gpu_world = [&] (int3 chunk_pos) {
				int3 rel_pos = chunk_pos - voxtex_offset;
				return (unsigned)(rel_pos.x) < GPU_WORLD_SIZE_CHUNKS &&
					   (unsigned)(rel_pos.y) < GPU_WORLD_SIZE_CHUNKS &&
					   (unsigned)(rel_pos.z) < GPU_WORLD_SIZE_CHUNKS;
			};

			{ // Find gpu world chunks which have wrapped around if gpu world cube moves
				auto reupload_chunk = [&] (int3 chunk_pos_rel) {
					int3 world_pos = chunk_pos_rel + offset;
					assert(chunk_in_gpu_world(world_pos));
					reupload_chunks.emplace(world_pos);
				};
			
				if (offset != old_offset) {
					//int3 move = offset - old_offset;
					//int3 lo = 0;
					//int3 hi = GPU_WORLD_SIZE_CHUNKS;
					//if (all(move < GPU_WORLD_SIZE_CHUNKS)) {
					//	lo = select(move > 0, GPU_WORLD_SIZE_CHUNKS-move, 0);
					//	hi = select(move > 0, GPU_WORLD_SIZE_CHUNKS, move);
					//}
					//
					//for (int x=lo.x; x<hi.x; x++)
					//for (int y=0; y<GPU_WORLD_SIZE_CHUNKS; y++)
					//for (int z=0; z<GPU_WORLD_SIZE_CHUNKS; z++) {
					//	reupload_chunk(int3(x,y,z));
					//}
					//for (int y=lo.y; y<hi.y; y++)
					//for (int x=0; x<GPU_WORLD_SIZE_CHUNKS; x++)
					//for (int z=0; z<GPU_WORLD_SIZE_CHUNKS; z++) {
					//	reupload_chunk(int3(x,y,z));
					//}
					//for (int z=lo.z; z<hi.z; z++)
					//for (int x=0; x<GPU_WORLD_SIZE_CHUNKS; x++)
					//for (int y=0; z<GPU_WORLD_SIZE_CHUNKS; z++) {
					//	reupload_chunk(int3(x,y,z));
					//}
			
					for (int z=0; z<GPU_WORLD_SIZE_CHUNKS; z++)
					for (int y=0; y<GPU_WORLD_SIZE_CHUNKS; y++)
					for (int x=0; x<GPU_WORLD_SIZE_CHUNKS; x++) {
						int3 world_pos = int3(x,y,z) + offset; // world position of all gpu chunks after shifting gpu world
						int3 old_pos_rel = world_pos - old_offset; // world pos of chunk before movement 
				
						bool was_inside =
							(unsigned)(old_pos_rel.x) < GPU_WORLD_SIZE_CHUNKS &&
							(unsigned)(old_pos_rel.y) < GPU_WORLD_SIZE_CHUNKS &&
							(unsigned)(old_pos_rel.z) < GPU_WORLD_SIZE_CHUNKS;
						if (!was_inside)
							reupload_chunk(int3(x,y,z));
					}
				}
			}

			//// Reupload any chunks with changes
			// take all chunks that have had voxels updated AND are inside the sliding window of gpu voxel memory
			//  -> ie chunk coords [voxtex_offset, voxtex_offset + GPU_WORLD_SIZE_CHUNKS)
			for (auto cid : chunks.upload_voxels) {
				auto& chunk = chunks.chunks[cid];
				if (chunk_in_gpu_world(chunk.pos)) {
					reupload_chunks.emplace(chunk.pos);
				}
			}
			// Unload chunks to unload
			for (auto cpos : chunks.unload_chunks) {
				if (chunk_in_gpu_world(cpos)) {
					clear_chunks.emplace(int3(cpos));
				}
			}
		
			// temp buffer to 'decompress' my sparse subchunks and enable uploading them in a single glTextureSubImage3D per chunk
			//block_id buffer[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE]; // WARNING: having two of these (even in separate scopes) apparently overflows the stack!
			auto* buffer = (DenseChunkVoxels*)malloc(sizeof(DenseChunkVoxels)*2);
		

			//// Execute reupload
			// and "decompress" them ie. de-sparsify them, then upload them to dense voxel region on gpu
			// but upload them using world positions wrapped by GPU_WORLD_SIZE_CHUNKS instead, so that sliding the window by 1 chunk does not require moving all the contents
			if (!reupload_chunks.empty()) {
				OGL_TRACE("raytracer upload changes");

				for (auto pos : reupload_chunks) {
					auto it = chunks.chunks_map.find(pos);
					if (it == chunks.chunks_map.end()) {
						// Chunk should be reuploaded due to gpu world movement, but we have no chunk loaded there, need to clear data
						clear_chunks.emplace(int3(pos));
						continue;
					}

					clear_chunks.erase(int3(pos)); // don't clear chunks we overwrite anyway

					auto cid = it->second;
					auto& chunk = chunks.chunks[cid];
					auto& vox = chunks.chunk_voxels[cid];

					assert(chunk_in_gpu_world(chunk.pos));
					int3 wrap_pos = chunk.pos & (GPU_WORLD_SIZE_CHUNKS-1);
				
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
									auto* dst = &buffer->voxels[sz*SUBCHUNK_SIZE + z][sy*SUBCHUNK_SIZE + y][sx*SUBCHUNK_SIZE + 0];
									memcpy(dst, val_packed, sizeof(block_id)*SUBCHUNK_SIZE);
								}
							
							} else {
								auto* data = chunks.subchunks[subc].voxels;
							
								for (int z=0; z<SUBCHUNK_SIZE; ++z)
								for (int y=0; y<SUBCHUNK_SIZE; ++y) {
									auto* dst = &buffer->voxels[sz*SUBCHUNK_SIZE + z][sy*SUBCHUNK_SIZE + y][sx*SUBCHUNK_SIZE + 0];
									auto* src = &data[IDX3D(0,y,z, SUBCHUNK_SIZE)];
									memcpy(dst, src, sizeof(block_id)*SUBCHUNK_SIZE);
								}
							}
						}
					}

					{
						ZoneScopedN("glTextureSubImage3D");

						glTextureSubImage3D(voxel_tex.tex, 0,
							wrap_pos.x*CHUNK_SIZE, wrap_pos.y*CHUNK_SIZE, wrap_pos.z*CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE,
							GL_RED_INTEGER, GL_UNSIGNED_SHORT, &buffer->voxels);
					}

					//g_debugdraw.wire_cube_stay((float3)pos*CHUNK_SIZE+CHUNK_SIZE/2, CHUNK_SIZE, lrgba(0,1,0,1), 2);

					reupload_chunk_flat.push_back(wrap_pos);
				}
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
			}
		
			// Unload chunks by replacing them with null voxels
			// Do this before DF gen to be correct
			if (!clear_chunks.empty()) {
				memset(&buffer->voxels, 0, sizeof(buffer->voxels));

				for (auto cpos : clear_chunks) {
					assert(chunk_in_gpu_world(cpos));

					int3 wrap_pos = cpos & (GPU_WORLD_SIZE_CHUNKS-1);

					// Actually unloaded and not replaced, reupload empty chunk
					glTextureSubImage3D(voxel_tex.tex, 0,
						wrap_pos.x*CHUNK_SIZE, wrap_pos.y*CHUNK_SIZE, wrap_pos.z*CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE,
						GL_RED_INTEGER, GL_UNSIGNED_SHORT, &buffer->voxels);
					
					reupload_chunk_flat.push_back(wrap_pos);

					//g_debugdraw.wire_cube_stay((float3)(wrap_pos + voxtex_offset)*CHUNK_SIZE+CHUNK_SIZE/2, CHUNK_SIZE, lrgba(1,.5f,0,1), 2);
				}
			}

			// Use uploaded dense voxel gpu data to compute VCT data in compute shader
			// (batched into multiple chunks per compute invoke)
			{
				//chunks.clear(); // for profiling df gen
				//for (int y=0; y<4; ++y)
				//for (int x=0; x<4; ++x) {
				//	chunks.push_back({x,y,7});
				//}

				if (!reupload_chunk_flat.empty()) {
					ZoneScopedN("rt_df_gen");
					OGL_TRACE("rt_df_gen");
					OGL_TIMER_ZONE(timer_df_init.timer);

					int count = (int)reupload_chunk_flat.size();

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
								offsets[j] = reupload_chunk_flat[i+j] * CHUNK_SIZE;

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
								offsets[j] = reupload_chunk_flat[i+j] * CHUNK_SIZE;
				
							shad->set_uniform_array("offsets[0]", offsets, BATCHSIZE);
				
							int dispatch_size = (CHUNK_SIZE + DFTexture::COMPUTE_GROUPSZ -1) / DFTexture::COMPUTE_GROUPSZ;
							glDispatchCompute(dispatch_size, dispatch_size, subcount);
						}
				
						glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
					}
				}

				glBindImageTexture(4, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8); // unbind
			}
			vct_data.recompute_mips(r, voxel_tex, reupload_chunk_flat);

			free(buffer);
		}

		// update things and upload changes to gpu
		void update (OpenglRenderer& r, Input& I) {
			ZoneScoped;
		
			macro_change |= conedev.vct_conedev(*this);

			if (renderscale.update(r.render_size)) {
				taa.resize(renderscale.size);
				gbuf.resize(renderscale.size);
				framebuf0.resize(renderscale.size, renderscale.nearest);
				framebuf1.resize(renderscale.size, renderscale.nearest);
			}

			upload_bind_ubo(cones_ubo, 4, &cone_data, sizeof(cone_data));

			if (I.buttons[KEY_R].went_down)
				enable = !enable;

			if (I.buttons[KEY_V].went_down) {
				lighting.vct = !lighting.vct;
			}

			// lazy init these (instead of doing it in ctor) to allow json changes to affect the macros
			// this would not be needed in a sane programming language (reflection support)
			if (!rt_forward ) rt_forward  = r.shaders.compile("rt_forward",  get_macros(), {{ COMPUTE_SHADER }});
			if (!rt_lighting) rt_lighting = r.shaders.compile("rt_lighting", get_macros(), {{ COMPUTE_SHADER }});
			if (!rt_lightingVCT) rt_lightingVCT = r.shaders.compile("rt_lighting",
				get_macros() + MacroDefinition{"VCT","1"}, {{ COMPUTE_SHADER }});
			if (!rt_post0   ) rt_post0    = r.shaders.compile("rt_post", get_post_macros(0));
			if (!rt_post1   ) rt_post1    = r.shaders.compile("rt_post", get_post_macros(1));

			if (macro_change && rt_forward) {
				rt_forward->macros = get_macros();
				rt_forward->recompile("macro_change", false);
			}
			if (macro_change && rt_lighting) {
				rt_lighting->macros = get_macros();
				rt_lighting->recompile("macro_change", false);
			}
			if (macro_change && rt_lightingVCT) {
				rt_lightingVCT->macros = get_macros() + MacroDefinition{"VCT","1"};
				rt_lightingVCT->recompile("macro_change", false);
			}
			if (macro_change && rt_post0) {
				rt_post0->macros = get_post_macros(0);
				rt_post0->recompile("macro_change", false);
			}
			if (macro_change && rt_post1) {
				rt_post1->macros = get_post_macros(1);
				rt_post1->recompile("macro_change", false);
			}
			macro_change = false;

			upload_changes(r);
		}

		void set_uniforms (OpenglRenderer& r, Shader* shad)  {
			shad->set_uniform("rand_seed_time", rand_seed_time ? (int)g->input.frame_counter : 0);

			shad->set_uniform("framebuf_size", renderscale.size);
			shad->set_uniform("update_debugdraw", r.debug_draw.update_indirect);

			float base_px_size;
			{
				// frustrum_size (world-space size of near plane) / clip_near -> world-space width of frustrum at 1m z dist
				// pixels / (frustrum_size / clip_near) -> pixels that a ~1m object has at 1m
				// divide this by any object's z-distance to get a pixel size for LOD purposes
				base_px_size = (float)renderscale.size.x / g->view.frustrum_size.x * g->view.clip_near;
			}
			shad->set_uniform("base_px_size", base_px_size);

			shad->set_uniform("max_iterations", max_iterations);

			shad->set_uniform("visualize_mult", visualize_mult);

			// HACK: when blurring light (rather than final image) for denoising vct_dbg_primary is harder to make work
			shad->set_uniform("show_light", lighting.show_light || lighting.vct_dbg_primary);
			shad->set_uniform("show_normals", lighting.show_normals);

			shad->set_uniform("bounce_max_dist", lighting.bounce_max_dist);
			shad->set_uniform("bounce_max_count", lighting.bounce_max_count);
			shad->set_uniform("bounce_samples", lighting.bounce_samples);

			shad->set_uniform("roughness", lighting.roughness);

			shad->set_uniform("vct_primary_cone_width", lighting.vct_primary_cone_width);
			shad->set_uniform("vct_min_start_dist", lighting.vct_min_start_dist);

			shad->set_uniform("vct_diffuse", lighting.vct_diffuse);
			shad->set_uniform("vct_specular", lighting.vct_specular);

			shad->set_uniform("test", lighting.test);

			shad->set_uniform("voxtex_world_min", (float3)(voxtex_offset * CHUNK_SIZE));
		}
		void draw (OpenglRenderer& r) {
			ZoneScoped;
			if (!rt_forward->prog || !rt_lighting->prog || !rt_post0->prog || !rt_post1->prog) return;
			OGL_TIMER_ZONE(timer_rt_total.timer);
		
			r.update_view(g->view, renderscale.size, g->lod_center());

			{ // forward pass -> writes to gbuf
			
				{ // forward voxel raycast pass
					ZoneScopedN("rt_shad");
					OGL_TRACE("rt_shad");
					OGL_TIMER_ZONE(timer_rt_forward.timer);
				
					glUseProgram(rt_forward->prog);
				
					set_uniforms(r, rt_forward);
				
					r.state.bind_textures(rt_forward, {
						{"voxel_tex", voxel_tex.tex},
						{"df_tex", df_tex.tex},
					
						{"tile_textures", r.tile_textures, r.pixelated_sampler},
					
						{"heat_gradient", r.gradient, r.smooth_sampler},
					});
				
					glBindImageTexture(0, gbuf.pos   , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
					glBindImageTexture(1, gbuf.faceid, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R16UI);
					glBindImageTexture(2, gbuf.col   , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
					glBindImageTexture(3, gbuf.norm  , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

					int2 dispatch_size;
					dispatch_size.x = (renderscale.size.x + rt_groupsz.size.x -1) / rt_groupsz.size.x;
					dispatch_size.y = (renderscale.size.y + rt_groupsz.size.y -1) / rt_groupsz.size.y;
					glDispatchCompute(dispatch_size.x, dispatch_size.y, 1);
				
					for (int i=0; i<4; ++i)
						glBindImageTexture(i, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
				}
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
			
				// normal rasterized drawing pass for gbuf testing
				glClear(GL_DEPTH_BUFFER_BIT);
				test_renderer.draw(r);
			}

			{ // deferred lighting pass  gbuf -> lit image
				ZoneScopedN("rt_lighting");
				OGL_TRACE("rt_lighting");
				OGL_TIMER_ZONE(timer_rt_lighting.timer);

				auto& shad = lighting.vct ? rt_lightingVCT : rt_lighting;
		
				glUseProgram(shad->prog);
		
				set_uniforms(r, shad);
		
				glBindImageTexture(0, framebuf0.col, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		
				// TAA
				GLuint prev_color  = taa.colors[taa.cur ^ 1];
				GLuint prev_posage = taa.posage[taa.cur ^ 1];
				GLuint cur_color   = taa.colors[taa.cur];
				GLuint cur_posage  = taa.posage[taa.cur];
		
				if (taa.enable) {
					shad->set_uniform("prev_world2clip", taa.prev_world2clip); // invalid on first frame, should be ok since history age = 0
					shad->set_uniform("taa_max_age", taa.max_age);
		
					glBindImageTexture(1, cur_color , 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F );
					glBindImageTexture(2, cur_posage, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RG16UI);
		
					taa.prev_world2clip = g->view.cam_to_clip * (float4x4)g->view.world_to_cam;
					taa.cur ^= 1;
				}
		
				r.state.bind_textures(shad, {
					{"voxel_tex", voxel_tex.tex},
					{"df_tex", df_tex.tex},
				
					{"vct_tex_mip0", vct_data.tex_mip0.tex, vct_data.sampler },
					{"vct_texNX", vct_data.textures[0].tex, vct_data.sampler },
					{"vct_texPX", vct_data.textures[1].tex, vct_data.sampler },
					{"vct_texNY", vct_data.textures[2].tex, vct_data.sampler },
					{"vct_texPY", vct_data.textures[3].tex, vct_data.sampler },
					{"vct_texNZ", vct_data.textures[4].tex, vct_data.sampler },
					{"vct_texPZ", vct_data.textures[5].tex, vct_data.sampler },
		
					{"gbuf_pos"   , gbuf.pos    },
					{"gbuf_faceid", gbuf.faceid },
					{"gbuf_col"   , gbuf.col    },
					{"gbuf_norm"  , gbuf.norm   },
				
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

			{
				OGL_TIMER_ZONE(timer_rt_post.timer);
				ZoneScopedN("rt_post");
				OGL_TRACE("rt_post");
			
				PipelineState s;
				s.depth_test = false;
				s.depth_write = false;
				s.blend_enable = false;
				r.state.set(s);

				glBindFramebuffer(GL_FRAMEBUFFER, framebuf1.fbo);

				{ // upscale and horizontal blur
					glUseProgram(rt_post0->prog);

					set_uniforms(r, rt_post0);
					rt_post0->set_uniform("exposure", lighting.post_exposure);
					rt_post0->set_uniform("gauss_radius_px", gauss_radius_px);

					r.state.bind_textures(rt_post0, {
						{"gbuf_faceid", gbuf.faceid },
						{"light", framebuf0.col, framebuf0.sampler},
					});

					glBindVertexArray(r.dummy_vao);
					glDrawArrays(GL_TRIANGLES, 0, 3);
				}

				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			
				r.update_view(g->view, r.render_size, g->lod_center());

				{ // vertical blur and exposure mapping
					glUseProgram(rt_post1->prog);

					set_uniforms(r, rt_post1);
					rt_post1->set_uniform("exposure", lighting.post_exposure);
					rt_post1->set_uniform("gauss_radius_px", gauss_radius_px);

					r.state.bind_textures(rt_post1, {
						{"gbuf_faceid", gbuf.faceid },
						{"light", framebuf1.col, framebuf1.sampler},
						{"gbuf_col", gbuf.col},
						{"gbuf_norm", gbuf.norm},
					});

					glBindVertexArray(r.dummy_vao);
					glDrawArrays(GL_TRIANGLES, 0, 3);
				}
			}

			//// try to all potentially bound textures
			//for (int i=0; i<3; ++i)
			//	glBindImageTexture(i, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
		}

		struct Cone {
			float3 dir;
			float  slope;
			float  weight;
			float3 _pad;
		};
		struct ConeConfig {
			int count;
			int _pad[3];
			Cone cones[32];
		};
		ConeConfig cone_data;

		Ubo cones_ubo = {"RT.cones_ubo"};

		struct Conedev {
			bool draw_cones=false, draw_boxes=false;
			float start_dist = 0.16f;
			bool refresh = true;
	
			struct Set {
				int count = 8;
				float cone_ang = 40.1f;

				float start_azim = 22.5f;
				float elev_offs = 2.1f;

				float weight = 1.0f;
			};
			std::vector<Set> sets = {
				{ 8, 40.1f, 22.5f, 2.1f, 0.25f },
				{ 4, 38.9f, 45.0f, 40.2f, 1.0f },
			};

			bool imgui () {
				if (!ImGui::TreeNodeEx("vct_conedev")) return false;
				refresh = true;

				ImGui::Checkbox("draw cones", &draw_cones);
				ImGui::SameLine();
				ImGui::Checkbox("draw boxes", &draw_boxes);
				ImGui::SliderFloat("start_dist", &start_dist, 0.05f, 2);

				int set_count = (int)sets.size();
				ImGui::DragInt("sets", &set_count, 0.01f);
				sets.resize(set_count);

				bool count_changed = false;

				int j=0;
				for (auto& s : sets) {
					if (ImGui::TreeNodeEx(&s, ImGuiTreeNodeFlags_DefaultOpen, "Set")) {

						count_changed = ImGui::SliderInt("count", &s.count, 0, 16) || count_changed;
						ImGui::DragFloat("cone_ang", &s.cone_ang, 0.1f, 0, 180);

						ImGui::DragFloat("start_azim", &s.start_azim, 0.1f);
						ImGui::DragFloat("elev_offs", &s.elev_offs, 0.1f);

						ImGui::DragFloat("weight", &s.weight, 0.01f);

						ImGui::TreePop();
					}
				}

				ImGui::TreePop();
				return count_changed;
			}
			// NOTE: kinda broken after I refactored it! Need to debug to get debug drawing working again
			bool vct_conedev (Raytracer& rt) {
				bool count_changed = imgui();
				if (!refresh) return false;
				refresh = false;

				lrgba cols[] = {
					{1,0,0,1},
					{0,1,0,1},
					{0,0,1,1},
					{1,1,0,1},
					{1,0,1,1},
					{0,1,1,1},
				};

				rt.cone_data.count = 0;

				float total_weight = 0;

				int j=0;
				for (auto& s : sets) {
					float ang = deg(s.cone_ang);

					for (int i=0; i<s.count; ++i) {
						float3 cone_pos = g->player->pos;

						float3x3 rot = rotate3_Z((float)(i-1) / s.count * deg(360) + deg(s.start_azim)) *
								rotate3_Y(deg(90) - ang * 0.5f - deg(s.elev_offs));

						auto& col = cols[i % ARRLEN(cols)];
						if (draw_cones) g_debugdraw.wire_cone(cone_pos, ang, 30, rot, col, 32, 4);

						float3 cone_dir = rot * float3(0,0,1);
						float cone_slope = tan(ang * 0.5f);
						float dist = start_dist;

						rt.cone_data.cones[j++] = { cone_dir, cone_slope, s.weight, 0 };
		
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

					rt.cone_data.count += s.count;
				}

				// normalize weights
				j=0;
				for (auto& s : sets) {
					for (int i=0; i<s.count; ++i) {
						rt.cone_data.cones[j++].weight /= total_weight;
					}
				}

				return count_changed;
			}
		};
		Conedev conedev;
		
	};

} // namespace gl
