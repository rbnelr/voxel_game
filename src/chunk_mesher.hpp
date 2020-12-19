#pragma once
#include "common.hpp"
#include "chunks.hpp"
#include "graphics.hpp"

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
			throw std::runtime_error("exceeded MAX_CHUNK_SLICES!");
		slices[used_slices++] = (ChunkSliceData*)malloc(sizeof(ChunkSliceData));

		next_ptr  = slices[used_slices-1]->verts;
		alloc_end = slices[used_slices-1]->verts + CHUNK_SLICE_LENGTH;
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

struct RemeshingMesh {
	ChunkMeshData opaque_vertices;
	ChunkMeshData tranparent_vertices;
};

struct RemeshChunkJob { // Chunk remesh
	// input
	Chunk*					chunk;
	Chunks&					chunks;
	// store these three LUTs here instead of Assets* to turn double indirection into single indir
	BlockMeshes::Mesh const*	block_mesh_info;
	int const*					block_meshes;
	BlockTile const*			block_tiles;

	uint64_t				chunk_seed;


	int xfaces_count = 0;
	int yfaces_count = 0;
	int zfaces_count = 0;
	int mesh_voxels_count = 0;

	struct Face {
		int idx;
		block_id id, nid;
	};
	Face xfaces[CHUNK_SIZE*CHUNK_SIZE*CHUNK_SIZE];
	Face yfaces[CHUNK_SIZE*CHUNK_SIZE*CHUNK_SIZE];
	Face zfaces[CHUNK_SIZE*CHUNK_SIZE*CHUNK_SIZE];

	struct MeshVox {
		int idx;
		block_id id;
	};
	MeshVox mesh_voxels[CHUNK_SIZE*CHUNK_SIZE*CHUNK_SIZE];


	// output
	RemeshingMesh			mesh;

	RemeshChunkJob (Chunk* chunk, Chunks& chunks, Assets const& assets, WorldGenerator const& wg);

	void execute ();
};
