#pragma once
#include "common.hpp"
#include "opengl_helper.hpp"
#include "opengl_shaders.hpp"
#include "assets.hpp"
#include "game.hpp"

namespace gl {
class OpenglRenderer;

struct ChunkRenderer {
	static constexpr uint64_t ALLOC_SIZE = 64 * (1024ull * 1024); // size of vram allocations
	static constexpr int SLICES_PER_ALLOC = (int)(ALLOC_SIZE / CHUNK_SLICE_BYTESIZE);

	enum DrawType { DT_OPAQUE=0, DT_TRANSPARENT=1 };

	struct AllocBlock {
		Vao vao;
		Vbo vbo;

		AllocBlock () {
			ZoneScopedC(tracy::Color::Crimson);
			OGL_TRACE("AllocBlock()");

			vbo = Vbo("ChunkRenderer.AllocBlock.vbo");
			vao = setup_vao<BlockMeshInstance>("ChunkRenderer.vao", vbo);

			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, ALLOC_SIZE, nullptr, GL_STREAM_DRAW);
		}

		struct DrawList {
			struct DrawSlice {
				uint16_t	vertex_count;
				uint16_t	slice_idx; // in alloc (not global slice id)
				chunk_id	chunk;
			};
			int				count;
			DrawSlice		slices[SLICES_PER_ALLOC];
		};

		DrawList		draw_lists[2];
	};

	std_vector<AllocBlock>	allocs;

	Shader* shad_opaque;
	Shader* shad_transparent;

	PipelineState state_opaque;
	PipelineState state_transparant;

	ChunkRenderer (Shaders& shaders) {
		shad_opaque			= shaders.compile("chunks", {{"ALPHA_TEST", "1"}});
		shad_transparent	= shaders.compile("chunks", {{"ALPHA_TEST", "0"}});

		state_opaque.depth_test		= true;
		state_opaque.depth_write	= true;
		state_opaque.blend_enable	= false;

		state_transparant.depth_test	= true;
		state_transparant.depth_write	= true;
		state_transparant.blend_enable	= true;
	}

	int drawcount_opaque = 0;
	int drawcount_transparent = 0;
	size_t draw_instances = 0;

	bool _draw_chunks = true; // allow disabling for debugging
	
	void imgui (Chunks& chunks) {

		size_t vertices = 0;
		size_t slices_total = 0;

		for (chunk_id cid=0; cid<chunks.end(); ++cid) {
			if (chunks[cid].flags == 0) continue;

			vertices += chunks[cid].opaque_mesh_vertex_count;
			vertices += chunks[cid].transp_mesh_vertex_count;

			slices_total += _slices_count(chunks[cid].opaque_mesh_vertex_count);
			slices_total += _slices_count(chunks[cid].transp_mesh_vertex_count);
		}

		size_t draw_vertices = draw_instances * BlockMeshes::MERGE_INSTANCE_FACTOR;

		ImGui::Separator();

		ImGui::Text("Drawcalls: opaque: %3d  transparent: %3d (%3d / %3d slices - %3.0f%%)",
			drawcount_opaque, drawcount_transparent, drawcount_opaque + drawcount_transparent,
			slices_total, (float)(drawcount_opaque + drawcount_transparent) / slices_total * 100);

		ImGui::Text("Vertex workload : drawn instances: %12s (vertices: %12s)",
			format_thousands(draw_instances).c_str(), format_thousands(draw_vertices).c_str());

		ImGui::Text("Mesh allocs: %2d  slices: %5d  vertices: %12s",
			allocs.size(), slices_total, format_thousands(vertices).c_str());
		ImGui::Text("Mesh VRAM: used: %7.3f MB  commited: %7.3f MB (%6.2f%% usage)",
			(float)(vertices * sizeof(BlockMeshInstance)) / 1024 / 1024,
			(float)(allocs.size() * ALLOC_SIZE) / 1024 / 1024,
			(float)(vertices * sizeof(BlockMeshInstance)) / (float)(allocs.size() * ALLOC_SIZE) * 100);

		if (ImGui::TreeNode("slices alloc")) {
			print_bitset_allocator(chunks.slices.slots, CHUNK_SLICE_BYTESIZE, ALLOC_SIZE);
			ImGui::TreePop();
		}

	}

	void upload_remeshed (Chunks& chunks);

	void draw_chunks (OpenglRenderer& r, Game& game);

};

struct Raytracer {
	//SERIALIZE(Raytracer, enable)

	Shader* shad;

	static constexpr int TEX3D_SIZE = 2048; // width, height for 3d textures

	static constexpr int SUBCHUNK_TEX_COUNT = TEX3D_SIZE / SUBCHUNK_SIZE; // max num of subchunks in one axis for tex
	
	static constexpr int SUBCHUNK_TEX_SHIFT = 8;
	static constexpr int SUBCHUNK_TEX_MASK  = (SUBCHUNK_TEX_COUNT-1) << SUBCHUNK_SHIFT;

	static_assert((1 << SUBCHUNK_TEX_SHIFT) == SUBCHUNK_TEX_COUNT, "");
	static_assert(SUBCHUNK_SIZE == SUBCHUNK_COUNT, "");

	// subchunk id to 3d tex offset (including subchunk_size multiplication)
	static int3 subchunk_id_to_texcoords (uint32_t id) {
		int3 coord;
		coord.x = (id << (SUBCHUNK_TEX_SHIFT*0 + SUBCHUNK_SHIFT)) & SUBCHUNK_TEX_MASK;
		coord.y = (id >> (SUBCHUNK_TEX_SHIFT*1 - SUBCHUNK_SHIFT)) & SUBCHUNK_TEX_MASK;
		coord.z = (id >> (SUBCHUNK_TEX_SHIFT*2 - SUBCHUNK_SHIFT)) & SUBCHUNK_TEX_MASK;
		return coord;
	}

	struct SubchunksTexture {
		Texture3D	tex;

		SubchunksTexture () {
			tex = {"Raytracer.subchunks_tex"};

			glTextureStorage3D(tex, 1, GL_R32UI, 32*SUBCHUNK_COUNT, 32*SUBCHUNK_COUNT, 32*SUBCHUNK_COUNT);

			GLuint val = SUBC_SPARSE_BIT | (uint32_t)B_NULL;
			glClearTexImage(tex, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, &val);
		}
	};

	template <typename T, int SZ>
	struct VoxTexture {
		std::string label;

		uint32_t	alloc_layers = 0;
		Texture3D	tex;

		VoxTexture (std::string_view label): label{label} {}

		void resize (uint32_t new_count) {
			uint32_t layer_sz = (TEX3D_SIZE/SZ) * (TEX3D_SIZE/SZ);
			// round up size to avoid constant resizing, ideally only happens sometimes
			uint32_t layers = std::max((new_count + (layer_sz-1)) / layer_sz, 1u);

			if (alloc_layers == layers)
				return;

			Texture3D new_tex = { (std::string_view)label };

			size_t sz = (size_t)TEX3D_SIZE * TEX3D_SIZE * layers*SZ * sizeof(T);
			clog(INFO, ">> Resized %s 3d texture to %dx%dx%d (%d MB)", label.c_str(), TEX3D_SIZE, TEX3D_SIZE, layers*SZ, (int)(sz / MB));

			glTextureStorage3D(new_tex, 1, sizeof(T) == 2 ? GL_R16UI : GL_R32UI, TEX3D_SIZE, TEX3D_SIZE, layers*SZ);

			{ // copy old tex data to new bigger tex
				if (alloc_layers)
					assert(tex != 0);

				uint32_t copy_layers = std::min(alloc_layers, layers);
				if (copy_layers > 0) {
					glCopyImageSubData(tex,     GL_TEXTURE_3D, 0, 0,0,0,
						new_tex, GL_TEXTURE_3D, 0, 0,0,0, TEX3D_SIZE, TEX3D_SIZE, copy_layers*SZ);
				}
			}

			alloc_layers = layers;
			tex = std::move(new_tex);
		}

		void upload (uint32_t idx, T* data) {
			int3 coord = subchunk_id_to_texcoords(idx);

			glTexSubImage3D(GL_TEXTURE_3D, 0,
				coord.x, coord.y, coord.z, SZ, SZ, SZ,
				GL_RED_INTEGER, sizeof(T) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, data);
		}
	};

	SubchunksTexture						subchunks_tex;

	VoxTexture<block_id, SUBCHUNK_SIZE>		voxels_tex = {"Raytracer.voxels_tex" };

	struct FramebufferTex {
		GLuint tex = 0;
		GLuint fbo;
		int2   size;

		void resize (int2 new_size, int idx) {
			if (tex == 0 || size != new_size) {
				if (tex) { // delete old
					glDeleteTextures(1, &tex);
					glDeleteFramebuffers(1, &fbo);
				}

				size = new_size;

				// create new (textures created with glTexStorage2D cannot be resized)
				glGenTextures(1, &tex);

				glBindTexture(GL_TEXTURE_2D, tex);
				glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, size.x, size.y);
				glBindTexture(GL_TEXTURE_2D, 0);

				glGenFramebuffers(1, &fbo);
				glBindFramebuffer(GL_FRAMEBUFFER, fbo);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

				OGL_DBG_LABEL(GL_TEXTURE    , tex, prints("Raytracer.framebuffers[%d].tex", idx).c_str());
				OGL_DBG_LABEL(GL_FRAMEBUFFER, fbo, prints("Raytracer.framebuffers[%d].fbo", idx).c_str());

				GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
				if (status != GL_FRAMEBUFFER_COMPLETE) {
					fprintf(stderr, "glCheckFramebufferStatus: %x\n", status);
				}
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			}
		}
	};
	FramebufferTex framebuffers[2];
	int cur_frambuffer = 0;

	float reprojection_alpha = 0.05;

	float4x4 prev_world2clip;
	bool init = true;

	//
	int max_iterations = 512;

	bool rand_seed_time = true;

	bool visualize_cost = false;
	bool visualize_warp_iterations = false;
	bool visualize_warp_reads = false;

	//
	int _im_selection = 2;
	static constexpr const char* _im_options = "4x4\0" "8x4\0" "8x8\0" "16x8\0" "16x16\0" "32x16\0";
	static constexpr int2 _im_sizes[] = { int2(4,4), int2(8,4), int2(8,8), int2(16,8), int2(16,16), int2(32,16), };

	int2 compute_local_size = int2(8,8);

	//
	bool  sunlight_enable = true;
	float sunlight_dist = 90;
	lrgb  sunlight_col = lrgb(0.98, 0.92, 0.65) * 1.3;

	bool  bounces_enable = false;
	float bounces_max_dist = 64;
	int   bounces_max_count = 4;

	lrgb  ambient_col = lrgb(0.5, 0.8, 1.0) * 0.8;
	float ambient_factor = 0.00f;

	float water_F0 = 0.05f;

	//int   rays = 1;

	bool  visualize_light = false;

	bool  only_primary_rays = true;

	std::vector<gl::MacroDefinition> get_macros () {
		return { {"LOCAL_SIZE_X", prints("%d", compute_local_size.x)},
		         {"LOCAL_SIZE_Y", prints("%d", compute_local_size.y)},
			     {"ONLY_PRIMARY_RAYS", only_primary_rays ? "1":"0"},
			     {"VISUALIZE_COST", visualize_cost ? "1":"0"},
			     {"VISUALIZE_WARP_COST", visualize_warp_iterations ? "1":"0"},
			     {"VISUALIZE_WARP_READS", visualize_warp_reads ? "1":"0"}};
	}

	bool enable = true;

	void imgui () {
		if (!ImGui::TreeNodeEx("Raytracer", ImGuiTreeNodeFlags_DefaultOpen)) return;

		ImGui::Checkbox("enable", &enable);

		ImGui::SliderFloat("reprojection_alpha", &reprojection_alpha, 0,1, "%f", ImGuiSliderFlags_Logarithmic);

		ImGui::SliderInt("max_iterations", &max_iterations, 1, 1024, "%4d", ImGuiSliderFlags_Logarithmic);
		ImGui::Checkbox("rand_seed_time", &rand_seed_time);

		bool macro_change = false;

		macro_change = ImGui::Checkbox("visualize_cost", &visualize_cost) || macro_change;
		ImGui::SameLine();
		macro_change = ImGui::Checkbox("warp_iterations", &visualize_warp_iterations) || macro_change;
		ImGui::SameLine();
		macro_change = ImGui::Checkbox("warp_reads", &visualize_warp_reads) || macro_change;

		if (ImGui::Combo("compute_local_size", &_im_selection, _im_options) && shad) {
			macro_change = true;
			compute_local_size = _im_sizes[_im_selection];
		}

		macro_change = ImGui::Checkbox("only_primary_rays", &only_primary_rays) || macro_change;

		if (macro_change && shad) {
			shad->macros = get_macros();
			shad->recompile("macro_change", false);
		}

		ImGui::Separator();

		//
		if (ImGui::TreeNodeEx("lighting")) {
			ImGui::Checkbox("sunlight_enable", &sunlight_enable);
			ImGui::SliderFloat("sunlight_dist", &sunlight_dist, 1, 128);
			imgui_ColorEdit("sunlight_col", &sunlight_col);
			ImGui::Spacing();

			ImGui::SliderFloat("ambient_factor", &ambient_factor, 0, 1.0f, "%f", ImGuiSliderFlags_Logarithmic);
			imgui_ColorEdit("ambient_col", &ambient_col);
			ImGui::Spacing();

			ImGui::Checkbox("bounces_enable", &bounces_enable);
			ImGui::SliderFloat("bounces_max_dist", &bounces_max_dist, 1, 128);
			ImGui::SliderInt("bounces_max_count", &bounces_max_count, 1, 16);
			ImGui::Spacing();

			//ImGui::SliderInt("rays", &rays, 1, 16, "%d", ImGuiSliderFlags_Logarithmic);
			ImGui::Checkbox("visualize_light", &visualize_light);

			ImGui::SliderFloat("water_F0", &water_F0, 0, 1);
			
			ImGui::TreePop();
		}

		if (ImGui::Button("Dump PTX")) {
			GLsizei length;
			glGetProgramiv(shad->prog, GL_PROGRAM_BINARY_LENGTH, &length);

			char* buf = (char*)malloc(length);

			GLenum format;
			glGetProgramBinary(shad->prog, length, &length, &format, buf);

			save_binary_file("../raytracer.glsl.asm", buf, length);

			free(buf);
		}
		
		//
		ImGui::TreePop();
	}

	Raytracer (Shaders& shaders) {
		shad = shaders.compile("raytracer", get_macros(), {{ COMPUTE_SHADER }});

		if (0) {
			int3 count, size;

			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &count.x);
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &count.y);
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &count.z);

			printf("max global (total) work group count (%d, %d, %d)\n", count.x, count.y, count.z);

			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &size.x);
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &size.y);
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &size.z);

			printf("max local (in one shader) work group size (%d, %d, %d)\n", size.x, size.y, size.z);

			int number;
			glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &number);
			printf("max local work group invocations %d\n", number);
		}

		if (0) {
			
			int max_sparse_texture_size;
			int max_sparse_3d_texture_size;
			int max_sparse_array_texture_layers;
			int sparse_texture_full_array_cube_mipmaps;

			glGetIntegerv(GL_MAX_SPARSE_TEXTURE_SIZE_ARB                 , &max_sparse_texture_size);
			glGetIntegerv(GL_MAX_SPARSE_3D_TEXTURE_SIZE_ARB              , &max_sparse_3d_texture_size);
			glGetIntegerv(GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS_ARB         , &max_sparse_array_texture_layers);
			glGetIntegerv(GL_SPARSE_TEXTURE_FULL_ARRAY_CUBE_MIPMAPS_ARB  , &sparse_texture_full_array_cube_mipmaps);

			GLint page_sizes;
			glGetInternalformativ(GL_TEXTURE_3D, GL_R32UI, GL_NUM_VIRTUAL_PAGE_SIZES_ARB, 1, &page_sizes);

			std::vector<GLint> sizes_x(page_sizes), sizes_y(page_sizes), sizes_z(page_sizes);
			glGetInternalformativ(GL_TEXTURE_3D, GL_R32UI, GL_VIRTUAL_PAGE_SIZE_X_ARB, page_sizes, sizes_x.data());
			glGetInternalformativ(GL_TEXTURE_3D, GL_R32UI, GL_VIRTUAL_PAGE_SIZE_Y_ARB, page_sizes, sizes_y.data());
			glGetInternalformativ(GL_TEXTURE_3D, GL_R32UI, GL_VIRTUAL_PAGE_SIZE_Z_ARB, page_sizes, sizes_z.data());

			Texture3D tex = {"test"};

			glBindTexture(GL_TEXTURE_3D, tex);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_SPARSE_ARB, GL_TRUE);
			glTexParameteri(GL_TEXTURE_3D, GL_VIRTUAL_PAGE_SIZE_INDEX_ARB, 0);

			glTextureStorage3D(tex, 1, GL_R32UI, 32*SUBCHUNK_COUNT, 32*SUBCHUNK_COUNT, 32*SUBCHUNK_COUNT);
			
			printf("...\n");
		}
	}

	void upload_changes (OpenglRenderer& r, Game& game);

	void draw (OpenglRenderer& r, Game& game);
};


} // namespace gl
