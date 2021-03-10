#pragma once
#include "common.hpp"
#include "chunks.hpp"
#include "assets.hpp"

struct WorldGenerator;
struct Assets;

struct ChunkSliceData {
	BlockMeshInstance verts[CHUNK_SLICE_LENGTH];
};

struct ChunkMeshData {
	BlockMeshInstance* next_ptr = nullptr;
	BlockMeshInstance* alloc_end = nullptr;

	std::vector<ChunkSliceData*> slices;

	ChunkMeshData () {
		slices.reserve(32);
	}

	uint32_t vertex_count () {
		return (uint32_t)slices.size() * CHUNK_SLICE_LENGTH - (uint32_t)(alloc_end - next_ptr);
	}

	void alloc_slice () {
		ZoneScopedC(tracy::Color::Crimson);

		auto* s = (ChunkSliceData*)malloc(sizeof(ChunkSliceData));

		next_ptr  = s->verts;
		alloc_end = s->verts + CHUNK_SLICE_LENGTH;

		slices.push_back(s);
	}
	static void free_slice (ChunkSliceData* s) {
		if (s) {
			ZoneScopedC(tracy::Color::Crimson);
			::free(s);
		}
	}

	// forceinline because this is doing nothing but an if and a increment 99% of the time, compiler should keep alloc_slice not inlined instead
	__forceinline BlockMeshInstance* push () {
		if (next_ptr != alloc_end) {
			// likely case
		} else {
			// unlikely case
			alloc_slice();
		}
		return next_ptr++;
	}
};

struct RemeshChunkJob { // Chunk remesh
	//// input data
	// LUTs
	BlockTypes::Block const*	block_types;
	int const*					block_meshes;
	BlockMeshes::Mesh const*	block_meshes_meshes;
	BlockTile const*			block_tiles;

	ChunkVoxels*				chunk_voxels;
	SubchunkVoxels*				subchunks;

	chunk_id					chunk;

	// chunk neighbours (neg dir)
	chunk_id					chunk_nx;
	chunk_id					chunk_ny;
	chunk_id					chunk_nz;

	bool						mesh_world_border;
	uint64_t					chunk_seed;

	//// output data
	ChunkMeshData				opaque_vertices;
	ChunkMeshData				transp_vertices;

	RemeshChunkJob (Chunks& chunks, chunk_id cid, WorldGenerator const& wg, bool mesh_world_border);

	void execute ();
};

inline auto parallelism_threadpool = Threadpool<RemeshChunkJob>(parallelism_threads, TPRIO_PARALLELISM, ">> parallelism threadpool" ); // parallelism_threads - 1 to let main thread contribute work too
