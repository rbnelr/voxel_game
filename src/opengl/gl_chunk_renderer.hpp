#pragma once
#include "common.hpp"
#include "opengl_helper.hpp"
#include "opengl_shaders.hpp"
#include "assets.hpp"
#include "game.hpp"

namespace gl {
class OpenglRenderer;

struct ChunkRenderer {
	SERIALIZE(ChunkRenderer, _draw_chunks)

	static constexpr int SLICES_PER_ALLOC = 1024;
	static constexpr size_t ALLOC_SIZE = SLICES_PER_ALLOC * CHUNK_SLICE_SIZE; // size of vram allocations

	static constexpr size_t LIGHTING_VBO_SLICE_SIZE = CHUNK_SLICE_LENGTH * sizeof(float4);
	static constexpr size_t LIGHTING_VBO_SIZE = SLICES_PER_ALLOC * LIGHTING_VBO_SLICE_SIZE;

	enum DrawType { DT_OPAQUE=0, DT_TRANSPARENT=1 };

	struct AllocBlock {
		Vao vao;
		Vbo vbo;
		Vbo lighting_vbo;

		AllocBlock () {
			ZoneScopedC(tracy::Color::Crimson);
			OGL_TRACE("AllocBlock()");

			vbo = Vbo("ChunkRenderer.AllocBlock.vbo");

			//vao = setup_vao<BlockMeshInstance>("ChunkRenderer.vao", vbo);
			vao = { "ChunkRenderer.vao" };
			glBindVertexArray(vao);

			//
			glBindBuffer(GL_ARRAY_BUFFER, vbo);
			glBufferData(GL_ARRAY_BUFFER, ALLOC_SIZE, nullptr, GL_STREAM_DRAW);

			VertexAttributes a;
			BlockMeshInstance::attributes(a);

			//
			lighting_vbo = Vbo("ChunkRenderer.AllocBlock.lighting_vbo");

			glBindBuffer(GL_ARRAY_BUFFER, lighting_vbo);
			glBufferData(GL_ARRAY_BUFFER, LIGHTING_VBO_SIZE, nullptr, GL_STREAM_DRAW);

			int loc = 3;
			glEnableVertexAttribArray(loc);
			glVertexAttribPointer(loc, 4, GL_FLOAT, false, sizeof(float4), 0);
			glVertexAttribDivisor(loc, 1);


			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
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
			print_bitset_allocator(chunks.slices.slots, CHUNK_SLICE_SIZE, ALLOC_SIZE);
			ImGui::TreePop();
		}

	}

	void upload_remeshed (Chunks& chunks);

	void draw_chunks (OpenglRenderer& r, Game& game);

};

static constexpr int GPU_WORLD_SIZE = 16;

struct ChunkOctrees {
	static constexpr int MAX_NODES = 1<<16;
	static constexpr int CHUNK_OCTREE_LAYERS = CHUNK_SIZE_SHIFT;

	static constexpr int TEX_WIDTH = GPU_WORLD_SIZE * CHUNK_SIZE/2;

	Texture3D tex = {"RT.octrees"};

	ChunkOctrees () {
		glTextureStorage3D(tex, CHUNK_OCTREE_LAYERS, GL_R8UI, TEX_WIDTH,TEX_WIDTH,TEX_WIDTH);

		uint8_t val = 0;
		for (int layer=0; layer<CHUNK_OCTREE_LAYERS; ++layer)
			glClearTexImage(tex, layer, GL_RED_INTEGER, GL_UNSIGNED_BYTE, &val);
	}

	struct OctreeBuilder {
		/*	Temporary buffer for building SVO that's not yet sparse
			final step is to sparsify it, by recursing this structure and copying out the relevant nodes

			nodes contains:

			voxel[32][32][32]
			voxel[16][16][16]
			voxel[ 8][ 8][ 8]
			voxel[ 4][ 4][ 4]
			voxel[ 2][ 2][ 2]
			voxel[ 1][ 1][ 1]

			offsets contains: nodes start index for every layer (32-layer, 16-layer, ...)
		*/

		int offsets[CHUNK_OCTREE_LAYERS];
		std::vector<uint8_t> voxels;

		uint8_t& get_vox (int layer, int x, int y, int z) {
			return voxels[offsets[layer] + IDX3D(x,y,z, (CHUNK_SIZE/2) >> layer)];
		}

		int node_count;

		OctreeBuilder () {
			int count = 0;

			for (int layer=CHUNK_OCTREE_LAYERS-1; layer>=0; --layer) {
				int size = (CHUNK_SIZE/2) >> layer;
				offsets[layer] = count;
				count += size*size*size;
			}

			voxels.resize(count);
		}
	};
	OctreeBuilder builder;

	void rebuild_chunk (Chunks& chunks, chunk_id cid, int3 cpos) {
		ZoneScoped;
		
		{
			ZoneScopedN("fill layer 0");
			auto bid_air = g_assets.block_types.map_id("air");

			for (int z=0; z<CHUNK_SIZE/2; z++)
			for (int y=0; y<CHUNK_SIZE/2; y++)
			for (int x=0; x<CHUNK_SIZE/2; x++) {
				uint8_t mask = 0;
				for (int i=0; i<8; ++i) {
					auto vox = chunks.read_block(x*2 + (i&1), y*2 + ((i>>1)&1), z*2 + (i>>2), cid);
					mask |= (vox != bid_air ? 1:0) << i;
				}
				builder.get_vox(0, x,y,z) = mask;
			}
		}

		{
			ZoneScopedN("fill other layers");

			for (int layer=1; layer<CHUNK_OCTREE_LAYERS; ++layer) {

				int size = (CHUNK_SIZE/2) >> layer;
				for (int z=0; z<size; z++)
				for (int y=0; y<size; y++)
				for (int x=0; x<size; x++) {
					uint8_t mask = 0;
					for (int i=0; i<8; ++i) {
						auto child = builder.get_vox(layer-1, x*2 + (i&1), y*2 + ((i>>1)&1), z*2 + (i>>2));
						mask |= (child != 0 ? 1:0) << i;
					}
					builder.get_vox(layer, x,y,z) = mask;
				}
			}
		}

		glBindTexture(GL_TEXTURE_3D, tex);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // required for the higher mip leves
		glPixelStorei(GL_PACK_ALIGNMENT, 1);

		for (int layer=0; layer<CHUNK_OCTREE_LAYERS; ++layer) {
			int size = (CHUNK_SIZE/2) >> layer;
			glTexSubImage3D(GL_TEXTURE_3D, layer,
				cpos.x*size, cpos.y*size, cpos.z*size,
				size, size, size,
				GL_RED_INTEGER, GL_UNSIGNED_BYTE, &builder.voxels[builder.offsets[layer]]);
		}

		glTextureParameteri(tex, GL_TEXTURE_BASE_LEVEL, 0);
		glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, CHUNK_OCTREE_LAYERS-1);

		glBindTexture(GL_TEXTURE_3D, 0);
	}
};

struct Raytracer {
	SERIALIZE(Raytracer, enable, enable_rt_lighting, max_iterations, rand_seed_time,
		sunlight_enable, sunlight_dist, sunlight_col,
		bounces_enable, bounces_max_dist, bounces_max_count,
		only_primary_rays, taa_alpha)

	Shader* shad = nullptr;
	Shader* shad_lighting = nullptr;

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
			tex = {"RT.subchunks"};

			glTextureStorage3D(tex, 1, GL_R32UI, GPU_WORLD_SIZE*SUBCHUNK_COUNT, GPU_WORLD_SIZE*SUBCHUNK_COUNT, GPU_WORLD_SIZE*SUBCHUNK_COUNT);

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

	VoxTexture<block_id, SUBCHUNK_SIZE>		voxels_tex = {"RT.voxels" };

	ChunkOctrees							octree;

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

	float taa_alpha = 0.05;
	float taa_alpha_rt_light = 0.01;

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

	float3 sunlight_pos = float3(-28, 67, 102);
	float sun_pos_size = 4.0;

	float2 sunlight_ang = float2(deg(60), deg(35));
	float sun_dir_rand = 0.05;

	bool sunlight_mode = 1;


	bool  bounces_enable = false;
	float bounces_max_dist = 64;
	int   bounces_max_count = 4;

	float water_F0 = 0.05f;

	//int   rays = 1;

	bool  visualize_light = false;

	bool  only_primary_rays = true;

	bool  update_debug_rays = false;
	bool  clear_debug_rays = false;

	std::vector<gl::MacroDefinition> get_macros () {
		return { {"LOCAL_SIZE_X", prints("%d", compute_local_size.x)},
		         {"LOCAL_SIZE_Y", prints("%d", compute_local_size.y)},
			     {"ONLY_PRIMARY_RAYS", only_primary_rays ? "1":"0"},
			     {"SUNLIGHT_MODE", sunlight_mode ? "1":"0"},
			     {"VISUALIZE_COST", visualize_cost ? "1":"0"},
			     {"VISUALIZE_WARP_COST", visualize_warp_iterations ? "1":"0"},
			     {"VISUALIZE_WARP_READS", visualize_warp_reads ? "1":"0"}};
	}

	int lighting_workgroup_size = 64;
	int lighting_samples = 16;
	int lighting_update_r = 1;

	std::vector<gl::MacroDefinition> get_lighting_macros () {
		return { {"WORKGROUP_SIZE", prints("%d", lighting_workgroup_size)},
			     {"SUNLIGHT_MODE", sunlight_mode ? "1":"0"} };
	}

	bool enable = true;
	bool enable_rt_lighting = true;

	void imgui () {
		if (!ImGui::TreeNodeEx("Raytracer", ImGuiTreeNodeFlags_DefaultOpen)) return;

		ImGui::Checkbox("enable [R]", &enable);

		ImGui::Checkbox("update_debug_rays [T]", &update_debug_rays);
		clear_debug_rays = ImGui::Button("clear_debug_rays") || clear_debug_rays;

		ImGui::SliderFloat("taa_alpha", &taa_alpha, 0,1, "%f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("taa_alpha_rt_light", &taa_alpha_rt_light, 0,1, "%f", ImGuiSliderFlags_Logarithmic);

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

		ImGui::Separator();

		//
		if (ImGui::TreeNodeEx("real_time_lighting")) {
			ImGui::Checkbox("sunlight_enable", &sunlight_enable);
			ImGui::SliderFloat("sunlight_dist", &sunlight_dist, 1, 128);
			imgui_ColorEdit("sunlight_col", &sunlight_col);

			ImGui::DragFloat3("sunlight_pos", &sunlight_pos.x, 0.25f);
			ImGui::DragFloat("sun_pos_size", &sun_pos_size, 0.1f, 0, 100, "%f", ImGuiSliderFlags_Logarithmic);

			ImGui::SliderAngle("sunlight_ang.x", &sunlight_ang.x, -180, +180);
			ImGui::SliderAngle("sunlight_ang.y", &sunlight_ang.y, -90, +90);
			ImGui::DragFloat("sun_dir_rand", &sun_dir_rand, 0.001f, 0, 0.5f, "%f", ImGuiSliderFlags_Logarithmic);

			macro_change = ImGui::Checkbox("sunlight_mode", &sunlight_mode) || macro_change;

			ImGui::Spacing();

			float2 sunlight_ang = float2(deg(0), deg(40));
			float sun_dir_rand = 4.0;

			ImGui::Checkbox("bounces_enable", &bounces_enable);
			ImGui::SliderFloat("bounces_max_dist", &bounces_max_dist, 1, 128);
			ImGui::SliderInt("bounces_max_count", &bounces_max_count, 1, 16);
			ImGui::Spacing();

			//ImGui::SliderInt("rays", &rays, 1, 16, "%d", ImGuiSliderFlags_Logarithmic);
			ImGui::Checkbox("visualize_light", &visualize_light);

			ImGui::SliderFloat("water_F0", &water_F0, 0, 1);
			
			ImGui::TreePop();
		}

		ImGui::Checkbox("enable_rt_lighting", &enable_rt_lighting);

		if (ImGui::TreeNodeEx("cached_lighting")) {
			ImGui::SliderInt("lighting_samples", &lighting_samples, 1, 128);
			ImGui::SliderInt("lighting_update_r", &lighting_update_r, 0, 5);
			
			ImGui::TreePop();
		}

		if (macro_change && shad) {
			shad->macros = get_macros();
			shad->recompile("macro_change", false);

			shad_lighting->macros = get_lighting_macros();
			shad_lighting->recompile("macro_change", false);
		}

		if (ImGui::Button("Dump asm")) {
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

	void upload_changes (OpenglRenderer& r, Game& game, Input& I);

	void setup_shader (OpenglRenderer& r, Shader* shad, bool rt_light);
	void draw (OpenglRenderer& r, Game& game);
	void compute_lighting (OpenglRenderer& r, Game& game);
};


} // namespace gl
