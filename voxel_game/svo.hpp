#pragma once
#include "stdafx.hpp"
#include "blocks.hpp"
#include "util/virtual_allocator.hpp"

class Voxels;
class Player;
struct WorldGenerator;

static constexpr int CHUNK_SIZE = 64;
static constexpr uint32_t CHUNK_SCALE = 6;

static constexpr uint32_t MAX_NODES = 1u << 16;
static constexpr uint32_t MAX_CHUNKS = 1u << 16;

namespace svo {

	static constexpr uint32_t MAX_DEPTH = 20;
	
	// how far to move in blocks in the opposite direction of a root move before it happens again, to prevent unneeded root moves
	static constexpr float ROOT_MOVE_HISTER = 20;

	struct Node {
		uint16_t children[8] = {};
		uint8_t leaf_mask = 0xff;
		uint8_t pad[15];
	};
	static_assert(is_pot(sizeof(Node)), "sizeof(Node) must be power of two!");

	struct StackNode {
		Node* node; // parent node
		int3 pos;
		int child_indx;
	};

	struct SVO;

	struct Chunk {
		Node* nodes;

		int3	pos;
		uint8_t	scale;

		bool gpu_dirty;

		uint32_t alloc_ptr;
		uint32_t dead_count; // TODO: cant actually keep track of nodes that disappear because of octree_writes with scale != 0, so maybe just keep a write counter that then occasionally triggers compactings
		uint32_t commit_ptr;

		char _pad[28];

		Node tinydata[6];

		Chunk (int3	pos, uint8_t scale): pos{pos}, scale{scale} {
			nodes = tinydata;

			gpu_dirty = true;

			alloc_ptr = 0;
			dead_count = 0;
			commit_ptr = (uint32_t)ARRLEN(tinydata);

		#if DBG_MEMSET
			memset(tinydata, DBG_MEMSET_VAL, sizeof(tinydata));
		#endif
		}

		uint16_t alloc_node (SVO& svo, StackNode* stack=nullptr);
	};
	static constexpr uint32_t _sz = sizeof(Chunk);

	struct AllocBlock {
		Node nodes[MAX_NODES];
	};
	static constexpr uint32_t _sz2 = sizeof(AllocBlock);

	struct SVO {
		Chunk*	root;

		std::unordered_map<vector_key<int3>, Chunk*> active_chunks; // chunks not currently in threadpools
		std::unordered_map<vector_key<int3>, Chunk*> pending_chunks;

		SparseAllocator<Chunk>				chunk_allocator = { MAX_CHUNKS };
		SparseAllocator<AllocBlock, false>	node_allocator = { MAX_CHUNKS };

		float3 root_move_hister = 0;

		bool debug_draw_chunks = false;
		bool debug_draw_svo = false;
		bool debug_draw_air = false;
		int debug_draw_octree_min = 3;
		int debug_draw_octree_max = 20;

		void imgui () {
			if (!imgui_push("SVO")) return;

			ImGui::Checkbox("debug_draw_chunks", &debug_draw_chunks);
			ImGui::Checkbox("debug_draw_svo", &debug_draw_svo);
			ImGui::Checkbox("debug_draw_air", &debug_draw_air);
			ImGui::SliderInt("debug_draw_octree_min", &debug_draw_octree_min, 0,20);
			ImGui::SliderInt("debug_draw_octree_max", &debug_draw_octree_max, 0,20);

			uintptr_t chunks_count = 0;
			uintptr_t active_nodes = 0;
			uintptr_t commit_nodes = 0;

			for (auto& it : active_chunks) {
				chunks_count++;
				active_nodes += it.second->alloc_ptr;
				commit_nodes += it.second->commit_ptr;
			}

			ImGui::Text("Active chunks:        %5d", chunks_count);
			ImGui::Text("SVO Nodes: active:    %5d k   committed: %5d k  avg/chunk: %.0f | %.0f",
				active_nodes / 1000, commit_nodes / 1000, (float)active_nodes / chunks_count, (float)commit_nodes / chunks_count);
			ImGui::Text("SVO mem: committed: %7.2f MB  wasted:    %5.2f%%",
				(float)(commit_nodes * sizeof(Node)) / 1024 / 1024, (float)(commit_nodes - active_nodes) / commit_nodes * 100);

			ImGui::Text("Root chunk: active:   %5d     committed: %5d", root->alloc_ptr, root->commit_ptr);
			
			if (ImGui::TreeNode("Show all chunks")) {
				for (auto& it : active_chunks) {
					ImGui::Text("%5d | %5d", it.second->alloc_ptr, it.second->commit_ptr);
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Show allocators")) {
				ImGui::Text("chunk_allocator:\n%s", chunk_allocator.dbg_string_free_slots().c_str());
				ImGui::Text("node_allocator:\n%s", node_allocator .dbg_string_free_slots().c_str());
				ImGui::TreePop();
			}

			imgui_pop();
		}

		SVO () {
			uint8_t root_scale = 10;
			int3 root_pos = -(1 << (root_scale - 1));
			
			root = chunk_allocator.alloc();
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
}
using svo::SVO;
using svo::Chunk;
