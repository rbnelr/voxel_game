#include "world_generator.hpp"
#include "blocks.hpp"
#include "chunks.hpp"
#include "util/random.hpp"

#include "open_simplex_noise/open_simplex_noise.hpp"

struct Generator {
	WorldGenerator const& worldgen;
	Chunk& chunk;
	uintptr_t world_seed;

	float heightmap (OSN::Noise<2> const& osn_noise, float2 pos_world) {
		auto noise = [&] (float2 pos, float period, float ang_offs, float2 offs, float range_l, float range_h) {
			pos = rotate2(ang_offs) * pos;
			pos /= period; // period is inverse frequency
			pos += offs;

			float val = osn_noise.eval<float>(pos.x, pos.y);
			val = map(val, -0.865773f, 0.865772f, range_l,range_h); // normalize into [range_l,range_h] range
			return val;
		};

		float elevation;
		float roughness;
		float detail;

		int i = 0;
		{
			float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
			offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
			elevation = noise(pos_world, worldgen.elev_freq, deg(37.17f) * (float)i, offs, -1,+1) * worldgen.elev_amp;

			++i;
		}
		{
			float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
			offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
			roughness = noise(pos_world, worldgen.rough_freq, deg(37.17f) * (float)i, offs, 0,+1);

			++i;
		}

		detail = 0;
		{
			float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
			offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
			detail += noise(pos_world, worldgen.detail0_freq, deg(37.17f) * (float)i, offs, -1,+1) * worldgen.detail0_amp;

			++i;
		}
		{
			float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
			offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
			detail += noise(pos_world, worldgen.detail1_freq, deg(37.17f) * (float)i, offs, -1,+1) * worldgen.detail1_amp;

			++i;
		}
		{
			float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
			offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
			detail += noise(pos_world, worldgen.detail2_freq, deg(37.17f) * (float)i, offs, -1,+1) * worldgen.detail2_amp;

			++i;
		}

		return (elevation +32) +(roughness * detail);
	}

	float noise_tree_density (OSN::Noise<2> const& osn_noise, float2 pos_world) {
		auto noise = [&] (float2 pos, float period, float ang_offs, float2 offs) {
			pos = rotate2(ang_offs) * pos;
			pos /= period; // period is inverse frequency
			pos += offs;

			float val = osn_noise.eval<float>(pos.x, pos.y);
			val = map(val, -0.865773f, 0.865772f); // normalize into [0,1] range
			return val;
		};

		float val = noise(pos_world, worldgen.noise_tree_desity_period, 0,0) * worldgen.noise_tree_density_amp;

		val = gradient<float>(val, {
			{ 0.00f,  0						},
			{ 0.05f,  1.0f / (5*5 * 32*32)	}, // avg one tree in 5x5 chunks
			{ 0.25f,  1.0f / (32*32)		}, // avg one tree in 1 chunk
			{ 0.50f,  4.0f / (32*32)		}, // avg 5 tree in 1 chunk
			{ 0.75f, 10.0f / (32*32)		}, // avg 15 tree in 1 chunk
			{ 1.00f, 25.0f / (32*32)		}, // avg 40 tree in 1 chunk
		});

	#if 0
		// TODO: use height of block to alter tree density
		val = gradient<float>(val, {
			{ 0.00f,  0						},
			{ 0.05f,  1.0f / (5*5 * 32*32)	}, // avg one tree in 5x5 chunks
			{ 0.25f,  1.0f / (32*32)		}, // avg one tree in 1 chunk
			{ 0.50f,  5.0f / (32*32)		}, // avg 5 tree in 1 chunk
			{ 0.75f, 15.0f / (32*32)		}, // avg 15 tree in 1 chunk
			{ 1.00f, 40.0f / (32*32)		}, // avg 40 tree in 1 chunk
		});
	#endif

		return val;
	}

	void gen () {
		bpos_t water_level = 21;

		bpos i; // position in chunk
		for (i.z=0; i.z<CHUNK_DIM_Z; ++i.z) {
			for (i.y=0; i.y<CHUNK_DIM_Y; ++i.y) {
				for (i.x=0; i.x<CHUNK_DIM_X; ++i.x) {
					auto* b = chunk.get_block(i);

					if (i.z <= water_level) {
						b->type = BT_WATER;
						b->hp_ratio = 1;
						b->dbg_tint = 255;
					} else {
						b->type = BT_AIR;
						b->hp_ratio = 1;
						b->dbg_tint = 0;
					}
				}
			}
		}

		uint64_t chunk_seed = world_seed ^ hash(chunk.coord);

		OSN::Noise<2> noise(world_seed);
		Random rand = Random(chunk_seed);

		std::vector<bpos> tree_poss;

		auto find_min_tree_dist = [&] (bpos2 new_tree_pos) {
			float min_dist = +INF;
			for (bpos p : tree_poss)
				min_dist = min(min_dist, length((float2)(bpos2)p -(float2)new_tree_pos));
			return min_dist;
		};

		for (i.y=0; i.y<CHUNK_DIM_Y; ++i.y) {
			for (i.x=0; i.x<CHUNK_DIM_X; ++i.x) {

				float2 pos_world = (float2)((bpos2)i +chunk.coord * CHUNK_DIM_2D);

				float height = heightmap(noise, pos_world);
				int highest_block = (int)floor(height -1 +0.5f); // -1 because height 1 means the highest block is z=0

				float tree_density = noise_tree_density(noise, pos_world);

				float tree_prox_prob = gradient<float>( find_min_tree_dist((bpos2)i), {
					{ SQRT_2,	0 },		// length(float2(1,1)) -> zero blocks free diagonally
					{ 2.236f,	0.02f },	// length(float2(1,2)) -> one block free
					{ 2.828f,	0.15f },	// length(float2(2,2)) -> one block free diagonally
					{ 4,		0.75f },
					{ 6,		1 },
					});
				float effective_tree_prob = tree_density * tree_prox_prob;
				//float effective_tree_prob = tree_density;

				float tree_chance = rand.uniform();
				if (tree_chance < effective_tree_prob)
					tree_poss.push_back( bpos((bpos2)i, highest_block +1) );

				for (i.z=0; i.z <= min(highest_block, (int)CHUNK_DIM_Z-1); ++i.z) {
					auto* b = chunk.get_block(i);

					if (i.z == highest_block && i.z >= water_level
						//&& equal(chunk->coord, 0)
						//&& (chunk->coord.x % 2 == 0 && chunk->coord.y % 2 == 0)
						) {
						b->type = BT_GRASS;
					} else {
						b->type = BT_EARTH;
					}

					b->hp_ratio = 1;

					//b->dbg_tint = lrgba8(spectrum_gradient(map(height, 0, 45)), 255);
					//b->dbg_tint = spectrum_gradient(tree_density / (25.0f/(32*32)));
				}
			}
		}

		auto place_tree = [&] (bpos pos_chunk) {
			auto* ground_block = chunk.get_block(pos_chunk - bpos(0,0,1));
			if (ground_block && ground_block->type == BT_GRASS)
				ground_block->type = BT_EARTH;

			auto place_block = [&] (bpos pos_chunk, block_type bt) {
				if (any(pos_chunk < 0 || pos_chunk >= CHUNK_DIM)) return;
				Block* b = chunk.get_block(pos_chunk);
				if (b->type == BT_AIR || b->type == BT_WATER || (bt == BT_TREE_LOG && b->type == BT_LEAVES)) {
					b->type = bt;
					b->hp_ratio = 1;
					b->dbg_tint = 255;
				}
			};
			auto place_block_sphere = [&] (bpos pos_chunk, float3 r, block_type bt) {
				bpos start = (bpos)floor((float3)pos_chunk +0.5f -r);
				bpos end = (bpos)ceil((float3)pos_chunk +0.5f +r);

				bpos i; // position in chunk
				for (i.z=start.z; i.z<end.z; ++i.z) {
					for (i.y=start.y; i.y<end.y; ++i.y) {
						for (i.x=start.x; i.x<end.x; ++i.x) {
							if (length_sqr((float3)(i -pos_chunk) / r) <= 1) place_block(i, bt);
						}
					}
				}
			};

			bpos_t tree_height = 6;

			for (bpos_t i=0; i<tree_height; ++i)
				place_block(pos_chunk +bpos(0,0,i), BT_TREE_LOG);

			place_block_sphere(pos_chunk +bpos(0,0,tree_height-1), float3(float2(3.2f),tree_height/2.5f), BT_LEAVES);
		};

		for (bpos p : tree_poss)
			place_tree(p);

	}
};

void WorldGenerator::generate_chunk (Chunk& chunk, uintptr_t world_seed) const {
	Generator gen = { *this, chunk, world_seed };
	gen.gen();
}