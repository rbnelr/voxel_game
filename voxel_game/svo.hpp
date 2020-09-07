#pragma once
#include "stdafx.hpp"
#include "blocks.hpp"
#include "util/virtual_allocator.hpp"
#include "world_generator.hpp"
#include "svo_alloc.hpp"

class Voxels;
class Player;
struct WorldGenerator;

struct WorldgenJob;

namespace svo {
	
	// how far to move in blocks in the opposite direction of a root move before it happens again, to prevent unneeded root moves
	static constexpr float ROOT_MOVE_HISTER = 20;

	struct LoadOp {
		Chunk* chunk;
		int3 pos;
		int scale;
		float dist;

		enum Type {
			CREATE,
			SPLIT,
			MERGE
		} type;
	};

	inline int set_root_scale = 16;

	struct SVO {
		Chunk*	root;

		// all loaded chunks, get uploaded to gpu and displayed in debug view,
		// can be not actually written into the SVO, when still waiting on siblings to finish for split
		ChunkHashmap<Chunk*>	chunks;
		// chunks still in the process of being async loaded
		ChunkHashmap<Chunk*>	pending_chunks;
		// parents of all chunks in chunks hashmap, with count of direct (not futher split) children as the value ([0,8])
		// when count is 8 this parent could merge it's children into itself and become a real chunk
		ChunkHashmap<int>		parent_chunks;

		SVOAllocator			allocator;

		float3 root_move_hister = 0;

		bool debug_draw_chunks = 1;//false;
		bool debug_draw_chunks_onlyz0 = 1;//false;
		bool debug_draw_svo = false;
		bool debug_draw_air = false;
		int debug_draw_octree_min = 3;
		int debug_draw_octree_max = 20;
		float debug_draw_octree_range = 100;

		void imgui () {
			if (!imgui_push("SVO")) return;

			ImGui::DragInt("root_scale", &set_root_scale);
			ImGui::Checkbox("debug_draw_chunks", &debug_draw_chunks);
			ImGui::Checkbox("debug_draw_chunks_onlyz0", &debug_draw_chunks_onlyz0);
			ImGui::Checkbox("debug_draw_svo", &debug_draw_svo);
			ImGui::Checkbox("debug_draw_air", &debug_draw_air);
			ImGui::SliderInt("debug_draw_octree_min", &debug_draw_octree_min, 0,20);
			ImGui::SliderInt("debug_draw_octree_max", &debug_draw_octree_max, 0,20);
			ImGui::SliderFloat("debug_draw_octree_range", &debug_draw_octree_range, 0,2048, "%f", 2);

			uintptr_t chunks_count = 0;
			uintptr_t active_nodes = 0;
			uintptr_t commit_nodes = 0;

			for (auto* chunk : chunks) {
				chunks_count++;
				active_nodes += chunk->alloc_ptr;
				commit_nodes += chunk->commit_ptr;
			}

			ImGui::Text("Active chunks:        %5d (structs take ~%.2f KB)", chunks_count, (float)(allocator.commit_ptr * sizeof(Chunk)) / 1024);
			ImGui::Text("SVO Nodes: active:    %5d k   committed: %5d k  avg/chunk: %.0f | %.0f",
				active_nodes / 1000, commit_nodes / 1000, (float)active_nodes / chunks_count, (float)commit_nodes / chunks_count);
			ImGui::Text("SVO mem: committed: %7.2f MB  wasted:    %5.2f%%",
				(float)(commit_nodes * sizeof(Node)) / 1024 / 1024, (float)(commit_nodes - active_nodes) / commit_nodes * 100);

			ImGui::Text("Root chunk: active:   %5d     committed: %5d", root->alloc_ptr, root->commit_ptr);
			
			if (ImGui::TreeNode("Show all chunks")) {
				std::vector<Chunk*> loaded_chunks;
				for (auto* chunk : chunks)
					loaded_chunks.push_back(chunk);

				std::sort(loaded_chunks.begin(), loaded_chunks.end(), [] (Chunk* l, Chunk* r) {
					if (l->scale != r->scale) return std::less<int>()(l->scale, r->scale);
					if (l->pos.z != r->pos.z) return std::less<int>()(l->pos.z, r->pos.z);
					if (l->pos.y != r->pos.y) return std::less<int>()(l->pos.y, r->pos.y);
					if (l->pos.x != r->pos.x) return std::less<int>()(l->pos.x, r->pos.x);
					return false;
				});

				ImGui::Text("[ index]                     pos    size -- alloc | commit");
				for (auto* chunk : loaded_chunks) {
					ImGui::Text("[%6d] %+7d,%+7d,%+7d %7d -- %5d | %5d", allocator.indexof(chunk),
						chunk->pos.x,chunk->pos.y,chunk->pos.z, 1 << chunk->scale, chunk->alloc_ptr, chunk->commit_ptr);
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Show allocator")) {
				allocator.imgui_print_free_slots();
				ImGui::TreePop();
			}

			imgui_pop();
		}

		SVO () {
			uint8_t root_scale = (uint8_t)set_root_scale;
			int3 root_pos = -(1 << (root_scale - 1));
			
			root = allocator.alloc_chunk();
			new (root) Chunk (root_pos, root_scale);
			root->alloc_ptr++;
			new (&root->nodes[0]) Node ();
		}

		void chunk_loading (Voxels& voxels, Player& player, WorldGenerator& world_gen);

		void chunk_to_octree (Chunk* chunk, block_id* blocks);

		// octree write, writes a single voxel at a desired pos, scale to be a particular leaf val
		// this decends the octree from the root and inserts or deletes nodes when needed
		// (ex. writing a 4x4x4 area to be air will delete the nodes of smaller scale contained)
		void octree_write (int3 pos, int scale, uint16_t val);

		block_id octree_read (int3 pos);
	};

	struct ChunkLoadJob : ThreadingJob {
		// input
		Chunk*			chunk;
		SVO&			svo;
		WorldGenerator&	world_gen;
		LoadOp::Type	load_type;

		ChunkLoadJob (Chunk* chunk, SVO& svo, WorldGenerator& world_gen, LoadOp::Type load_type):
			chunk{chunk}, svo{svo}, world_gen{world_gen}, load_type{load_type} {}

		virtual void execute () {
			generate_chunk(chunk, svo, world_gen);
		}
		virtual void finalize ();
	};
}
using svo::SVO;
using svo::Chunk;
