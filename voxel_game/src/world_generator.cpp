#include "stdafx.hpp"
#include "world_generator.hpp"
#include "blocks.hpp"
#include "voxel_system.hpp"
#include "svo.hpp"

#include "open_simplex_noise/open_simplex_noise.hpp"
#include "noise.hpp"

namespace worldgen {
	template<typename T>
	struct Gradient_KV {
		float	key;
		T		val;
	};

	template<typename T>
	inline T gradient (float key, Gradient_KV<T> const* kvs, size_t kvs_count) {
		if (kvs_count == 0) return T(0);

		size_t i=0;
		for (; i<kvs_count; ++i) {
			if (key < kvs[i].key) break;
		}

		if (i == 0) { // val is lower than the entire range
			return kvs[0].val;
		} else if (i == kvs_count) { // val is higher than the entire range
			return kvs[i -1].val;
		} else {
			assert(kvs_count >= 2 && i < kvs_count);

			auto& a = kvs[i -1];
			auto& b = kvs[i];
			return map(key, a.key, b.key, a.val, b.val);
		}
	}
	template<typename T>
	inline T gradient (float key, std::initializer_list<Gradient_KV<T>> const& kvs) {
		return gradient<T>(key, &*kvs.begin(), kvs.size());
	}

	struct ChunkGenerator {
		Chunk*					chunk;
		SVO&					svo;
		const WorldGenerator&	wg;
		int						lod;

		OSN::Noise<2> osn_noise2;
		Random rand;

		block_id blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];

		float heightmap[CHUNK_SIZE][CHUNK_SIZE];
		float earth_layers[CHUNK_SIZE][CHUNK_SIZE];
		float sand_thicknesss[CHUNK_SIZE][CHUNK_SIZE];

		float noise (float2 pos, float period, float ang_offs, float2 offs, float range_l=0, float range_h=1) {
			pos = rotate2(ang_offs) * pos;
			pos /= period; // period is inverse frequency
			pos += offs;

			float val = osn_noise2.eval<float>(pos.x, pos.y);
			val = map(val, -0.865773f, 0.865772f, range_l,range_h); // normalize into [range_l,range_h] range
			return val;
		}

		float clac_heightmap (float2 pos_world, float* earth_layer) {
			float elevation;
			float roughness;
			float detail;

			float elev_mult;

			int i = 0;
			{
				float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
				offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
				elev_mult = pow(noise(pos_world, 5000, deg(37.17f) * (float)i, offs, -1,+1), 2);

				++i;
			}
			{
				float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
				offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
				elevation = pow(noise(pos_world, 10000, deg(37.17f) * (float)i, offs, -1,+1), 2) * 1700 * elev_mult;

				++i;
			}
			{
				float2 offs = (i % 3 ? +1 : -1) * 12.34f * (float)i;
				offs[i % 2] = (i % 2 ? -1 : +1) * 43.21f * (float)i;
				elevation += noise(pos_world, wg.elev_freq, deg(37.17f) * (float)i, offs, -1,+1) * wg.elev_amp;

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

			return elevation - 20 +(roughness * detail);
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


		inline block_id gen_block (float3 pos) {
			float noise_val = noise::perlin(pos.x / 12) * 20;

			return pos.z + 0.5f <= noise_val ? B_GRASS : B_AIR;
		}

		void gen_terrain (int3 chunk_origin) {
			ZoneScoped;
			
			float lod_scale = (float)(1u << lod);

		#if 1 // noise dev test
			int3 i;
			for (i.z=0; i.z<CHUNK_SIZE; ++i.z) {
				for (i.y=0; i.y<CHUNK_SIZE; ++i.y) {
					for (i.x=0; i.x<CHUNK_SIZE; ++i.x) {
						float3 pos_world = ((float3)i + 0.5f) * lod_scale + (float3)chunk_origin;

						blocks[i.z][i.y][i.x] = gen_block(pos_world);
					}
				}
			}

			return;
		#endif

			float water_level = 0; // z=0 is first air above water

			{
				ZoneScopedN("2d noise eval");

				int2 i;
				for (i.y=0; i.y<CHUNK_SIZE; ++i.y) {
					for (i.x=0; i.x<CHUNK_SIZE; ++i.x) {
						float2 pos_world = ((float2)i + 0.5f) * lod_scale + (float2)int2(chunk_origin);

						float earth_layer;
						float height = clac_heightmap(pos_world, &earth_layer);
						float sand_thickness = noise(pos_world, 7.2f, deg(237.17f), 0, 1.2f, 2.7f);

						heightmap[i.y][i.x] = height;
						earth_layers[i.y][i.x] = earth_layer;
						sand_thicknesss[i.y][i.x] = sand_thickness;
					}
				}
			}

			{
				ZoneScopedN("3d block write");

				int3 i;
				for (i.z=0; i.z<CHUNK_SIZE; ++i.z) {
					for (i.y=0; i.y<CHUNK_SIZE; ++i.y) {
						for (i.x=0; i.x<CHUNK_SIZE; ++i.x) {
							float3 pos_world = ((float3)i + 0.5f) * lod_scale + (float3)chunk_origin;

							block_id bid;

							float height = heightmap[i.y][i.x];
							float earth_layer = earth_layers[i.y][i.x];
							earth_layer = max(earth_layer, lod_scale);

							float sand_thickness = sand_thicknesss[i.y][i.x];

							bool beach = abs(height - water_level) <= sand_thickness/2;

							if (pos_world.z <= height) {
								if (pos_world.z <= height - earth_layer) {
									bid = B_STONE;
								} else {
									if (beach) {
										bid = B_SAND;
									} else {
										if (height < water_level) {
											bid = B_PEBBLES;
										} else {
											if ((height - pos_world.z) <= lod_scale) {
												bid = B_GRASS;
											} else {
												bid = B_EARTH;
											}
										}
									}
								}
							} else {
								bid = pos_world.z <= water_level ? B_WATER : B_AIR;
							}

							blocks[i.z][i.y][i.x] = bid;
						}
					}
				}
			}
		}

		void place_objects (int3 chunk_origin) {
			ZoneScoped;

			std::vector<int3> tree_poss;
		
			{
				ZoneScopedN("iterate blocks");

				auto find_min_tree_dist = [&] (int2 new_tree_pos) {
					float min_dist = +INF;
					for (int3 p : tree_poss)
						min_dist = min(min_dist, length((float2)(int2)p -(float2)new_tree_pos));
					return min_dist;
				};
	
				int3 i;
				for (i.z=0; i.z<CHUNK_SIZE; ++i.z) {
					for (i.y=0; i.y<CHUNK_SIZE; ++i.y) {
						for (i.x=0; i.x<CHUNK_SIZE; ++i.x) {
							int3 pos_world = i + chunk_origin;
					
							block_id* bid   =           &blocks[i.z    ][i.y][i.x];
							block_id* below = i.z > 0 ? &blocks[i.z - 1][i.y][i.x] : nullptr;
	
							if (*bid == B_AIR && below && (*below == B_GRASS || *below == B_EARTH)) {
								float tree_density = noise_tree_density((float2)(int2)pos_world);
	
								float grass_density = noise_grass_density((float2)(int2)pos_world);
	
								float tree_prox_prob = gradient<float>( find_min_tree_dist((int2)i), {
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
			}
	
			auto place_tree = [&] (int3 pos) {
				ZoneScopedN("place_tree");
			
				auto place_block = [&] (int3 pos, block_id bt) {
					if (any(pos < 0 || pos >= CHUNK_SIZE)) return;

					block_id* bid = &blocks[pos.z][pos.y][pos.x];
	
					if (bid && (*bid == B_AIR || *bid == B_LEAVES)) {
						*bid = bt;
					}
				};
				auto place_block_sphere = [&] (int3 pos, float3 r, block_id bt) {
					int3 start = (int3)floor((float3)pos +0.5f -r);
					int3 end = (int3)ceil((float3)pos +0.5f +r);
	
					int3 i; // position in chunk
					for (i.z=start.z; i.z<end.z; ++i.z) {
						for (i.y=start.y; i.y<end.y; ++i.y) {
							for (i.x=start.x; i.x<end.x; ++i.x) {
								if (length_sqr((float3)(i - pos) / r) <= 1) place_block(i, bt);
							}
						}
					}
				};
	
				int tree_height = 6;
	
				for (int i=0; i<tree_height; ++i)
					place_block(pos +int3(0,0,i), B_TREE_LOG);
	
				place_block_sphere(pos +int3(0,0,tree_height-1), float3(float2(3.2f),tree_height/2.5f), B_LEAVES);
			};
		
			for (int3 p : tree_poss)
				place_tree(p);
		}

		void gen () {
			ZoneScoped;

			osn_noise2 = OSN::Noise<2>(wg.seed);

			auto chunk_seed = wg.seed ^ hash(chunk->pos);
			rand = Random(chunk_seed);

			gen_terrain(chunk->pos);

			//if (lod == 0)
			//	place_objects(chunk->pos);

			svo.chunk_to_octree(chunk, &blocks[0][0][0]);
		}
	};

	void WorldGenerator::generate_chunk (Chunk* chunk, SVO& svo) const {
		ZoneScoped;

		int lod = chunk->scale - CHUNK_SCALE;

		ChunkGenerator* gen;
		{
			ZoneScopedN("alloc buffer");
			gen = new ChunkGenerator{ chunk, svo, *this, lod };
		}

		gen->gen();

		{
			ZoneScopedN("free buffer");
			delete gen;
		}
	}
}
