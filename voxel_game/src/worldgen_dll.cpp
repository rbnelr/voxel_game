#include "worldgen_dll.hpp"

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

struct ChunkGenerator {
	block_id* blocks;
	int3 chunk_pos;
	int chunk_lod;
	uint64_t world_seed;

	//Random rand;

	float lod_scale;

	ChunkGenerator (block_id* blocks, int3 chunk_pos, int chunk_lod, uint64_t world_seed):
			blocks{blocks}, chunk_pos{chunk_pos}, chunk_lod{chunk_lod}, world_seed{world_seed} {

	}

	inline block_id gen_block (float3 pos) {
		return B_NULL;
	}

	void gen_terrain () {
		lod_scale = (float)(1u << chunk_lod);

		//auto chunk_seed = seed ^ hash(chunk->pos);

		for (int z=0; z<CHUNK_SIZE; ++z) {
			for (int y=0; y<CHUNK_SIZE; ++y) {
				for (int x=0; x<CHUNK_SIZE; ++x) {
					float3 pos_world = ((float3)int3(x,y,z) + 0.5f) * lod_scale + (float3)chunk_pos;

					blocks[CHUNK_3D_INDEX(x,y,z)] = gen_block(pos_world);
				}
			}
		}

	}
};

extern "C" {
	__declspec(dllexport) void generate_chunk_dll (block_id* blocks, int3 chunk_pos, int chunk_lod, uint64_t world_seed) {
		ChunkGenerator gen{blocks, chunk_pos, chunk_lod, world_seed};
		gen.gen_terrain();
	}
}
