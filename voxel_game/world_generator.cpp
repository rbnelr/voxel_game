#include "world_generator.hpp"
#include "blocks.hpp"
#include "chunks.hpp"
#include "util/random.hpp"
#include "util/timer.hpp"

#include "open_simplex_noise/open_simplex_noise.hpp"

struct ChunkGenerator {
	WorldGenerator const& wg;
	Chunk& chunk;

	OSN::Noise<2> osn_noise2;
	Random rand;

	static constexpr int3 BORDER = int3(1, 0, 0);
	static constexpr int3 DIM = int3(CHUNK_DIM + BORDER.x*2, CHUNK_DIM + BORDER.y*2, CHUNK_DIM + BORDER.z*2);
	
	struct Blocks {
		block_id blocks[DIM.z][DIM.y][DIM.x];
	};

	Blocks* blocks;

	float noise (float2 pos, float period, float ang_offs, float2 offs, float range_l=0, float range_h=1) {
		pos = rotate2(ang_offs) * pos;
		pos /= period; // period is inverse frequency
		pos += offs;

		float val = osn_noise2.eval<float>(pos.x, pos.y);
		val = map(val, -0.865773f, 0.865772f, range_l,range_h); // normalize into [range_l,range_h] range
		return val;
	}

	float heightmap (float2 pos_world, float* earth_layer) {
		float elevation;
		float roughness;
		float detail;

		int i = 0;
		{
			float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
			offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
			elevation = noise(pos_world, wg.elev_freq, deg(37.17f) * (float)i, offs, -1,+1) * wg.elev_amp;

			++i;
		}
		{
			float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
			offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
			roughness = noise(pos_world, wg.rough_freq, deg(37.17f) * (float)i, offs, 0,+1);

			++i;
		}

		detail = 0;
		for (auto& d : wg.detail) {
			float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
			offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
			detail += noise(pos_world, d.freq, deg(37.17f) * (float)i, offs, -1,+1) * d.amp;

			++i;
		}

		{
			float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
			offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
			*earth_layer = noise(pos_world, 48, deg(37.17f) * (float)i, offs, 3,5.5f);

			++i;
		}

		return (elevation +32) +(roughness * detail);
	}

	float noise_tree_density (float2 pos_world) {
		float val = noise(pos_world, wg.tree_desity_period, 0,0) * wg.tree_density_amp;

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

	float noise_grass_density (float2 pos_world) {
		float valb = noise(pos_world, 4, 0,0) * wg.grass_density_amp / 2;
		float val = noise(pos_world, wg.grass_desity_period, 0,0) * wg.grass_density_amp;

		return smoothstep( smoothstep(val + valb) );
	}

	void gen_terrain (bpos chunk_origin) {
		bpos_t water_level = 21;

		float highest_blocks[DIM.y][DIM.x];
		float earth_layers[DIM.y][DIM.x];

		{
			bpos2 i;
			for (i.y=0; i.y<DIM.y; ++i.y) {
				for (i.x=0; i.x<DIM.x; ++i.x) {
					bpos2 pos_world = i - bpos2(BORDER) + bpos2(chunk_origin);

					float earth_layer;
					float height = heightmap((float2)pos_world, &earth_layer);

					highest_blocks[i.y][i.x] = height;
					earth_layers[i.y][i.x] = earth_layer;
				}
			}
		}

		{
			bpos i;
			for (i.z=0; i.z<DIM.z; ++i.z) {
				for (i.y=0; i.y<DIM.y; ++i.y) {
					for (i.x=0; i.x<DIM.x; ++i.x) {
						bpos pos_world = i - BORDER + chunk_origin;

						block_id bid;

						float highest_block = highest_blocks[i.y][i.x];
						float earth_layer = earth_layers[i.y][i.x];

						bool over_water = water_level > highest_block;
						
						float sand_thickness = noise((float2)(int2)pos_world, 7.2f, deg(237.17f), 0, 0.8f, 1.7f);
						bool beach = abs(highest_block - water_level) <= sand_thickness/2;

						if (pos_world.z <= highest_block) {
							if (pos_world.z <= highest_block - earth_layer) {
								bid = B_STONE;
							} else {
								if (beach) {
									bid = B_SAND;
								} else {
									if (pos_world.z == int(highest_block) && pos_world.z >= water_level) {
										bid = B_GRASS;
									} else {
										bid = over_water ? B_PEBBLES : B_EARTH;
									}
								}
							}
						} else {
							bid = pos_world.z <= water_level ? B_WATER : B_AIR;
						}

						blocks->blocks[i.z][i.y][i.x] = bid;
					}
				}
			}
		}
	}

	void place_objects (bpos chunk_origin) {
		std::vector<bpos> tree_poss;
	
		auto find_min_tree_dist = [&] (bpos2 new_tree_pos) {
			float min_dist = +INF;
			for (bpos p : tree_poss)
				min_dist = min(min_dist, length((float2)(bpos2)p -(float2)new_tree_pos));
			return min_dist;
		};
	
		bpos i;
		for (i.z=0; i.z<DIM.z; ++i.z) {
			for (i.y=0; i.y<DIM.y; ++i.y) {
				for (i.x=0; i.x<DIM.x; ++i.x) {
					bpos pos_world = i - BORDER + chunk_origin;
					
					block_id* bid   =           &blocks->blocks[i.z    ][i.y][i.x];
					block_id* below = i.z > 0 ? &blocks->blocks[i.z - 1][i.y][i.x] : nullptr;
	
					if (*bid == B_AIR && below && (*below == B_GRASS || *below == B_EARTH)) {
						float tree_density = noise_tree_density((float2)(int2)pos_world);
	
						float grass_density = noise_grass_density((float2)(int2)pos_world);
	
						float tree_prox_prob = gradient<float>( find_min_tree_dist((bpos2)i), {
							{ SQRT_2,	0 },		// length(float2(1,1)) -> zero blocks free diagonally
							{ 2.236f,	0.02f },	// length(float2(1,2)) -> one block free
							{ 2.828f,	0.15f },	// length(float2(2,2)) -> one block free diagonally
							{ 4,		0.75f },
							{ 6,		1 },
							});
						float effective_tree_prob = tree_density * tree_prox_prob;
	
						// TODO: can do these if else if chance things with just a single rand call by taking the remaining prob
						// ex. first case has 60% prob -> if (rand < .6)    rand = [0,1]
						//     second case wants 30% prob -> possible range now [.6,1] -> if (rand < 0.72)    0.72 = (1 - 0.6) * 0.3 + 0.6     ==   a + b - a * b
						
						if (rand.uniform() < effective_tree_prob) {
							*below = B_EARTH;
	
							{
								tree_poss.push_back(i);
							}
						} else if (rand.uniform() < grass_density) {
							*bid = B_TALLGRASS;
						} else if (rand.uniform() < 0.0005f) {
							*bid = B_TORCH;
						}
					}
				}
			}
		}
	
		auto place_tree = [&] (bpos pos) {
			
			auto place_block = [&] (bpos pos, block_id bt) {
				if (any(pos < 0 || pos >= DIM)) return;

				block_id* bid = &blocks->blocks[pos.z][pos.y][pos.x];
	
				if (bid && (*bid == B_AIR || *bid == B_LEAVES)) {
					*bid = bt;
				}
			};
			auto place_block_sphere = [&] (bpos pos, float3 r, block_id bt) {
				bpos start = (bpos)floor((float3)pos +0.5f -r);
				bpos end = (bpos)ceil((float3)pos +0.5f +r);
	
				bpos i; // position in chunk
				for (i.z=start.z; i.z<end.z; ++i.z) {
					for (i.y=start.y; i.y<end.y; ++i.y) {
						for (i.x=start.x; i.x<end.x; ++i.x) {
							if (length_sqr((float3)(i - pos) / r) <= 1) place_block(i, bt);
						}
					}
				}
			};
	
			bpos_t tree_height = 6;
	
			for (bpos_t i=0; i<tree_height; ++i)
				place_block(pos +bpos(0,0,i), B_TREE_LOG);
	
			place_block_sphere(pos +bpos(0,0,tree_height-1), float3(float2(3.2f),tree_height/2.5f), B_LEAVES);
		};
	
		for (bpos p : tree_poss)
			place_tree(p);
	}

	void gen () {
		osn_noise2 = OSN::Noise<2>(wg.seed);

		bpos chunk_origin = chunk.coord * CHUNK_DIM;
		
		auto chunk_seed = wg.seed ^ hash(chunk.coord);
		rand = Random(chunk_seed);

		{
			blocks = (Blocks*)malloc(sizeof(Blocks));
		}

		gen_terrain(chunk_origin);

		place_objects(chunk_origin);

		{
			bpos i;
			for (i.z=0; i.z<CHUNK_DIM; ++i.z) {
				for (i.y=0; i.y<CHUNK_DIM; ++i.y) {
					for (i.x=0; i.x<CHUNK_DIM; ++i.x) {
						chunk.blocks->id[ChunkData::pos_to_index(i)] = blocks->blocks[i.z + BORDER.z][i.y + BORDER.y][i.x + BORDER.x];
					}
				}
			}
		}

		{
			free(blocks);
		}
	}
};

void WorldGenerator::generate_chunk (Chunk& chunk) const {
	ChunkGenerator gen = { *this, chunk };

	chunk.init_blocks();
	gen.gen();
}
