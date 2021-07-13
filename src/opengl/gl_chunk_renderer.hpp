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

	enum DrawType { DT_OPAQUE=0, DT_TRANSPARENT=1 };

	struct AllocBlock {
		Vao vao;
		Vbo vbo;

		AllocBlock () {
			ZoneScopedC(tracy::Color::Crimson);
			OGL_TRACE("AllocBlock()");

			vbo = Vbo("ChunkRenderer.AllocBlock.vbo");
			vao = setup_vao<BlockMeshInstance>("ChunkRenderer.vao", vbo);

			//
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
			print_bitset_allocator(chunks.slices.slots, CHUNK_SLICE_SIZE, ALLOC_SIZE);
			ImGui::TreePop();
		}

	}

	void upload_remeshed (Chunks& chunks);

	void draw_chunks (OpenglRenderer& r, Game& game);

};

static constexpr int GPU_WORLD_SIZE_CHUNKS = 8;
static constexpr int GPU_WORLD_SIZE = GPU_WORLD_SIZE_CHUNKS * CHUNK_SIZE;

struct ChunkOctrees {
	static constexpr int TEX_WIDTH = GPU_WORLD_SIZE;

	static constexpr int COMPUTE_FILTER_LOCAL_SIZE = 4;

	static constexpr int OCTREE_MIPS = get_const_log2((uint32_t)TEX_WIDTH)+1;

	// how many octree layers to filter per uploaded chunk (rest are done for whole world)
	// only compute mips per chunk until dipatch size is 4^3, to not waste dispatches for workgroups with only 1 or 2 active threads
	static constexpr int OCTREE_FILTER_CHUNK_MIPS = get_const_log2((uint32_t)(CHUNK_SIZE/2 / 4))+1;

	Texture3D tex = {"RT.octree"};
	Shader* octree_filter;
	Shader* octree_filter_mip0;

	ChunkOctrees (Shaders& shaders) {
		octree_filter      = shaders.compile("rt_octree_filter", {{"MIP0","0"}}, {COMPUTE_SHADER});
		octree_filter_mip0 = shaders.compile("rt_octree_filter", {{"MIP0","1"}}, {COMPUTE_SHADER});

		glTextureStorage3D(tex, OCTREE_MIPS, GL_R8UI, TEX_WIDTH,TEX_WIDTH,TEX_WIDTH);
		glTextureParameteri(tex, GL_TEXTURE_BASE_LEVEL, 0);
		glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, OCTREE_MIPS-1);

		uint8_t val = (uint8_t)g_assets.block_types.map_id("air");
		for (int layer=0; layer<OCTREE_MIPS; ++layer)
			glClearTexImage(tex, layer, GL_RED_INTEGER, GL_UNSIGNED_BYTE, &val);
	}

	void recompute_mips (OpenglRenderer& r, Game& game, std::vector<int3> const& chunks);
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

	Sampler sampler = {"sampler"};
	Sampler filter_sampler = {"filter_sampler"};

	Shader* filter;
	Shader* filter_mip0;

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
		filter       = shaders.compile("vct_filter", {{"MIP0","0"}}, {COMPUTE_SHADER});
		filter_mip0  = shaders.compile("vct_filter", {{"MIP0","1"}}, {COMPUTE_SHADER});

		lrgba color = lrgba(0,0,0,0);
		
		glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
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
	SERIALIZE(Raytracer, enable, enable_vct, max_iterations, rand_seed_time,
		sunlight_enable, sunlight_dist, sunlight_col,
		bounces_enable, bounces_max_dist, bounces_max_count,
		only_primary_rays, taa_alpha, taa_enable)

	Shader* rt_shad = nullptr;
	Shader* vct_shad = nullptr;
	Vao dummy_vao = {"dummy_vao"};

	struct SubchunksTexture {
		Texture3D	tex;

		SubchunksTexture () {
			tex = {"RT.subchunks"};

			glTextureStorage3D(tex, 1, GL_R32UI, GPU_WORLD_SIZE/SUBCHUNK_SIZE, GPU_WORLD_SIZE/SUBCHUNK_SIZE, GPU_WORLD_SIZE/SUBCHUNK_SIZE);
			glTextureParameteri(tex, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(tex, GL_TEXTURE_MAX_LEVEL, 0);

			GLuint val = SUBC_SPARSE_BIT | (uint32_t)B_NULL;
			glClearTexImage(tex, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, &val);
		}
	};

	template <typename T>
	struct VoxTexture {
		static constexpr int VOXTEX_SIZE = 2048;
		static constexpr int VOXTEX_SIZE_SHIFT = 11;

		static constexpr int VOXTEX_COUNT = VOXTEX_SIZE / SUBCHUNK_SIZE;

		static constexpr int VOXTEX_MASK  = VOXTEX_COUNT -1; // max num of subchunks in one axis for tex
		static constexpr int VOXTEX_SHIFT = VOXTEX_SIZE_SHIFT - SUBCHUNK_SHIFT;

		std::string label;

		uint32_t	alloc_layers = 0;
		Texture3D	tex;

		VoxTexture (std::string_view label): label{label} {}

		void resize (uint32_t new_count) {
			uint32_t layer_sz = VOXTEX_COUNT*VOXTEX_COUNT;
			// round up size to avoid constant resizing, ideally only happens sometimes
			uint32_t layers = std::max((new_count + (layer_sz-1)) / layer_sz, 1u);

			if (alloc_layers == layers)
				return;

			Texture3D new_tex = { (std::string_view)label };

			size_t sz = (size_t)VOXTEX_SIZE * VOXTEX_SIZE * layers*SUBCHUNK_SIZE * sizeof(T);
			clog(INFO, ">> Resized %s 3d texture to %dx%dx%d (%d MB)", label.c_str(), VOXTEX_SIZE, VOXTEX_SIZE, layers*SUBCHUNK_SIZE, (int)(sz / MB));

			glTextureStorage3D(new_tex, 1, sizeof(T) == 2 ? GL_R16UI : GL_R32UI, VOXTEX_SIZE, VOXTEX_SIZE, layers*SUBCHUNK_SIZE);
			glTextureParameteri(new_tex, GL_TEXTURE_BASE_LEVEL, 0);
			glTextureParameteri(new_tex, GL_TEXTURE_MAX_LEVEL, 0);

			{ // copy old tex data to new bigger tex
				if (alloc_layers)
					assert(tex != 0);

				uint32_t copy_layers = std::min(alloc_layers, layers);
				if (copy_layers > 0) {
					glCopyImageSubData(tex,     GL_TEXTURE_3D, 0, 0,0,0,
						new_tex, GL_TEXTURE_3D, 0, 0,0,0, VOXTEX_SIZE, VOXTEX_SIZE, copy_layers*SUBCHUNK_SIZE);
				}
			}

			alloc_layers = layers;
			tex = std::move(new_tex);
		}

		void upload (uint32_t idx, T* data) {
			int3 coord;
			coord.x = ((idx >> (VOXTEX_SHIFT*0)) & VOXTEX_MASK) << SUBCHUNK_SHIFT;
			coord.y = ((idx >> (VOXTEX_SHIFT*1)) & VOXTEX_MASK) << SUBCHUNK_SHIFT;
			coord.z = ((idx >> (VOXTEX_SHIFT*2)) & VOXTEX_MASK) << SUBCHUNK_SHIFT;

			glTexSubImage3D(GL_TEXTURE_3D, 0,
				coord.x, coord.y, coord.z, SUBCHUNK_SIZE, SUBCHUNK_SIZE, SUBCHUNK_SIZE,
				GL_RED_INTEGER, sizeof(T) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, data);
		}
	};

	SubchunksTexture			subchunks_tex;

	VoxTexture<block_id>		voxels_tex = {"RT.voxels" };

	ChunkOctrees				octree;
	VCT_Data					vct_data;

	struct Gbuffer {
		int2						size = 0;
		Texture2D					pos ;
		Texture2D					col ;
		Texture2D					norm;
		Texture2D					tang;

		// keep FBO so we can rasterize into gbuffer
		// compute raytracer gbuf generation has same or slightly better perf
		GLuint						fbo;

		void resize (int2 const& new_size) {
			if (size == new_size) return;
			size = new_size;

			pos   = {"gbuf.pos"  }; // could be computed from depth
			col   = {"gbuf.col"  }; // rgb albedo + emissive multiplier
			norm  = {"gbuf.norm" }; // rgb normal
			tang  = {"gbuf.tang" }; // rgb tang

			//int mips = calc_mipmaps(size.x, size.y);

			glTextureStorage2D(pos , 1, GL_RGBA32F, size.x, size.y);
			glTextureStorage2D(col , 1, GL_RGBA16F, size.x, size.y);
			glTextureStorage2D(norm, 1, GL_RGBA16F, size.x, size.y);
			glTextureStorage2D(tang, 1, GL_RGBA16F, size.x, size.y);

			glDeleteFramebuffers(1, &fbo);
			glGenFramebuffers(1, &fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			OGL_DBG_LABEL(GL_FRAMEBUFFER, fbo, "gbuf.fbo");

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pos, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, col, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, norm, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, tang, 0);
			//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);

			GLenum draw_buffers[] = {
				GL_COLOR_ATTACHMENT0,
				GL_COLOR_ATTACHMENT1,
				GL_COLOR_ATTACHMENT2,
				GL_COLOR_ATTACHMENT3,
			};
			glDrawBuffers(ARRLEN(draw_buffers), draw_buffers);

			//GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			//if (status != GL_FRAMEBUFFER_COMPLETE) {
			//	fprintf(stderr, "glCheckFramebufferStatus: %x\n", status);
			//}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
		~Gbuffer () {
			glDeleteFramebuffers(1, &fbo);
		}
	};
	Gbuffer gbuf;

	void bind_voxel_textures (OpenglRenderer& r, Shader* shad);

	struct TAAHistory {
		GLuint colors[2] = {};
		GLuint posage[2] = {};
		int2   size = 0;
		int    cur = 0;

		Sampler sampler = {"sampler"};
		Sampler sampler_int = {"sampler_int"};

		TAAHistory () {
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

				if (colors[0]) { // delete old
					glDeleteTextures(2, colors);
					glDeleteTextures(2, posage);
				}

				size = new_size;

				// create new (textures created with glTexStorage2D cannot be resized)
				glGenTextures(2, colors);
				glGenTextures(2, posage);

				glBindTexture(GL_TEXTURE_2D, colors[0]);
				glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, size.x, size.y);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

				glBindTexture(GL_TEXTURE_2D, colors[1]);
				glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, size.x, size.y);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

				glBindTexture(GL_TEXTURE_2D, posage[0]);
				glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG16UI, size.x, size.y);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

				glBindTexture(GL_TEXTURE_2D, posage[1]);
				glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG16UI, size.x, size.y);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

				OGL_DBG_LABEL(GL_TEXTURE, colors[0], "RT.taa_color0");
				OGL_DBG_LABEL(GL_TEXTURE, colors[1], "RT.taa_color1");
				OGL_DBG_LABEL(GL_TEXTURE, posage[0], "RT.taa_posage0");
				OGL_DBG_LABEL(GL_TEXTURE, posage[1], "RT.taa_posage1");

				// clear textures to be read on first frame
				float3 col = float3(0,0,0);
				glClearTexImage(colors[0], 0, GL_RGB, GL_FLOAT, &col.x);

				uint32_t pos[2] = { 0xffffu, 0 };
				glClearTexImage(posage[0], 0, GL_RG_INTEGER, GL_UNSIGNED_INT, pos); // Invalid??

				cur = 1;

				glBindTexture(GL_TEXTURE_2D, 0);
			}
		}
	};
	//TAAHistory history;
	//
	bool taa_enable = true;
	float taa_alpha = 0.05;
	//
	//float4x4 prev_world2clip;
	//bool init = true;

	//
	int max_iterations = 512;

	bool rand_seed_time = true;

	bool visualize_cost = false;
	bool visualize_warp_iterations = false;
	bool visualize_warp_reads = false;

	//
	int2 group_size = int2(8,8);
	bool update_group_size = false; // alow editing without instanly updating, as to avoid invalid sizes

	int2 _group_size = int2(8,8);

	//
	bool  sunlight_enable = true;
	float sunlight_dist = 90;
	lrgb  sunlight_col = lrgb(0.98, 0.92, 0.65) * 1.3;

	float3 sunlight_pos = float3(-28, 67, 102);
	float sun_pos_size = 4.0;

	float2 sunlight_ang = float2(deg(60), deg(35));
	float sun_dir_rand = 0.05;

	bool sunlight_mode = 1;


	float vct_start_dist = 1.0f / 64.0f;
	float vct_stepsize = 1.0f;
	float vct_test = 60;
	bool vct_dbg_primary = false;
	bool vct_visualize_sparse = false;
	bool test = false;


	bool  bounces_enable = false;
	float bounces_max_dist = 64;
	int   bounces_max_count = 4;

	float water_F0 = 0.05f;

	bool  visualize_light = false;

	bool  only_primary_rays = true;

	bool  update_debugdraw = false;
	bool  clear_debugdraw = false;

	std::vector<gl::MacroDefinition> get_rt_macros () {
		return { {"WG_PIXELS_X", prints("%d", _group_size.x)},
		         {"WG_PIXELS_Y", prints("%d", _group_size.y)},
		         {"TEST", test ? "1":"0"},
		};
	}
	std::vector<gl::MacroDefinition> get_vct_macros () {
		return { {"WG_PIXELS_X", prints("%d", _group_size.x)},
		         {"WG_PIXELS_Y", prints("%d", _group_size.y)},
		         {"WG_CONES", prints("%d", 12)},
		         {"VCT_DBG_PRIMARY", vct_dbg_primary ? "1":"0"},
		         {"VISUALIZE_COST", visualize_cost ? "1":"0"},
		         {"VISUALIZE_WARP_COST", visualize_warp_iterations ? "1":"0"},
		         {"TEST", test ? "1":"0"},
		};
	}

	bool enable = true;
	bool enable_vct = true;

	void imgui () {
		if (!ImGui::TreeNodeEx("Raytracer", ImGuiTreeNodeFlags_DefaultOpen)) return;

		bool macro_change = false;

		ImGui::Checkbox("enable [R]", &enable);
		macro_change |= ImGui::Checkbox("enable VCT [V]", &enable_vct);

		ImGui::Checkbox("update debugdraw [T]", &update_debugdraw);
		clear_debugdraw = ImGui::Button("clear debugdraw") || clear_debugdraw;

		ImGui::SliderFloat("taa_alpha", &taa_alpha, 0,1, "%f", ImGuiSliderFlags_Logarithmic);
		ImGui::SameLine();
		macro_change |= ImGui::Checkbox("TAA", &taa_enable);

		ImGui::SliderInt("max_iterations", &max_iterations, 1, 1024, "%4d", ImGuiSliderFlags_Logarithmic);
		ImGui::Checkbox("rand_seed_time", &rand_seed_time);

		macro_change |= ImGui::Checkbox("visualize_cost", &visualize_cost);
		ImGui::SameLine();
		macro_change |= ImGui::Checkbox("warp_iterations", &visualize_warp_iterations);

		ImGui::InputInt2("group_size", &group_size.x); ImGui::SameLine(); 
		if (ImGui::Button("Update")) {
			macro_change = true;
			_group_size = group_size;
		}

		macro_change |= ImGui::Checkbox("only_primary_rays", &only_primary_rays);

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

			macro_change |= ImGui::Checkbox("sunlight_mode", &sunlight_mode);

			ImGui::Spacing();

			float2 sunlight_ang = float2(deg(0), deg(40));
			float sun_dir_rand = 4.0;

			ImGui::Checkbox("bounces_enable", &bounces_enable);
			ImGui::SliderFloat("bounces_max_dist", &bounces_max_dist, 1, 512);
			ImGui::SliderInt("bounces_max_count", &bounces_max_count, 1, 16);
			ImGui::Spacing();

			//ImGui::SliderInt("rays", &rays, 1, 16, "%d", ImGuiSliderFlags_Logarithmic);
			ImGui::Checkbox("visualize_light", &visualize_light);

			ImGui::SliderFloat("water_F0", &water_F0, 0, 1);
			
			ImGui::TreePop();
		}

		ImGui::SliderFloat("vct_start_dist", &vct_start_dist, 0, 4, "%.4f", ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("vct_stepsize", &vct_stepsize, 0, 1);
		ImGui::SliderFloat("vct_test", &vct_test, 0, 256);
		macro_change |= ImGui::Checkbox("vct_dbg_primary", &vct_dbg_primary);
		ImGui::Checkbox("vct_visualize_sparse", &vct_visualize_sparse);

		macro_change |= ImGui::Checkbox("test", &test);

		if (macro_change && rt_shad) {
			rt_shad->macros = get_rt_macros();
			rt_shad->recompile("macro_change", false);
		}
		if (macro_change && vct_shad) {
			vct_shad->macros = get_vct_macros();
			vct_shad->recompile("macro_change", false);
		}
		
		//
		ImGui::TreePop();
	}

	Raytracer (Shaders& shaders): octree(shaders), vct_data(shaders) {
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
			
			printf("...\n");
		}
	}

	void upload_changes (OpenglRenderer& r, Game& game, Input& I);

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

	void vct_conedev (OpenglRenderer& r, Game& game);
};


} // namespace gl
