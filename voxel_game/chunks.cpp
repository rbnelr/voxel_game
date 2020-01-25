#include "chunks.hpp"

Chunk* _prev_query_chunk = nullptr; // avoid hash map lookup most of the time, since a lot of query_chunk's are going to end up in the same chunk (in query_block of clustered blocks)

std::unordered_map<s64v2_hashmap, Chunk*> chunks;
