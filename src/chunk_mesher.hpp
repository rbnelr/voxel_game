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

	int used_slices = 0;

	uint32_t vertex_count () {
		return used_slices * CHUNK_SLICE_LENGTH - (uint32_t)(alloc_end - next_ptr);
	}

	ChunkSliceData* slices[MAX_CHUNK_SLICES] = {};

	void alloc_slice () {
		ZoneScopedC(tracy::Color::Crimson);
		if (used_slices >= MAX_CHUNK_SLICES)
			return;
			//throw std::runtime_error("exceeded MAX_CHUNK_SLICES!");
		auto* s = (ChunkSliceData*)malloc(sizeof(ChunkSliceData));

		next_ptr  = s->verts;
		alloc_end = s->verts + CHUNK_SLICE_LENGTH;

		slices[used_slices++] = s;
	}
	static void free_slice (ChunkSliceData* s) {
		if (s) {
			ZoneScopedC(tracy::Color::Crimson);
			::free(s);
		}
	}

	// forceinline because this is doing nothing but an if and a increment 99% of the time, compiler should keep alloc_slice not inlined instead
	__forceinline BlockMeshInstance* push () {
		if (used_slices >= MAX_CHUNK_SLICES)
			return nullptr;

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

	ChunkVoxels*				dense_chunks;
	SubchunkVoxels*				dense_subchunks;

	Chunk*						chunk;

	// chunk neighbours (neg dir)
	Chunk const*				chunk_nx;
	Chunk const*				chunk_ny;
	Chunk const*				chunk_nz;

	bool						mesh_world_border;
	uint64_t					chunk_seed;

	//// output data
	ChunkMeshData				opaque_vertices;
	ChunkMeshData				tranparent_vertices;

	RemeshChunkJob (Chunks& chunks, Chunk* chunk, WorldGenerator const& wg, bool mesh_world_border);

	void execute ();
};

inline auto parallelism_threadpool = Threadpool<RemeshChunkJob>(parallelism_threads, TPRIO_PARALLELISM, ">> parallelism threadpool" ); // parallelism_threads - 1 to let main thread contribute work too
