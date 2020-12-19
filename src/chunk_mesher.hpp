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
	}
	static void free_slice (ChunkSliceData* s) {
		if (s) {
			ZoneScopedC(tracy::Color::Crimson);
			::free(s);
		}
	}

	BlockMeshInstance* push () {
		if (next_ptr == alloc_end) {
			alloc_slice();

			next_ptr  = slices[used_slices-1]->verts;
			alloc_end = slices[used_slices-1]->verts + CHUNK_SLICE_LENGTH;
		}
		return next_ptr++;
	}
};

struct RemeshingMesh {
	ChunkMeshData opaque_vertices;
	ChunkMeshData tranparent_vertices;
};

void mesh_chunk (Assets const& assets, WorldGenerator const& wg, Chunks& chunks, Chunk const* chunk, RemeshingMesh* mesh);
