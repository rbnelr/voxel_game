#pragma once
#include "chunks.hpp"

extern std::vector<bpos> dbg_block_light_add_list;
extern std::vector<bpos> dbg_block_light_remove_list;

uint8 calc_block_light_level (Chunk* chunk, bpos pos_in_chunk, Block new_block);
void update_block_light (Chunks& chunks, bpos pos, uint8 old_light_level, uint8 new_light_level);
