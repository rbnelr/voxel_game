#pragma once
#include "common.hpp"
#include "chunks.hpp"

struct WorldGenerator;
struct Graphics;

struct RemeshChunkJob : ThreadingJob { // Chunk remesh
	// input
	Chunk* chunk;
	Chunks* chunks; // not modfied
	Graphics const* graphics;
	WorldGenerator const* wg;
	// output
	MeshingResult meshing_result;

	RemeshChunkJob (Chunk* chunk, Chunks* chunks, Graphics const* graphics, WorldGenerator const* wg):
		chunk{chunk}, chunks{chunks}, graphics{graphics}, wg{wg} {}
	virtual ~RemeshChunkJob() = default;

	virtual void execute ();
	virtual void finalize ();
};
