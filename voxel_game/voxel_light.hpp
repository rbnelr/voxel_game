#pragma once
#include "stdafx.hpp"
#include "chunks.hpp"

extern std::vector<bpos> dbg_block_light_add_list;
extern std::vector<bpos> dbg_block_light_remove_list;

unsigned calc_block_light_level (Chunk* chunk, bpos pos_in_chunk, Block new_block);
void update_block_light (Chunks& chunks, bpos pos, unsigned old_light_level, unsigned new_light_level);

void update_sky_light_column (Chunk* chunk, bpos pos_in_chunk);
void update_sky_light_chunk (Chunk* chunk);
