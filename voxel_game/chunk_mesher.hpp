#pragma once
#include "stdafx.hpp"
#include "chunks.hpp"

struct WorldGenerator;

void mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, TileTextures const& tile_textures, WorldGenerator const& wg, Chunk* chunk, MeshingResult* res);
