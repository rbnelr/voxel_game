#pragma once
#include "common.hpp"
#include "opengl_helper.hpp"
#include "opengl_shaders.hpp"
#include "assets.hpp"
#include "game.hpp"

#include "engine/window.hpp" // frame_counter

namespace gl {
	class OpenglRenderer;

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

			auto& m = g_assets.stock_models;
			mesh = upload_mesh("stock_mesh", m.vertices.data(), m.vertices.size(), m.indices.data(), m.indices.size());
		}
		void imgui () {
			g_debugdraw.movable("Stock_mesh", &pos, 0.4f, lrgba(0.7f,0,0.7f,1));

			ImGui::DragFloat3("Stock_mesh pos", &pos.x, 0.1f);
			ImGui::DragFloat2("Stock_mesh rot", &rot.x, 0.1f);
			ImGui::DragFloat("Stock_mesh size", &size, 0.1f);
		}
		void draw (OpenglRenderer& r);
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

		void visualize_sparse (OpenglRenderer& r);
		void recompute_mips (OpenglRenderer& r, Game& game, std::vector<int3> const& chunks);
	};
	
	struct Raytracer {
		SERIALIZE(Raytracer, enable, max_iterations, taa, lighting)
		
		Shader* rt_forward   = nullptr;
		Shader* rt_lighting  = nullptr;
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

		std::vector<gl::MacroDefinition> get_macros () {
			return { {"WORLD_SIZE_CHUNKS", prints("%d", GPU_WORLD_SIZE_CHUNKS)},
			         {"WG_PIXELS_X", prints("%d", rt_groupsz.size.x)},
			         {"WG_PIXELS_Y", prints("%d", rt_groupsz.size.y)},
			         {"WG_CONES", prints("%d", cone_data.count)},
			         {"TAA_ENABLE", taa.enable ? "1":"0"},
			         {"BEVEL", lighting.bevel ? "1":"0"},
			         {"BOUNCE_ENABLE", lighting.bounce_enable ? "1":"0"},
			         {"VCT", lighting.vct ? "1":"0"},
			         {"VCT_DBG_PRIMARY", lighting.vct_dbg_primary ? "1":"0"},
			         {"VISUALIZE_COST", visualize_cost ? "1":"0"},
			         {"VISUALIZE_TIME", visualize_time ? "1":"0"}
			};
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
		
		int3 voxtex_offset;
		
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

				macro_change |= ImGui::Checkbox("Vct [V]", &vct);
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
		void imgui (Input& I, Game& g) {
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
			
			macro_change |= conedev.vct_conedev(g, *this);

			//
			ImGui::TreePop();
		}

		void upload_changes (OpenglRenderer& r, Game& game);

		// update things and upload changes to gpu
		void update (OpenglRenderer& r, Game& game, Input& I);

		void set_uniforms (OpenglRenderer& r, Game& game, Shader* shad);
		void draw (OpenglRenderer& r, Game& game);

	
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

			bool vct_conedev (Game& game, Raytracer& rt) {
				if (!ImGui::TreeNodeEx("vct_conedev")) return false;

				lrgba cols[] = {
					{1,0,0,1},
					{0,1,0,1},
					{0,0,1,1},
					{1,1,0,1},
					{1,0,1,1},
					{0,1,1,1},
				};

				ImGui::Checkbox("draw cones", &draw_cones);
				ImGui::SameLine();
				ImGui::Checkbox("draw boxes", &draw_boxes);
				ImGui::SliderFloat("start_dist", &start_dist, 0.05f, 2);

				int set_count = (int)sets.size();
				ImGui::DragInt("sets", &set_count, 0.01f);
				sets.resize(set_count);

				rt.cone_data.count = 0;

				float total_weight = 0;

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

				ImGui::TreePop();
				return count_changed;
			}
		};
		Conedev conedev;
		
	};

} // namespace gl
