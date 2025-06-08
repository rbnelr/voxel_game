#pragma once
#include "common.hpp"
#include "opengl_helper.hpp"
#include "opengl_shaders.hpp"
#include "assets.hpp"
#include "game.hpp"

namespace gl {
class OpenglRenderer;

// Rasterized chunk renderer
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

	std::vector<AllocBlock>	allocs;

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

	float detail_draw_dist = 200;

	float water_scrolling_t = 0;
	
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
		
		ImGui::DragFloat("detail_draw_dist", &detail_draw_dist);
	}

	void upload_remeshed (Chunks& chunks);

	void draw_chunks (OpenglRenderer& r, Game& game);

};

} // namespace gl
