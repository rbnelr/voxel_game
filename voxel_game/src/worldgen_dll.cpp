#include "worldgen_dll.hpp"
#include "../FastNoise/FastNoise.h"

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

namespace worldgen {
	struct ChunkGenerator {
		block_id* blocks;
		int3 chunk_pos;
		int chunk_lod;
		uint64_t world_seed;

		//Random rand;

		float lod_scale;
		
		FastNoise noise[4];

		ChunkGenerator (block_id* blocks, int3 chunk_pos, int chunk_lod, uint64_t world_seed):
				blocks{blocks}, chunk_pos{chunk_pos}, chunk_lod{chunk_lod}, world_seed{world_seed},
				noise{ FastNoise((int)world_seed), FastNoise((int)world_seed+1), FastNoise((int)world_seed+2), FastNoise((int)world_seed+3) } {

		}

		inline block_id gen_block (float3 pos) {
			noise[1].SetFrequency(1.0f / 50);
			noise[1].SetFractalOctaves(2);
			noise[2].SetFrequency(1.0f / 50);
			noise[2].SetFractalOctaves(2);
			float warpx = noise[1].GetSimplexFractal(pos.x, pos.y, pos.z);
			float warpy = noise[2].GetSimplexFractal(pos.x, pos.y, pos.z);

			noise[0].SetFrequency(1.0f / 2000);
			noise[0].SetFractalOctaves(3);
			float SDF = (noise[0].GetSimplexFractal(pos.x, pos.y, pos.z) - 0.4f) * 1000;

			noise[3].SetFrequency(1.0f / 500);
			noise[3].SetFractalOctaves(2);
			SDF += max(noise[3].GetSimplexFractal(pos.x, pos.y, (pos.z + warpy*20) * 10) * 30, 0.0);

			block_id biome_blocks[] = {
				B_ICE1, B_DUST1, B_GREEN1, B_SHRUBS1, B_SAND, B_HOT_ROCK,
			};

			//int biome = roundi(cell * (float)ARRLEN(biome_blocks));
			
			//float SDF = pos.z - height;
			if (SDF > 0)
				return B_AIR;

			if (SDF > -lod_scale)
				return B_GRASS;
			if (SDF > -5)
				return B_EARTH;
			//if (SDF > -5)
			//	return biome_blocks[clamp(biome, 0, ARRLEN(biome_blocks)-1)];
			if (SDF > -14)
				return B_PEBBLES;
			return B_STONE;
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

}

extern "C" {
	__declspec(dllexport) void generate_chunk_dll (block_id* blocks, int3 chunk_pos, int chunk_lod, uint64_t world_seed) {
		using namespace worldgen;

		ChunkGenerator gen{blocks, chunk_pos, chunk_lod, world_seed};
		gen.gen_terrain();
	}
}
