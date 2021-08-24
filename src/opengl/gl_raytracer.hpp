#pragma once
#include "common.hpp"
#include "opengl_helper.hpp"
#include "opengl_shaders.hpp"
#include "assets.hpp"
#include "game.hpp"

namespace gl {
	class OpenglRenderer;

	static constexpr int GPU_WORLD_SIZE_CHUNKS = 8;
	static constexpr int GPU_WORLD_SIZE = GPU_WORLD_SIZE_CHUNKS * CHUNK_SIZE;

	// Render arbitrary meshses into gbuffer to test combining rasterized and raytraced objects
	struct TestRenderer {
		Shader*		shad;
		IndexedMesh	mesh;

		float3 pos = float3(-18.3f,0,40);
		float2 rot = float3(deg(-90),0,0);
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

	struct Raytracer {
		SERIALIZE(Raytracer, enable, max_iterations)
		
		Shader* rt_forward = nullptr;
		
		VoxelTexture voxel_tex;
		DFTexture df_tex;

		TestRenderer test_renderer;

		ComputeGroupSize rt_groupsz = int2(8,8);

		OGL_TIMER_HISTOGRAM(rt);
		OGL_TIMER_HISTOGRAM(df_init);

		std::vector<gl::MacroDefinition> get_forward_macros () {
			return { {"WORLD_SIZE_CHUNKS", prints("%d", GPU_WORLD_SIZE_CHUNKS)},
			         {"WG_PIXELS_X", prints("%d", rt_groupsz.size.x)},
			         {"WG_PIXELS_Y", prints("%d", rt_groupsz.size.y)},
			         {"VISUALIZE_COST", visualize_cost ? "1":"0"},
			         {"VISUALIZE_WARP_COST", visualize_warp_cost ? "1":"0"}
			};
		}

		Raytracer (Shaders& shaders): df_tex(shaders), test_renderer(shaders) {
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

		bool visualize_cost = false;
		bool visualize_warp_cost = false;

		bool macro_change = false; // shader macro change
		void imgui (Input& I) {
			if (!ImGui::TreeNodeEx("Raytracer", ImGuiTreeNodeFlags_DefaultOpen)) return;

			OGL_TIMER_HISTOGRAM_UPDATE(rt, I.dt)
			OGL_TIMER_HISTOGRAM_UPDATE(df_init, I.dt)

			ImGui::Checkbox("enable [R]", &enable);

			ImGui::SliderInt("max_iterations", &max_iterations, 1, 1024, "%4d", ImGuiSliderFlags_Logarithmic);

			macro_change |= ImGui::Checkbox("visualize_cost", &visualize_cost);
			ImGui::SameLine();
			macro_change |= ImGui::Checkbox("warp_cost", &visualize_warp_cost);

			macro_change |= rt_groupsz.imgui("rt_groupsz");

			ImGui::Separator();

			//ImGui::SliderFloat("vct_diff_renderscale", &vct_diff_framebuf.renderscale, .5f, 1, "%.2f");

			test_renderer.imgui();

			//
			ImGui::TreePop();
		}

		void upload_changes (OpenglRenderer& r, Game& game);

		// update things and upload changes to gpu
		void update (OpenglRenderer& r, Game& game, Input& I);

		void draw (OpenglRenderer& r, Game& game);
	};

} // namespace gl
