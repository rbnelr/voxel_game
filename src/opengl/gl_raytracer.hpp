#pragma once
#include "common.hpp"
#include "opengl_helper.hpp"
#include "opengl_shaders.hpp"
#include "assets.hpp"
#include "game.hpp"

namespace gl {
	class OpenglRenderer;

#if 0
	struct Gbuffer {
		Fbo fbo = {};

		Texture2D depth = {};
		Texture2D pos = {};
		Texture2D col = {};
		Texture2D norm = {};

		int2 size = -1;

		Sampler sampler = {"Gbuf.sampler"};

		Gbuffer () {
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}

		void resize (int2 new_size) {
			if (fbo == 0 || size != new_size) {
				glActiveTexture(GL_TEXTURE0);

				size = new_size;

				depth = {"Gbuf.depth"};
				pos   = {"Gbuf.pos"  };
				col   = {"Gbuf.col"  };
				norm  = {"Gbuf.norm" };
				fbo   = {"Gbuf.fbo"  };

				glTextureStorage2D(depth, 1, GL_DEPTH_COMPONENT32F, size.x, size.y);
				glTextureParameteri(depth, GL_TEXTURE_BASE_LEVEL, 0);
				glTextureParameteri(depth, GL_TEXTURE_MAX_LEVEL, 0);

				glTextureStorage2D(pos, 1, GL_RGBA32F, size.x, size.y);
				glTextureParameteri(pos, GL_TEXTURE_BASE_LEVEL, 0);
				glTextureParameteri(pos, GL_TEXTURE_MAX_LEVEL, 0);

				glTextureStorage2D(col, 1, GL_RGBA16F, size.x, size.y);
				glTextureParameteri(col, GL_TEXTURE_BASE_LEVEL, 0);
				glTextureParameteri(col, GL_TEXTURE_MAX_LEVEL, 0);

				glTextureStorage2D(norm, 1, GL_RGBA16F, size.x, size.y);
				glTextureParameteri(norm, GL_TEXTURE_BASE_LEVEL, 0);
				glTextureParameteri(norm, GL_TEXTURE_MAX_LEVEL, 0);

				glNamedFramebufferTexture(fbo, GL_DEPTH_ATTACHMENT, depth, 0);
				glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, pos, 0);
				glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT1, col, 0);
				glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT2, norm, 0);

				GLuint bufs[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
				glNamedFramebufferDrawBuffers(fbo, 3, bufs);

				//GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				//if (status != GL_FRAMEBUFFER_COMPLETE) {
				//	fprintf(stderr, "glCheckFramebufferStatus: %x\n", status);
				//}
			}
		}
	};
#endif
	struct Gbuffer {
		Texture2D pos  = {};
		Texture2D col  = {};
		Texture2D norm = {};

		void resize (int2 size) {
			glActiveTexture(GL_TEXTURE0);

			pos   = {"gbuf.pos"  }; // could be computed from depth
			col   = {"gbuf.col"  }; // rgb albedo + emissive multiplier
			norm  = {"gbuf.norm" }; // rgb normal

			glTextureStorage2D(pos , 1, GL_RGBA32F, size.x, size.y);
			glTextureStorage2D(col , 1, GL_RGBA16F, size.x, size.y);
			glTextureStorage2D(norm, 1, GL_RGBA16F, size.x, size.y);

			glTextureParameteri(pos, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(pos, GL_TEXTURE_MAX_LEVEL, 0);

			glTextureParameteri(col, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(col, GL_TEXTURE_MAX_LEVEL, 0);

			glTextureParameteri(norm, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(norm, GL_TEXTURE_MAX_LEVEL, 0);
		}
	};
	struct Framebuffer {
		Texture2D col  = {};

		Sampler sampler = {"RTBuf.sampler"};

		void resize (int2 size, bool nearest) {
			glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, nearest ? GL_NEAREST : GL_LINEAR);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glActiveTexture(GL_TEXTURE0);

			col   = {"RTBuf.col"  };

			glTextureStorage2D(col , 1, GL_RGBA16F, size.x, size.y);
			glTextureParameteri(col, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(col, GL_TEXTURE_MAX_LEVEL, 0);
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
			if (colors[0] == 0 || size != new_size) {
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
					glTextureStorage2D(buf, 1, GL_RGBA16UI, size.x, size.y);
					glTextureParameteri(buf, GL_TEXTURE_BASE_LEVEL, 0);
					glTextureParameteri(buf, GL_TEXTURE_MAX_LEVEL, 0);
				}

				// clear textures to be read on first frame
				float3 col = float3(0,0,0);
				glClearTexImage(colors[0], 0, GL_RGB, GL_FLOAT, &col.x);

				uint32_t pos[4] = { 0u, 0u, 0u, 0xffffu };
				glClearTexImage(posage[0], 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, pos);

				cur = 1;
			}
		}
	};

	static constexpr int GPU_WORLD_SIZE_CHUNKS = 8;
	static constexpr int GPU_WORLD_SIZE = GPU_WORLD_SIZE_CHUNKS * CHUNK_SIZE;

#if 0
	// Render arbitrary meshses into gbuffer to test combining rasterized and raytraced objects
	struct TestRenderer {
		Shader*		shad;
		IndexedMesh	mesh;

		float3 pos = float3(-42,83,-101);
		float2 rot = float3(0,0,0);
		float size = 5;

		TestRenderer (Shaders& shaders) {
			shad = shaders.compile("test", {{"WORLD_SIZE_CHUNKS", prints("%d", GPU_WORLD_SIZE_CHUNKS)}});

			auto& m = g_assets.stock_models;
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
			glTextureParameteri(tex, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);

			glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
			glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			glTextureParameteri(tex, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);

			lrgba color = lrgba(0,0,0,0);
			glTextureParameterfv(tex, GL_TEXTURE_BORDER_COLOR, &color.x);

			glClearTexImage(tex, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr); // clear texture to B_NULL
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
				if (sparse) {
					glTextureParameteri(tex, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
					glTextureParameteri(tex, GL_VIRTUAL_PAGE_SIZE_INDEX_ARB, 0);
				}
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

		VctTexture textures[6] = {
			{"VCT.texNX", MIPS, TEX_WIDTH},
			{"VCT.texPX", MIPS, TEX_WIDTH},
			{"VCT.texNY", MIPS, TEX_WIDTH},
			{"VCT.texPY", MIPS, TEX_WIDTH},
			{"VCT.texNZ", MIPS, TEX_WIDTH},
			{"VCT.texPZ", MIPS, TEX_WIDTH},
		};

		void bind_textures (Shader* shad, int base_tex_unit) {

			GLint units[6];
			for (int i=0; i<6; ++i) {
				int unit = base_tex_unit + i;
				glActiveTexture(GL_TEXTURE0 + unit);
				glBindSampler(unit, sampler);
				glBindTexture(GL_TEXTURE_3D, textures[i].tex);
				units[i] = unit;
			}
			glUniform1iv(shad->get_uniform_location("vct_tex[0]"), 6, units);
		}

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
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
			glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, &color.x);

			glSamplerParameteri(filter_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glSamplerParameteri(filter_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glSamplerParameteri(filter_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(filter_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glSamplerParameteri(filter_sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

			sparse_size = get_sparse_texture3d_config(GL_SRGB8_ALPHA8);

			sparse_page_state.resize(int3(GPU_WORLD_SIZE) / sparse_size);
			sparse_page_state.clear(true);

			assert(GLAD_GL_ARB_sparse_texture && // for sparse texture support
				GLAD_GL_ARB_sparse_texture2); // for relying on decommitted texture regions reading as zero
											  //	assert(GLAD_GL_NV_memory_object_sparse);
		}

		void visualize_sparse (OpenglRenderer& r);
		void recompute_mips (OpenglRenderer& r, Game& game, std::vector<int3> const& chunks);
	};

	struct Raytracer {
		SERIALIZE(Raytracer, enable, max_iterations, taa, lighting)
		
		Shader* rt_forward   = nullptr;
		Shader* rt_lighting  = nullptr;
		Shader* rt_post      = nullptr;
		
		RenderScale renderscale;

		VoxelTexture voxel_tex;
		DFTexture df_tex;
		VCT_Data vct_data;

		//TestRenderer test_renderer;

		Gbuffer     gbuf;
		Framebuffer framebuf;

		TemporalAA taa;
		
		ComputeGroupSize rt_groupsz = int2(8,8);

		OGL_TIMER_HISTOGRAM(rt_total);
		OGL_TIMER_HISTOGRAM(rt_forward);
		OGL_TIMER_HISTOGRAM(rt_lighting);
		OGL_TIMER_HISTOGRAM(rt_post);
		OGL_TIMER_HISTOGRAM(df_init);

		std::vector<gl::MacroDefinition> get_macros () {
			return { {"WORLD_SIZE_CHUNKS", prints("%d", GPU_WORLD_SIZE_CHUNKS)},
			         {"WG_PIXELS_X", prints("%d", rt_groupsz.size.x)},
			         {"WG_PIXELS_Y", prints("%d", rt_groupsz.size.y)},
			         {"TAA_ENABLE", taa.enable ? "1":"0"},
			         {"BEVEL", lighting.bevel ? "1":"0"},
			         {"BOUNCE_ENABLE", lighting.bounce_enable ? "1":"0"},
			         {"VISUALIZE_COST", visualize_cost ? "1":"0"},
			         {"VISUALIZE_TIME", visualize_time ? "1":"0"}
			};
		}

		Raytracer (Shaders& shaders): df_tex(shaders), vct_data(shaders) {
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

		bool enable = true;

		int max_iterations = 512;

		bool rand_seed_time = true;

		bool visualize_cost = false;
		bool visualize_time = false;
		float visualize_mult = 1;

		struct Lighting {
			SERIALIZE(Lighting, bounce_enable, bounce_max_dist, bounce_max_count, bounce_samples, roughness)

			bool show_light = false;
			bool show_normals = false;

			bool bounce_enable = true;
			float bounce_max_dist = 90.0f;
			int bounce_max_count = 4;
			int bounce_samples = 1;

			float roughness = 0.8f;

			bool bevel = true;

			float post_exposure = 1.0f;

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

			lighting.imgui(macro_change);

			//ImGui::Separator();
			//test_renderer.imgui();

			//
			ImGui::TreePop();
		}

		void upload_changes (OpenglRenderer& r, Game& game);

		// update things and upload changes to gpu
		void update (OpenglRenderer& r, Game& game, Input& I);

		void set_uniforms (OpenglRenderer& r, Game& game, Shader* shad);
		void draw (OpenglRenderer& r, Game& game);
	};

} // namespace gl
