#pragma once
#include "stdafx.hpp"
#include "blocks.hpp"
#include "util/allocator.hpp"
#include "voxel_mesher.hpp"
#include "worldgen_dll.hpp"

#include "immintrin.h"

struct World;
struct WorldGenerator;

namespace svo {

	static inline constexpr int MAX_DEPTH = 20;
	static inline constexpr int MAX_CHUNKS = 256 * 1024;
	static inline constexpr int MAX_NODES = 256 * 1024;

	static inline constexpr int3 children_pos[8] = {
		int3(0,0,0),
		int3(1,0,0),
		int3(0,1,0),
		int3(1,1,0),
		int3(0,0,1),
		int3(1,0,1),
		int3(0,1,1),
		int3(1,1,1),
	};
	static inline constexpr float3 corners[8] = {
		float3(0,0,0),
		float3(1,0,0),
		float3(0,1,0),
		float3(1,1,0),
		float3(0,0,1),
		float3(1,0,1),
		float3(0,1,1),
		float3(1,1,1),
	};
	static inline lrgba cols[] = {
		srgba(255,0,0),
		srgba(0,255,0),
		srgba(0,0,255),
		srgba(255,255,0),
		srgba(255,0,255),
		srgba(0,255,255),
		srgba(127,0,255),
		srgba(255,0,127),
		srgba(255,127,255),
	};

	inline int get_child_index (int x, int y, int z, int size) {
		//// Subpar asm generated for a lot of these, at least from what I can tell, might need to look into this if I octree decent actually becomes a bottleneck
		//return	(((pos.x >> scale) & 1) << 0) |
		//		(((pos.y >> scale) & 1) << 1) |
		//		(((pos.z >> scale) & 1) << 2);
		//return	((pos.x >> (scale  )) & 1) |
		//		((pos.y >> (scale-1)) & 2) |
		//		((pos.z >> (scale-2)) & 4);
		int ret = 0;
		if (x & size) ret |= 1;
		if (y & size) ret |= 2;
		if (z & size) ret |= 4;
		return ret;
	}

	typedef uint32_t Voxel;

	enum VoxelType : uint32_t { // 2 bit
		BLOCK_ID		=0, // block_id leaf node
		NODE_PTR		=1, // uint32_t node index into current chunk
		CHUNK_PTR		=2, // uint32_t chunk index
	};
	ENUM_BITFLAG_OPERATORS_TYPE(VoxelType, uint32_t)

	static constexpr uint16_t ONLY_BLOCK_IDS = 0;

	struct TypedVoxel {
		VoxelType	type;
		Voxel		value;
	};

	struct Node {
		uint16_t children_types;
		uint16_t _pad[1];
		Voxel children[8];

		TypedVoxel get_child (int child_idx) {
			return { VoxelType((children_types >> child_idx*2) & 3), children[child_idx] };
		}
		void set_child (int child_idx, TypedVoxel vox) {
			children_types &= ~(3u << child_idx*2);
			children_types |= vox.type << child_idx*2;
			children[child_idx] = vox.value;
		}
	};

	enum ChunkFlags : uint8_t {
		LOCKED = 1, // read only, still displayed but not allowed to be modified
		SVO_DIRTY = 2, // svo data changed, reupload to gpu
		MESH_DIRTY = 4, // change that requires remesh
	};
	ENUM_BITFLAG_OPERATORS_TYPE(ChunkFlags, uint8_t)

	struct Chunk {
		Node*	nodes = nullptr;

		const int3		pos;
		const uint8_t	scale;

		ChunkFlags flags = (ChunkFlags)0;

		uint32_t alloc_ptr = 0; // in nodes
		uint32_t commit_ptr = 0; // in bytes

		uint32_t svo_data_size = 0;
		uint32_t opaque_vertex_count = 0;
		uint32_t transparent_vertex_count = 0;

		GLuint gl_svo_data = 0;
		GLuint gl_mesh = 0; // SSBO

		GLuint64EXT gl_svo_data_ptr = 0;
		
		Chunk (int3 pos, uint8_t scale): pos{pos}, scale{scale} {}

		~Chunk () {
			if (gl_svo_data) {
				glDeleteBuffers(1, &gl_svo_data);
				gl_svo_data = 0;
			}
			if (gl_mesh) {
				glDeleteBuffers(1, &gl_mesh);
				gl_mesh = 0;
			}
		}
	};
	static constexpr auto _sz = sizeof(Chunk);

	typedef int4 ChunkKey; // xyz: pos, w: scale

	template <typename VALT>
	struct ChunkHashmap {
		std::unordered_map<ChunkKey, VALT> chunks;

		bool get (int3 pos, int scale, VALT* val) {
			auto it = chunks.find(int4(pos, scale));
			if (it != chunks.end()) {
				*val = it->second;
				return true;
			} else {
				return false;
			}
		}
		VALT* get (int3 pos, int scale) {
			auto it = chunks.find(int4(pos, scale));
			if (it != chunks.end()) {
				return &it->second;
			} else {
				return nullptr;
			}
		}
		bool contains (int3 pos, int scale) {
			VALT val;
			return get(pos, scale, &val);
		}
		// insert new, error if already exist 
		void insert (int3 pos, int scale, VALT chunk) {
			assert(!contains(pos, scale));
			chunks.emplace(int4(pos, scale), chunk);
		}
		// remove, return old if did exist
		VALT remove (int3 pos, int scale) {
			auto it = chunks.find(int4(pos, scale));
			if (it != chunks.end()) {
				VALT val = it->second;
				chunks.erase(it);
				return val;
			} else {
				return VALT();
			}
		}

		int count () {
			return (int)chunks.size();
		}

		struct Iterator {
			typename decltype(chunks)::iterator it;

			Chunk* operator* () { return it->second; }
			Iterator& operator++ () {
				++it;
				return *this;
			}
			bool operator!= (Iterator const& r) { return it != r.it; }
			bool operator== (Iterator const& r) { return it != r.it; }
		};
		Iterator begin () {
			Iterator it = { chunks.begin() };
			return it;
		}
		Iterator end () {
			return { chunks.end() };
		}
	};

	struct Allocator {
		Chunk*				chunks;
		Node*				nodes;

		char*				commit_ptr; // end of commited chunks, in bytes

		Bitset				free_chunks;
		int					chunk_count = 0;

		uint32_t			node_ptrs_ssbo_length = 0;
		GLuint				node_ptrs_ssbo;

		uint32_t indexof (Chunk* chunk) {
			return (uint32_t)(chunk - chunks);
		}
		uint32_t indexof (Chunk* chunk, Node* node) {
			return (uint32_t)(node - chunk->nodes);
		}

		uint32_t comitted_chunk_count () {
			return (uint32_t)free_chunks.bits.size() * 64;
		}
		bool chunk_is_allocated (uint32_t idx) {
			return (free_chunks.bits[idx / 64] & (1ull << (idx % 64))) == 0;
		}

		Allocator () {
			assert((sizeof(Node) * MAX_NODES) % os_page_size == 0);

			chunks = (Chunk*)reserve_address_space((uintptr_t)MAX_CHUNKS * sizeof(Chunk));
			nodes = (Node*)reserve_address_space((uintptr_t)MAX_CHUNKS * MAX_NODES * sizeof(Node));
			commit_ptr = (char*)chunks;

			glGenBuffers(1, &node_ptrs_ssbo);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, node_ptrs_ssbo);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
		~Allocator () {
			glDeleteBuffers(1, &node_ptrs_ssbo);
			node_ptrs_ssbo = 0;

			for (uint32_t i=0; i<comitted_chunk_count(); ++i) {
				if (chunk_is_allocated(i)) {
					chunks[i].~Chunk();
				}
			}

			release_address_space(chunks, (uintptr_t)MAX_CHUNKS * sizeof(Chunk));
			release_address_space(nodes, (uintptr_t)MAX_CHUNKS * MAX_NODES * sizeof(Node));
		}

		// Allocate first possible slot
		Chunk* alloc_chunk (int3 pos, int scale) {
			if (chunk_count >= MAX_CHUNKS) {
				assert(chunk_count < MAX_CHUNKS);
				throw new std::runtime_error("max chunks reached!");
			}

			int idx = free_chunks.clear_first_1();
			chunk_count++;

			Chunk* chunk = &chunks[idx];

			// commit page if all page slots were free before
			if ((char*)(chunk + 1) > commit_ptr) {
				commit_pages(commit_ptr, os_page_size);
				commit_ptr += os_page_size;
			}

			new (chunk) Chunk (pos, (uint8_t)scale);
			chunk->nodes = nodes + (uintptr_t)idx * MAX_NODES;
			return chunk;
		}

		// Free random slot
		void free_chunk (Chunk* chunk) {
			free_nodes(chunk);
			chunk->~Chunk();
		#if DBG_MEMSET
			memset(chunk, DBG_MEMSET_VAL, sizeof(Chunk));
		#endif

			int idx = (int)indexof(chunk);

			free_chunks.set_bit(idx);
			chunk_count--;

			int last_alloc = free_chunks.bitscan_reverse_0(); // -1 on all free

			// decommit last page(s)
			Chunk* last_chunk = &chunks[last_alloc];
			while ((char*)(last_chunk + 1) <= commit_ptr - os_page_size) {
				commit_ptr -= os_page_size;
				decommit_pages(commit_ptr, os_page_size);
			}
		}

		void grow_nodes (Chunk* chunk) {
			if (chunk->alloc_ptr >= MAX_NODES) {
				assert(chunk->alloc_ptr < MAX_NODES);
				throw new std::runtime_error("max nodes reached!");
			}

			char* new_page = (char*)chunk->nodes + chunk->commit_ptr;

			commit_pages(new_page, os_page_size);
			chunk->commit_ptr += os_page_size;
		}
		Node* alloc_node (Chunk* chunk, uint32_t* idx) {
			uint32_t new_idx = chunk->alloc_ptr++;

			if (chunk->alloc_ptr * sizeof(Node) > chunk->commit_ptr) {
				grow_nodes(chunk);
			}

			*idx = new_idx;
			return chunk->nodes + new_idx;
		}
		void free_nodes (Chunk* chunk) {
			decommit_pages(chunk->nodes, MAX_NODES * sizeof(Node));
			chunk->alloc_ptr = 0;
			chunk->commit_ptr = 0;
		}

		void imgui_print_free_slots () {
			for (int i=0; i<free_chunks.bits.size(); ++i) {
				char bits[64+1];
				bits[64] = '\0';
				for (int j=0; j<64; ++j)
					bits[j] = (free_chunks.bits[i] & (1ull << j)) ? '_':'#';
				ImGui::Text(bits);
			}
		}
	};
	
	// how far to move in blocks in the opposite direction of a root move before it happens again, to prevent unneeded root moves
	static constexpr float ROOT_MOVE_HISTER = 20;

	struct StackNode {
		// parent node
		Node* node;
		// child info
		int3 pos;
		int child_idx;
	};

	inline int set_root_scale = 12;

	struct OctreeReadResult {
		TypedVoxel	vox;
		int			size;
		int			lod;
	};

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

		Allocator				allocator;

		float load_lod_start = 100.0f;
		float load_lod_unit = 200.0f;

		// artifically limit both the size of the async queue and how many results to take from the results
		int cap_chunk_load = 64;
		// artifically limit (delay) meshing of chunks to prevent complete freeze of main thread at the cost of some visual artefacts
		int cap_chunk_mesh = max(parallelism_threads*2, 4); // max is 2 meshings per cpu core per frame

		float3 root_move_hister = 0;

		bool debug_draw_chunks = false;
		bool debug_draw_chunks_onlyz0 = 1;//false;

		bool debug_draw_svo = false;
		bool debug_draw_air = false;

		float debug_draw_inset = 0.05f;
		int debug_draw_octree_min = 3;
		int debug_draw_octree_max = 20;
		float debug_draw_octree_range = 100;

		bool debug_spam_place_block = false;
		int debug_spam_place_block_per_frame = 10;

		void imgui () {
			if (!imgui_push("SVO")) return;

			ImGui::DragFloat("load_lod_start", &load_lod_start, 1, 0, 1024);
			ImGui::DragFloat("load_lod_unit", &load_lod_unit, 1, 16, 1024);

			ImGui::DragInt("cap_chunk_load", &cap_chunk_load, 0.02f);
			ImGui::DragInt("cap_chunk_mesh", &cap_chunk_mesh, 0.02f);

			ImGui::DragInt("root_scale", &set_root_scale);
			
			ImGui::Checkbox("debug_draw_chunks", &debug_draw_chunks);
			ImGui::Checkbox("debug_draw_chunks_onlyz0", &debug_draw_chunks_onlyz0);
			
			ImGui::Checkbox("debug_draw_svo", &debug_draw_svo);
			ImGui::Checkbox("debug_draw_air", &debug_draw_air);
			
			ImGui::SliderFloat("debug_draw_inset", &debug_draw_inset, 0, 10, "%7.5f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderInt("debug_draw_octree_min", &debug_draw_octree_min, 0,20);
			ImGui::SliderInt("debug_draw_octree_max", &debug_draw_octree_max, 0,20);
			ImGui::SliderFloat("debug_draw_octree_range", &debug_draw_octree_range, 0,2048, "%f", 2);
			
			ImGui::Checkbox("debug_spam_place_block", &debug_spam_place_block);
			ImGui::SliderInt("debug_spam_place_block_per_frame", &debug_spam_place_block_per_frame, 0,200);

			uintptr_t chunks_count = 0;
			uintptr_t active_nodes = 0;
			uintptr_t commit_bytes = 0;
			uintptr_t mesh_vertices = 0;
			uintptr_t mesh_bytes = 0;

			for (auto* chunk : chunks) {
				chunks_count++;
				active_nodes += chunk->alloc_ptr;
				commit_bytes += chunk->commit_ptr;
				mesh_vertices += chunk->opaque_vertex_count + chunk->transparent_vertex_count;
				mesh_bytes += (chunk->opaque_vertex_count + chunk->transparent_vertex_count) * sizeof(VoxelInstance);
			}

			uintptr_t active_bytes = active_nodes * sizeof(Node);

			ImGui::Text("Active chunks:      %7d (structs take ~%.2f KB)", chunks_count, (float)(allocator.commit_ptr - (char*)allocator.chunks) / 1024);
			ImGui::Text("SVO Nodes: active:  %7.2f k", (float)active_nodes / 1000);
			ImGui::Text("SVO mem: committed: %7.2f MB  wasted: %5.2f%%",
				(float)active_bytes / 1024 / 1024, (float)(commit_bytes - active_bytes) / commit_bytes * 100);
			ImGui::Text("Meshing: vertices: %7.2f M  mem: %7.2f MB",
				(float)mesh_vertices / 1000 / 1000, (float)mesh_bytes / 1024 / 1024);

			ImGui::Text("Root chunk: active:   %5d", root->alloc_ptr);
			
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

				ImGui::Text("[ index]                     pos    size -- alloc B | commit B  wasted    mesh O | T");
				for (auto* chunk : loaded_chunks) {
					ImGui::Text("[%6d] %+7d,%+7d,%+7d %7d -- %7d | %7d   %2.0f %%     %5d | %5d", allocator.indexof(chunk),
						chunk->pos.x,chunk->pos.y,chunk->pos.z, 1 << chunk->scale, chunk->alloc_ptr * sizeof(Node), chunk->commit_ptr,
						(float)(chunk->commit_ptr - chunk->alloc_ptr * sizeof(Node)) / chunk->commit_ptr * 100,
						chunk->opaque_vertex_count, chunk->transparent_vertex_count);
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
			
			uint32_t root_idx;

			root = allocator.alloc_chunk(root_pos, root_scale);
			Node* root_node = allocator.alloc_node(root, &root_idx);

			*root_node = { ONLY_BLOCK_IDS }; // clear to all B_NULL block ids
		}

		void update_chunk_loading (World& world);

		void update_chunk_gpu_data (World& world, Graphics& graphics);

		void update_chunk_loading_and_meshing (World& world, Graphics& graphics);

		void chunk_to_octree (struct Chunk* chunk, block_id* blocks);

		// octree write, writes a single voxel at a desired pos, scale to be a particular leaf val
		// this decends the octree from the root and inserts or deletes nodes when needed
		// (ex. writing a 4x4x4 area to be air will delete the nodes of smaller scale contained)
		void __vectorcall octree_write (int x, int y, int z, int scale, TypedVoxel vox);

		// target_scale=-1: stop at chunk
		OctreeReadResult __vectorcall octree_read (int x, int y, int z, int target_scale, bool read_chunk=false);
	};

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

	struct ChunkLoadJob : ThreadingJob {
		// input
		Chunk*			chunk;
		SVO&			svo;
		WorldGenerator&	wg;
		LoadOp::Type	load_type;

		ChunkLoadJob (Chunk* chunk, SVO& svo, WorldGenerator& wg, LoadOp::Type load_type):
			chunk{chunk}, svo{svo}, wg{wg}, load_type{load_type} {}
		virtual ~ChunkLoadJob() = default;

		virtual void execute ();
		virtual void finalize ();
	};

	struct RemeshChunkJob : ThreadingJob {
		// input
		Chunk* chunk;
		SVO& svo;
		Graphics const& g;
		uint64_t world_seed;
		// output
		std::vector<VoxelInstance> opaque_mesh;
		std::vector<VoxelInstance> transparent_mesh;

		RemeshChunkJob (Chunk* chunk, SVO& svo, Graphics const& g, uint64_t world_seed):
				chunk{chunk}, svo{svo}, g{g}, world_seed{world_seed} {
			opaque_mesh		.reserve(6 * CHUNK_SIZE*CHUNK_SIZE);
			transparent_mesh.reserve(6 * CHUNK_SIZE*CHUNK_SIZE);
		}
		virtual ~RemeshChunkJob() = default;

		virtual void execute () {
			remesh_chunk(chunk, svo, g, world_seed, opaque_mesh, transparent_mesh);
		}
		virtual void finalize ();

	};
}
using svo::SVO;
using svo::Chunk;
