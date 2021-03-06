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

	template <typename T>
	struct SSBO {
		uint32_t	count; // how many allocated on gpu
		Ssbo		ssbo;

		SSBO (std::string_view label): ssbo{label} {}
	};

	SSBO<Chunk>				chunks_ssbo				= {"Raytracer.chunks_ssbo"};
	SSBO<ChunkVoxels>		dense_chunks_ssbo		= {"Raytracer.dense_chunks_ssbo"};
	SSBO<SubchunkVoxels>	dense_subchunks_ssbo	= {"Raytracer.dense_subchunks_ssbo"};

	int max_iterations = 256;

	bool visualize_cost = false;
	bool visualize_warp_iterations = false;
	bool visualize_warp_reads = false;

	int2 compute_local_size = int2(8,8);

	int _im_selection = 2;
	static constexpr const char* _im_options = "4x4\0" "8x4\0" "8x8\0" "16x8\0" "16x16\0" "32x16\0";
	static constexpr int2 _im_sizes[] = { int2(4,4), int2(8,4), int2(8,8), int2(16,8), int2(16,16), int2(32,16), };

	std::vector<gl::MacroDefinition> get_macros () {
		return { {"LOCAL_SIZE_X", prints("%d", compute_local_size.x)},
		         {"LOCAL_SIZE_Y", prints("%d", compute_local_size.y)},
			     {"VISUALIZE_COST", visualize_cost ? "1":"0"},
			     {"VISUALIZE_WARP_COST", visualize_warp_iterations ? "1":"0"},
			     {"VISUALIZE_WARP_READS", visualize_warp_reads ? "1":"0"}};
	}

	bool inited = false;
	bool reupload = false;

	bool enable = false;

	void imgui () {
		if (!ImGui::TreeNodeEx("Raytracer", ImGuiTreeNodeFlags_DefaultOpen)) return;

		ImGui::Checkbox("enable", &enable);
		reupload = ImGui::Button("reupload") || reupload;

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

		if (macro_change && shad) {
			shad->macros = get_macros();
			shad->recompile("macro_change", false);
		}

		ImGui::SliderInt("max_iterations", &max_iterations, 1, 1024, "%4d", ImGuiSliderFlags_Logarithmic);

		ImGui::TreePop();
	}

	Raytracer (Shaders& shaders) {
		shad = shaders.compile("raytracer_test", get_macros(), {{ COMPUTE_SHADER }});

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
		
	}

	void draw (OpenglRenderer& r, Game& game);
};


} // namespace gl
