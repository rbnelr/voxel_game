#pragma once
#include "chunks.hpp"

// TODO: can you always determine which of these to call based on the block?, if so do that instead of having two functions
// with multiple block changes in one frame does it make sense to use build a queue from those first and then do the algorithm once?

// A block at bp was changed that now produces light (torch placed)
void update_block_light_add (Chunks& chunks, bpos bp);
// A block at bp was chnaged that now does not produce light anymore (torch broken)
// TODO: might work for block placing inside lit air block too, since this effectivly removes a lit propagation block and replaces it with a 0 light level block
void update_block_light_remove (Chunks& chunks, bpos bp);
