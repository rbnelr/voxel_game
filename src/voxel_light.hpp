#pragma once
#include "common.hpp"
#include "chunks.hpp"

extern std::vector<int3> dbg_block_light_add_list;
extern std::vector<int3> dbg_block_light_remove_list;

unsigned calc_block_light_level (Chunk* chunk, int3 pos_in_chunk, Block new_block);
void update_block_light (Chunks& chunks, int3 pos, unsigned old_light_level, unsigned new_light_level);

void update_sky_light_column (Chunk* chunk, int3 pos_in_chunk);
void update_sky_light_chunk (Chunk* chunk);
