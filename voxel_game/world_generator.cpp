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

	struct ChunkToGenerate {
		Random rand;

		uint64_t chunk_seed;
		bpos chunk_origin;

		block_id* blocks;

		//
		static constexpr uint64_t COUNT = (CHUNK_DIM+2) * (CHUNK_DIM+2) * (CHUNK_DIM+2);

		static constexpr uint64_t pos_to_index (bpos pos) {
			return (pos.z + 1) * (CHUNK_DIM+2) * (CHUNK_DIM+2) + (pos.y + 1) * (CHUNK_DIM+2) + (pos.x + 1);
		}

		block_id& get (bpos pos) {
			if (blocks == nullptr) return *(block_id*)nullptr;
			return blocks[ pos_to_index(pos) ];
		}
	};

	ChunkToGenerate neighbours[3][3][3];

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

	void gen_terrain (ChunkToGenerate& chunk) {
		bpos_t water_level = 21 - chunk.chunk_origin.z;

		bpos i; // position in chunk
		for (i.z=0; i.z<CHUNK_DIM; ++i.z) {
			for (i.y=0; i.y<CHUNK_DIM; ++i.y) {
				for (i.x=0; i.x<CHUNK_DIM; ++i.x) {
					chunk.get(i) = i.z <= water_level ? B_WATER : B_AIR;
				}
			}
		}

		for (i.y=0; i.y<CHUNK_DIM; ++i.y) {
			for (i.x=0; i.x<CHUNK_DIM; ++i.x) {

				bpos pos_world = bpos(i,0) + chunk.chunk_origin;

				float earth_layer;
				float height = heightmap((float2)(int2)pos_world, &earth_layer);
				int highest_block = (int)floor(height -1 +0.5f) - pos_world.z; // -1 because height 1 means the highest block is z=0

				for (i.z=0; i.z <= min(highest_block, CHUNK_DIM-1); ++i.z) {
					auto& bid = chunk.get(i);

					if (i.z <= highest_block - earth_layer) {
						bid = B_STONE;
					} else {
						if (i.z == highest_block && i.z >= water_level) {
							bid = B_GRASS;
						} else {
							bid = B_EARTH;
						}
					}
				}

			}
		}
	}

	block_id* query_block (bpos pos_rel) {
		bpos pos_in_chunk;
		bpos neighbour = get_chunk_from_block_pos(pos_rel, &pos_in_chunk);

		if (any(neighbour < -1 || neighbour > +1))
			return nullptr;

		return &neighbours[neighbour.z+1][neighbour.y+1][neighbour.x+1].get(pos_in_chunk);
	}

	void place_objects (ChunkToGenerate& chunk, bpos neighbour_offs) {

		std::vector<bpos> tree_poss;

		auto find_min_tree_dist = [&] (bpos2 new_tree_pos) {
			float min_dist = +INF;
			for (bpos p : tree_poss)
				min_dist = min(min_dist, length((float2)(bpos2)p -(float2)new_tree_pos));
			return min_dist;
		};

		bpos i;
		for (i.z=0; i.z<CHUNK_DIM; ++i.z) {
			for (i.y=0; i.y<CHUNK_DIM; ++i.y) {
				for (i.x=0; i.x<CHUNK_DIM; ++i.x) {
					bpos pos_world = i + chunk.chunk_origin;
					bpos pos_rel = i + neighbour_offs * CHUNK_DIM;
					
					auto& bid = chunk.get(i);

					if (bid == B_AIR) {
						block_id* below = query_block(pos_rel - bpos(0,0,1));

						if (below && (*below == B_GRASS || *below == B_EARTH)) {
							OPTICK_EVENT("roll place object");

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
						
							if (chunk.rand.uniform() < effective_tree_prob) {
								*below = B_EARTH;

								{
									OPTICK_EVENT("tree_poss.push_back");
									tree_poss.push_back(i);
								}
							} else if (chunk.rand.uniform() < grass_density) {
								bid = B_TALLGRASS;
							} else if (chunk.rand.uniform() < 0.0005f) {
								bid = B_TORCH;
							}
						}
					}
				}
			}
		}

		auto place_tree = [&] (bpos pos_chunk) {
			bpos pos_rel = pos_chunk + neighbour_offs * CHUNK_DIM;

			auto place_block = [&] (bpos pos_rel, block_id bt) {
				block_id* bid = query_block(pos_rel);

				if (bid && (*bid == B_AIR || *bid == B_LEAVES)) {
					*bid = bt;
				}
			};
			auto place_block_sphere = [&] (bpos pos_rel, float3 r, block_id bt) {
				bpos start = (bpos)floor((float3)pos_rel +0.5f -r);
				bpos end = (bpos)ceil((float3)pos_rel +0.5f +r);

				bpos i; // position in chunk
				for (i.z=start.z; i.z<end.z; ++i.z) {
					for (i.y=start.y; i.y<end.y; ++i.y) {
						for (i.x=start.x; i.x<end.x; ++i.x) {
							if (length_sqr((float3)(i - pos_rel) / r) <= 1) place_block(i, bt);
						}
					}
				}
			};

			bpos_t tree_height = 6;

			for (bpos_t i=0; i<tree_height; ++i)
				place_block(pos_rel +bpos(0,0,i), B_TREE_LOG);

			place_block_sphere(pos_rel +bpos(0,0,tree_height-1), float3(float2(3.2f),tree_height/2.5f), B_LEAVES);
		};

		for (bpos p : tree_poss)
			place_tree(p);
	}

#define ITER3D for (int z=-1; z<=1; ++z) for (int y=-1; y<=1; ++y) for (int x=-1; x<=1; ++x)
//#define ITER3D int x=0, y=0, z=0;

	void gen () {
		osn_noise2 = OSN::Noise<2>(wg.seed);

		{
			OPTICK_EVENT("alloc neighbours");
			ITER3D {
				auto& c = neighbours[z+1][y+1][x+1];

				auto chunk_coord = chunk.coord + bpos(x,y,z);

				c.chunk_origin = chunk_coord * CHUNK_DIM;
				c.chunk_seed = wg.seed ^ hash(chunk_coord);
				c.rand = Random(c.chunk_seed);

				if (x==0 && y==0 && z==0) {
					c.blocks = chunk.blocks->id;
				} else {
					OPTICK_EVENT("malloc");
					c.blocks = (block_id*)malloc(sizeof(block_id) * ChunkToGenerate::COUNT);
				}
			}
		}

		{
			OPTICK_EVENT("gen_terrain");
			ITER3D {
				auto& c = neighbours[z+1][y+1][x+1];

				gen_terrain(c);
			}
		}

		{
			OPTICK_EVENT("place_objects");
			ITER3D {
				auto& c = neighbours[z+1][y+1][x+1];

				place_objects(c, bpos(x,y,z));
			}
		}

		{
			OPTICK_EVENT("free neighbours");
			ITER3D {
				auto& c = neighbours[z+1][y+1][x+1];

				if (!(x==0 && y==0 && z==0)) {
					OPTICK_EVENT("free");
					free(c.blocks);
					c.blocks = nullptr;
				}
			}
		}
	}
};

void WorldGenerator::generate_chunk (Chunk& chunk) const {
	OPTICK_EVENT();

	ChunkGenerator gen = { *this, chunk };

	chunk.init_blocks();
	gen.gen();
}
