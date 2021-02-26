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

	void imgui (Chunks& chunks) {

		size_t vertices = 0;
		size_t slices_total = 0;
		for (chunk_id cid=0; cid<chunks.end(); ++cid) {
			if (chunks[cid].flags == 0) continue;

			vertices += chunks[cid].opaque_mesh.vertex_count;
			vertices += chunks[cid].transparent_mesh.vertex_count;

			for (int i=0; i<MAX_CHUNK_SLICES; ++i)
				if (chunks[cid].opaque_mesh.slices[i] != U16_NULL) slices_total++;
			for (int i=0; i<MAX_CHUNK_SLICES; ++i)
				if (chunks[cid].transparent_mesh.slices[i] != U16_NULL) slices_total++;
		}

		ImGui::Separator();

		ImGui::Text("Drawcalls: opaque: %3d  transparent: %3d (%3d / %3d slices - %3.0f%%)",
			drawcount_opaque, drawcount_transparent, drawcount_opaque + drawcount_transparent,
			slices_total, (float)(drawcount_opaque + drawcount_transparent) / slices_total * 100);

		ImGui::Text("Mesh allocs: %2d  slices: %5d  vertices: %12s",
			allocs.size(), slices_total, format_thousands(vertices).c_str());
		ImGui::Text("Mesh VRAM: used: %7.3f MB  commited: %7.3f MB (%6.2f%% usage)",
			(float)(vertices * sizeof(BlockMeshInstance)) / 1024 / 1024,
			(float)(allocs.size() * ALLOC_SIZE) / 1024 / 1024,
			(float)(vertices * sizeof(BlockMeshInstance)) / (float)(allocs.size() * ALLOC_SIZE) * 100);

		if (ImGui::TreeNode("slices alloc")) {
			print_bitset_allocator(chunks.slices_alloc, CHUNK_SLICE_BYTESIZE, ALLOC_SIZE);
			ImGui::TreePop();
		}

	}

	void upload_remeshed (Chunks& chunks);

	void draw_chunks (OpenglRenderer& r, Game& game);

};

struct Raytracer {
	//SERIALIZE(Raytracer, enable)

	Shader* shad_test;

	template <typename T>
	struct SSBO {
		uint32_t	count; // how many allocated on gpu
		Ssbo		ssbo;

		SSBO (std::string_view label): ssbo{label} {}
	};

	SSBO<Chunk> chunks_ssbo = {"Raytracer.chunks_ssbo"};

	bool enable = false;

	void imgui () {
		ImGui::Separator();
		//if (!ImGui::TreeNode("Raytracer")) return;

		ImGui::Checkbox("enable", &enable);

		//ImGui::TreePop();
	}

	Raytracer (Shaders& shaders) {
		shad_test = shaders.compile("raytracer_test", {}, {{ COMPUTE_SHADER }});

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
