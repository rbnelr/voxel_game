#include "worldgen_dll.hpp"
#include "noise.hpp"

namespace worldgen {
	struct ChunkGenerator {
		block_id* blocks;
		int3 chunk_pos;
		int chunk_lod;
		uint64_t chunk_seed;

		//Random rand;

		inline block_id gen_block (float3 pos) {
			float noise_val = noise::perlin(pos.x / 200) * 20;

			return pos.z + 0.5f <= noise_val ? B_GRASS : B_AIR;
		}

		void gen_terrain () {
			float lod_scale = (float)(1u << chunk_lod);

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
	__declspec(dllexport) void generate_chunk_dll (block_id* blocks, int3 chunk_pos, int chunk_lod, uint64_t chunk_seed) {
		using namespace worldgen;

		ChunkGenerator gen{blocks, chunk_pos, chunk_lod, chunk_seed};
		gen.gen_terrain();
	}
}
