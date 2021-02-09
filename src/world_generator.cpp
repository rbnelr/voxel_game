#include "common.hpp"
#include "world_generator.hpp"
#include "blocks.hpp"
#include "chunks.hpp"

#include "open_simplex_noise/open_simplex_noise.hpp"

float heightmap (WorldGenerator const& wg, OSN::Noise<2> const& osn_noise, float2 pos_world, float* earth_layer) {
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

float cave_noise (OSN::Noise<3> const& osn_noise, OSN::Noise<2> noise2, float3 pos_world) {
	auto noise = [&] (float3 pos, float period, float3 offs) {
		pos /= period; // period is inverse frequency
		pos += offs;

		return osn_noise.eval<float>(pos.x, pos.y, pos.z / 0.7f);
	};

	//float val = noise(pos_world, 180.0f, 0) * 180.0f - 50.0f;
	//float dx = (noise(pos_world + float3(1,0,0), 180.0f, 0) * 180.0f - 50.0f) - val;
	//float dy = (noise(pos_world + float3(0,1,0), 180.0f, 0) * 180.0f - 50.0f) - val;
	//float dz = (noise(pos_world + float3(0,0,1), 180.0f, 0) * 180.0f - 50.0f) - val;
	//
	//float3 normal = normalize(float3(dx,dy,dz));
	//
	//float stalag_freq = 5.0f;
	//float stalag = noise2.eval(pos_world.x / stalag_freq, pos_world.y / stalag_freq) * (abs(normal.z) - 0.2f);
	//if (dz < 0) {
	//	stalag = max(stalag, 0.0f);
	//	stalag *= -40;
	//	val += stalag;
	//}

	float val = 0.0f;

	// large scale
	val += noise(pos_world, 300.0f, 0) * 300.0f;

	// small scale
	val += noise(pos_world, 180.0f, 0) * 180.0f - 50.0f;
	val += noise(pos_world, 70.0f, float3(700,800,900)) * 70.0f;
	val += noise(pos_world, 4.0f, float3(700,800,900)) * 2.0f;

	val += pos_world.z * 0.5f;

	return val;
}

float noise_tree_density (WorldGenerator const& wg, OSN::Noise<2> const& osn_noise, float2 pos_world) {
	auto noise = [&] (float2 pos, float period, float ang_offs, float2 offs) {
		pos = rotate2(ang_offs) * pos;
		pos /= period; // period is inverse frequency
		pos += offs;

		float val = osn_noise.eval<float>(pos.x, pos.y);
		val = map(val, -0.865773f, 0.865772f); // normalize into [0,1] range
		return val;
	};

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

float noise_grass_density (WorldGenerator const& wg, OSN::Noise<2> const& osn_noise, float2 pos_world) {
		auto noise = [&] (float2 pos, float period, float ang_offs, float2 offs) {
			pos = rotate2(ang_offs) * pos;
			pos /= period; // period is inverse frequency
			pos += offs;

			float val = osn_noise.eval<float>(pos.x, pos.y);
			val = map(val, -0.865773f, 0.865772f); // normalize into [0,1] range
			return val;
		};

		float valb = noise(pos_world, 4, 0,0) * wg.grass_density_amp / 2;
		float val = noise(pos_world, wg.grass_desity_period, 0,0) * wg.grass_density_amp;

		return smoothstep( smoothstep(val + valb) );
	}

#define POS2IDX(x,y,z) IDX3D(x,y,z, CHUNK_SIZE)

void WorldgenJob::execute () {
	ZoneScoped;

	const auto AIR			= g_assets.block_types.map_id("air");
	const auto WATER		= g_assets.block_types.map_id("water");
	const auto STONE		= g_assets.block_types.map_id("stone");
	const auto EARTH		= g_assets.block_types.map_id("earth");
	const auto GRASS		= g_assets.block_types.map_id("grass");
	const auto TREE_LOG		= g_assets.block_types.map_id("tree_log");
	const auto LEAVES		= g_assets.block_types.map_id("leaves");
	const auto TALLGRASS	= g_assets.block_types.map_id("tallgrass");
	const auto TORCH		= g_assets.block_types.map_id("torch");
	
	int3 chunk_origin = chunk->pos * CHUNK_SIZE;

	int water_level = 21 - chunk_origin.z;

	uint64_t chunk_seed = wg->seed ^ hash(chunk->pos);

	OSN::Noise<2> noise(wg->seed);
	Random rand = Random(chunk_seed);

	OSN::Noise<3> noise3 (wg->seed);

	block_id* blocks = voxel_buffer;

	{ // 3d noise generate
		ZoneScopedN("3d noise generate");

		block_id* cur = blocks;
		for (int z=0; z<CHUNK_SIZE; ++z) {
			for (int y=0; y<CHUNK_SIZE; ++y) {
				for (int x=0; x<CHUNK_SIZE; ++x) {
					int3 pos_world = int3(x,y,z) + chunk_origin;

					float val = cave_noise(noise3, noise, (float3)pos_world);

					block_id bid;
					if (val < -5.0f) {
						bid = STONE;
					} else if (val <= 0.0f) {
						bid = EARTH;
					} else {
						if (z >= water_level) {
							bid = AIR;
						} else {
							bid = WATER;
						}
					}

					*cur++ = bid;
				}
			}
		}
	}

	std_vector<int3> tree_poss;

	{
		ZoneScopedN("Tree pass");

		auto find_min_tree_dist = [&] (int2 new_tree_pos) {
			float min_dist = +INF;
			for (int3 p : tree_poss)
				min_dist = min(min_dist, length((float2)(int2)p -(float2)new_tree_pos));
			return min_dist;
		};

		for (int y=0; y<CHUNK_SIZE; ++y) {
			for (int x=0; x<CHUNK_SIZE; ++x) {

				int3 pos_world2 = int3(x,y,0) + chunk_origin;

				float tree_density = noise_tree_density(*wg, noise, (float2)(int2)pos_world2);

				float grass_density = noise_grass_density(*wg, noise, (float2)(int2)pos_world2);

				float tree_prox_prob = gradient<float>( find_min_tree_dist(int2(x,y)), {
					{ SQRT_2,	0 },		// length(float2(1,1)) -> zero blocks free diagonally
					{ 2.236f,	0.02f },	// length(float2(1,2)) -> one block free
					{ 2.828f,	0.15f },	// length(float2(2,2)) -> one block free diagonally
					{ 4,		0.75f },
					{ 6,		1 },
				});
				float effective_tree_prob = tree_density * tree_prox_prob;
				//float effective_tree_prob = tree_density;

				for (int z=1; z<CHUNK_SIZE; ++z) {
					auto* below = &blocks[POS2IDX(x,y,z-1)];
					auto* bid   = &blocks[POS2IDX(x,y,z)];

					bool block_free = *bid == AIR && z > 0 && *below == EARTH;

					if (block_free) {
						*below = GRASS;

						float tree_chance = rand.uniformf();
						float grass_chance = rand.uniformf();

						if (rand.uniformf() < effective_tree_prob) {
							tree_poss.push_back( int3(x,y,z) );
						} else if (rand.uniformf() < grass_density) {
							*bid = TALLGRASS;
						} else if (rand.uniformf() < 0.0005f) {
							*bid = TORCH;
							//*block_light = g_blocks.blocks[TORCH].glow;
						}
					}
				}
			}
		}
	}

	{
		ZoneScopedN("Place structures");

		auto place_tree = [&] (int3 pos) {
			auto* bid = &blocks[POS2IDX(pos.x, pos.y, pos.z) + POS2IDX(0,0,-1)];

			if (pos.z > 0 && *bid == GRASS) {
				*bid = EARTH;
			}

			auto place_block = [&] (int3 pos, block_id bt) {
				if (any(pos < 0 || pos >= CHUNK_SIZE)) return;
				auto* bid = &blocks[POS2IDX(pos.x, pos.y, pos.z)];

				if (*bid == AIR || *bid == WATER || (bt == TREE_LOG && *bid == LEAVES)) {
					*bid = bt;
				}
			};
			auto place_block_sphere = [&] (int3 pos_chunk, float3 r, block_id bt) {
				int3 start = (int3)floor((float3)pos_chunk +0.5f -r);
				int3 end = (int3)ceil((float3)pos_chunk +0.5f +r);

				int3 i; // position in chunk
				for (i.z=start.z; i.z<end.z; ++i.z) {
					for (i.y=start.y; i.y<end.y; ++i.y) {
						for (i.x=start.x; i.x<end.x; ++i.x) {
							if (length_sqr((float3)(i -pos_chunk) / r) <= 1) place_block(i, bt);
						}
					}
				}
			};

			int tree_height = 6;

			for (int i=0; i<tree_height; ++i)
				place_block(pos +int3(0,0,i), TREE_LOG);

			place_block_sphere(pos +int3(0,0,tree_height-1), float3(float2(3.2f),tree_height/2.5f), LEAVES);
		};

		for (int3 p : tree_poss)
			place_tree(p);
	}
}

WorldgenJob::WorldgenJob () {
	ZoneScopedNC("malloc WorldgenJob::voxel_buffer", tracy::Color::Crimson);
	voxel_buffer = (block_id*)malloc(sizeof(block_id) * CHUNK_VOXEL_COUNT);
}
WorldgenJob::~WorldgenJob () {
	ZoneScopedNC("free WorldgenJob::voxel_buffer", tracy::Color::Crimson);
	free(voxel_buffer);
}
