#include "common.hpp"
#include "world_generator.hpp"
#include "blocks.hpp"
#include "chunks.hpp"

#include "open_simplex_noise/open_simplex_noise.hpp"

namespace worldgen {
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

		float val = noise(pos_world, 180.0f, 0) * 180.0f - 50.0f;
		float dx = (noise(pos_world + float3(1,0,0), 180.0f, 0) * 180.0f - 50.0f) - val;
		float dy = (noise(pos_world + float3(0,1,0), 180.0f, 0) * 180.0f - 50.0f) - val;
		float dz = (noise(pos_world + float3(0,0,1), 180.0f, 0) * 180.0f - 50.0f) - val;
	
		float3 normal = normalize(float3(dx,dy,dz));
	
		float stalag_freq = 5.0f;
		float stalag = noise2.eval(pos_world.x / stalag_freq, pos_world.y / stalag_freq) * (abs(normal.z) - 0.2f);
		if (dz < 0) {
			stalag = max(stalag, 0.0f);
			stalag *= -40;
			val += stalag;
		}

		//float val = 0.0f;

		// large scale
		//val += noise(pos_world, 300.0f, 0) * 300.0f;

		// small scale
		val += noise(pos_world, 180.0f, 0) * 180.0f - 50.0f;
		val += noise(pos_world, 70.0f, float3(700,800,900)) * 70.0f;
		val += noise(pos_world, 4.0f, float3(700,800,900)) * 2.0f;

		//val += pos_world.z * 0.5f;

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

	void noise_pass (WorldgenJob& j) {
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

		int3 chunk_origin = j.chunk_pos * CHUNK_SIZE;

		int water_level = 21 - chunk_origin.z;

		OSN::Noise<2> noise2(j.wg->seed);
		OSN::Noise<3> noise3(j.wg->seed);

		block_id* blocks = j.voxel_output;

		{ // 3d noise generate
			ZoneScopedN("3d noise generate");

			block_id* cur = blocks;
			for (int z=0; z<CHUNK_SIZE; ++z) {
				for (int y=0; y<CHUNK_SIZE; ++y) {
					for (int x=0; x<CHUNK_SIZE; ++x) {
						int3 pos_world = int3(x,y,z) + chunk_origin;

						float val = cave_noise(noise3, noise2, (float3)pos_world);

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

	}

	void object_pass (WorldgenJob& j) {
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

		OSN::Noise<2> noise(j.wg->seed);

		int3 chunkpos = j.chunk_pos * CHUNK_SIZE;

		memcpy(j.voxel_output, j.phase1_voxels[1][1][1], sizeof(block_id) * CHUNK_VOXEL_COUNT);
		block_id* blocks = j.voxel_output;

		// read block with coord relative to this chunk, reads go through neighbours array, so neighbour chunks can also be read
		auto read_block = [&] (int x, int y, int z) -> chunk_id {
			assert(x >= -CHUNK_SIZE && x < CHUNK_SIZE*2 &&
			       y >= -CHUNK_SIZE && y < CHUNK_SIZE*2 &&
			       z >= -CHUNK_SIZE && z < CHUNK_SIZE*2);
			int bx, by, bz;
			int cx, cy, cz;
			CHUNK_BLOCK_POS(x,y,z, cx,cy,cz, bx,by,bz);

			return j.phase1_voxels[cz+1][cy+1][cx+1][IDX3D(bx,by,bz, CHUNK_SIZE)];
		};
		// write block with coord relative to this chunk, writes outside of this chunk are ignored
		auto write_block = [blocks] (int x, int y, int z, block_id bid) -> void {
			bool in_chunk = x >= 0 && x < CHUNK_SIZE &&
			                y >= 0 && y < CHUNK_SIZE &&
			                z >= 0 && z < CHUNK_SIZE;
			if (in_chunk)
				blocks[IDX3D(x,y,z, CHUNK_SIZE)] = bid;
		};

		auto replace_block = [&] (int x, int y, int z, block_id val) { // for tree placing
			bool in_chunk = x >= 0 && x < CHUNK_SIZE &&
			                y >= 0 && y < CHUNK_SIZE &&
			                z >= 0 && z < CHUNK_SIZE;
			if (!in_chunk) return;
			
			auto* bid = &blocks[IDX3D(x,y,z, CHUNK_SIZE)];
			if (*bid == AIR || *bid == WATER || (val == TREE_LOG && *bid == LEAVES)) { // replace air and water with tree log, replace leaves with tree log
				*bid = val;
			}
		};

		auto place_block_ellipsoid = [&] (float3 const& center, float3 const& radius, block_id bid) {
			// round makes the box include all blocks that have their center intersect with the ellipsoid
			int3 start = roundi((float3)center -radius);
			int3 end   = roundi((float3)center +radius);

			for (int z=start.z; z<end.z; ++z) {
				for (int y=start.y; y<end.y; ++y) {
					for (int x=start.x; x<end.x; ++x) {
						// check dist^2 of block to ellipsoid center,  /radius turns radius 1 sphere into desired ellipsoid
						if (length_sqr(((float3)int3(x,y,z) + 0.5f - center) / radius) <= 1)
							replace_block(x,y,z, bid);
					}
				}
			}
		};

		auto place_tree = [&] (int x, int y, int z) {
			int tree_height = 6;

			for (int i=0; i<tree_height; ++i)
				replace_block(x,y, z + i, TREE_LOG);

			place_block_ellipsoid(float3(x + 0.5f, y + 0.5f, z + tree_height-0.5f), float3(3.2f, 3.2f, tree_height/2.5f), LEAVES);
		};

		// how many blocks around the main chunk to process to fix cutoff trees
		static constexpr int borderxy = 5;
		static constexpr int borderz0 = 5 + 6; // need to add more depth to fix trees being cut off at the top (trees are higher than 5)
		static constexpr int borderz1 = 5; 

		// cx=-1 means that blocks [range[0], range[1]) are iterated, ie. [-border, -1]
		// cx= 0 means that blocks [range[1], range[2]) are iterated, ie. [0, CHUNK_SIZE-1]
		// cx=+1 means that blocks [range[2], range[3]) are iterated, ie. [CHUNK_SIZE, CHUNK_SIZE +border-1]
		static constexpr int rangexy[4] = { -borderxy, 0, CHUNK_SIZE, CHUNK_SIZE +borderxy };
		static constexpr int rangez[4] = { -borderz0, 0, CHUNK_SIZE, CHUNK_SIZE +borderz1 };

		// For neighbour chunks (and this chunk)
		for (int cz=-1; cz<=+1; ++cz)
		for (int cy=-1; cy<=+1; ++cy)
		for (int cx=-1; cx<=+1; ++cx) {

			// For blocks in chunk
			for (int y=rangexy[cy+1]; y<rangexy[cy+2]; ++y)
			for (int x=rangexy[cx+1]; x<rangexy[cx+2]; ++x) {

				for (int z=rangez[cz+1]; z<rangez[cz+2]; ++z) {
					auto bid   = read_block(x,y,z);
					if (bid == EARTH && read_block(x,y,z+1) == AIR) {
						write_block(x,y,z, GRASS);
							
						// get a 'random' but deterministic value based on block position
						uint64_t h = hash(int3(x,y,z) + chunkpos, j.wg->seed);

						double rand = (double)h * (1.0 / (double)(uint64_t)-1); // uniform in [0, 1]

						auto chance = [&] (float prob) {
							double probd = (double)prob;

							bool happened = rand <= probd;
							// transform uniform random value such that [0,prob] becomes [0,1]
							// so that successive checks are independent of the result of this one
							// Note: this effectively removes bits from the value (50% prob cuts 1 bit, 1% change cuts 99% of number space)
							if (happened)	rand = rand / probd;
							else			rand = (rand - probd) / (1.0 - probd);
							return happened;
						};

						int wx = x + chunkpos.x;
						int wy = y + chunkpos.y;
						int wz = z + chunkpos.z;

						//
						float tree_density  = noise_tree_density (*j.wg, noise, (float2)int2(wx, wy));
						float grass_density = noise_grass_density(*j.wg, noise, (float2)int2(wx, wy));

						if (j.chunks->blue_noise_tex.sample(wx,wy,wz) < tree_density) {
							place_tree(x,y,z+1);
						}
						else if (chance(grass_density)) {
							write_block(x,y,z+1, TALLGRASS);
						}
						else if (chance(0.0005f)) {
							write_block(x,y,z+1, TORCH);
						}
					}
				}
			}
		}
	}
}

void WorldgenJob::execute () {
	if (phase == 1)
		worldgen::noise_pass(*this);
	else if (phase == 2)
		worldgen::object_pass(*this);
}

