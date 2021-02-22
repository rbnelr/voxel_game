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

	BlockID cave_noise (float3 const& pos_world, OSN::Noise<3> const& osn_noise, OSN::Noise<2> const& noise2) {
		int water_level = 21;

		auto noise01 = [&] (float3 pos, float period, float seed) {
			pos /= period; // period is inverse frequency

			return osn_noise.eval<float>(pos.x, pos.y, pos.z) * 0.5f + 0.5f;
		};
		auto noise = [&] (float3 pos, float period, float seed) {
			//pos += (1103 * float3(53, 211, 157)) * seed; // random prime directional offset to replace lack of seed in OSN noise
			pos /= period; // period is inverse frequency

			return osn_noise.eval<float>(pos.x, pos.y, pos.z / 0.7f) * period; // 0.7 to flatten world
		};

		auto large_freq = [noise] (float3 pos) {
			float val = 70;
			val += -noise(pos, 300, 0) + 30;
			val += -noise(pos, 180, 1);
			val += -noise(pos,  70, 2) * 1.5f;
			return val;
		};

		// large scale
		float base = large_freq(pos_world);

		if (base > 40)
			return B_UNBREAKIUM;

		// large normal vector
		float3 delta;
		delta.x = base - large_freq(pos_world + float3(1,0,0));
		delta.y = base - large_freq(pos_world + float3(0,1,0));
		delta.z = base - large_freq(pos_world + float3(0,0,1));
		float3 normal = normalize(delta);

		// stalag
		float stalag_freq = 6;
		float stalag = noise2.eval(pos_world.x / stalag_freq, pos_world.y / stalag_freq) * clamp(map(normal.z, -0.2f, -0.9f));
		base += stalag*stalag * 40;

		// small scale
		base += noise(pos_world, 4.0f, 4) * 0.3f;
		//base -= max(noise(pos_world, 20.0f, 3) * 0.9f, 0.0f);

		bool ground = normal.z > 0.55f;

		// erode rock
		float eroded = base;

		if (!ground && normal.z > -0.6f) {
			eroded -= 1.2f;

			float3 roughpos = pos_world;
			roughpos.z /= 14;

			float stren = noise01(roughpos, 14, 6);
			stren = max(stren - 0.1f, 0.0f);
			stren = stren * stren;

			eroded -= max(noise(roughpos, 3, 6) * 18 * stren, 0.0f);
		}

		{
			BlockID bid;
			if (pos_world.z >= water_level) {
				bid = B_AIR;
			} else {
				bid = B_WATER;
			}

			if (base > 0) {
				// positive -> in rock
				if (ground && base < 5.0f) {
					// ground soil
					bid = B_EARTH;
				} else {
					// rock
					if (eroded > 0) { // eroded cuts air into the 'base' depth, thus exposing depth > 0
						if (base < 10.0f) {
							bid = B_STONE; 
						} else {
							bid = B_HARDSTONE;
						}
					}
				}
			}
			return bid;
		}
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

		int3 chunkpos = j.chunk_pos * CHUNK_SIZE;

		OSN::Noise<2> noise2(j.wg->seed);
		OSN::Noise<3> noise3(j.wg->seed);

		block_id* blocks = j.voxel_output;

		{ // 3d noise generate
			ZoneScopedN("3d noise generate");

			block_id* cur = blocks;
			float3 pos_world;

			for (int z = chunkpos.z; z < chunkpos.z + CHUNK_SIZE; ++z) {
				pos_world.z = (float)z;
				
				for (int y = chunkpos.y; y < chunkpos.y + CHUNK_SIZE; ++y) {
					pos_world.y = (float)y;
					
					for (int x = chunkpos.x; x < chunkpos.x + CHUNK_SIZE; ++x) {
						pos_world.x = (float)x;

						auto bid = cave_noise(pos_world, noise3, noise2);
						*cur++ = j.wg->bids[bid];
					}
				}
			}
		}

	}

#if 1

}
#include "immintrin.h"
namespace worldgen {

	__m256i _read_subchunk_plane (Chunks& chunks, int sx, int sy, int z, Chunk const* c) {
		assert(c->flags != 0);

		block_id sparse;

		if (c->flags & Chunk::SPARSE_VOXELS) {
			// sparse chunk
			sparse = (block_id)c->voxel_data;
		} else {
			auto& dc = chunks.dense_chunks[c->voxel_data];

			uint32_t subchunk_i = IDX3D(sx,sy, z >> SUBCHUNK_SHIFT, SUBCHUNK_COUNT);
			auto subchunk_val = dc.sparse_data[subchunk_i];

			if (dc.is_subchunk_sparse(subchunk_i)) {
				// sparse subchunk
				sparse = (block_id)subchunk_val;
			} else {
				// dense subchunk
				auto& subchunk = chunks.dense_subchunks[subchunk_val];

				auto blocki = BLOCK_IDX(0,0,z);
				return _mm256_load_si256((__m256i*)&subchunk.voxels[blocki]);
			}
		}

		// sparse
		return _mm256_set1_epi16((short)sparse);
	}
	void _read_subchunk (Chunks& chunks, int sx, int sy, int sz, Chunk const* c, __m256i* planes) {
		assert(c->flags != 0);

		block_id sparse;

		if (c->flags & Chunk::SPARSE_VOXELS) {
			// sparse chunk
			sparse = (block_id)c->voxel_data;
		} else {
			auto& dc = chunks.dense_chunks[c->voxel_data];

			uint32_t subchunk_i = IDX3D(sx,sy,sz, SUBCHUNK_COUNT);
			auto subchunk_val = dc.sparse_data[subchunk_i];

			if (dc.is_subchunk_sparse(subchunk_i)) {
				// sparse subchunk
				sparse = (block_id)subchunk_val;
			} else {
				// dense subchunk
				auto& subchunk = chunks.dense_subchunks[subchunk_val];

				for (int z=0; z<4; ++z)
					planes[z] = _mm256_load_si256((__m256i*)&subchunk.voxels[z*16]);
				return;
			}
		}

		// sparse
		auto plane = _mm256_set1_epi16((short)sparse);
		for (int z=0; z<4; ++z)
			planes[z] = plane;
	}

	template <typename FUNC>
	void iter_growable_blocks (Chunks& chunks, Chunk& chunk, Neighbours& neighbours, WorldGenerator const* wg, FUNC block) {
		auto air   = _mm256_set1_epi16(wg->bids[B_AIR]);
		auto earth = _mm256_set1_epi16(wg->bids[B_EARTH]);
		
		for (int sy=0; sy<SUBCHUNK_COUNT; ++sy)
		for (int sx=0; sx<SUBCHUNK_COUNT; ++sx) {
			auto below = _read_subchunk_plane(chunks, sx,sy, CHUNK_SIZE-1, neighbours.get(0,0,-1));

			for (int sz=0; sz<SUBCHUNK_COUNT; ++sz) {
				__m256i subc[4];
				_read_subchunk(chunks, sx,sy,sz, &chunk, subc);

				for (int z=0; z<4; ++z) {
					auto cur = subc[z];

					auto a = _mm256_cmpeq_epi16(cur,   air);
					auto b = _mm256_cmpeq_epi16(below, earth);
					auto c = _mm256_testz_si256(a, b); // 1: all !=   0: any ==

					if (c == 0) {
						auto ab = _mm256_and_si256(a, b);

						for (int y=0; y<4; ++y)
						for (int x=0; x<4; ++x) {
							int i = y*4 + x;
							if (ab.m256i_u16[i])
								block(sx*4 + x, sy*4 + y, sz*4 + z, cur.m256i_u16[i]);
						}
					}

					below = cur;
				}
			}
		}
	}
#else
	template <typename FUNC>
	void iter_growable_blocks (Chunks& chunks, Chunk& chunk, Neighbours& neighbours, WorldGenerator const* wg, FUNC block) {
		for (int y=0; y<CHUNK_SIZE; ++y)
		for (int x=0; x<CHUNK_SIZE; ++x) {
			auto below = chunks.read_block(x,y, CHUNK_SIZE-1, neighbours.get(0,0,-1));

			for (int z=0; z<CHUNK_SIZE; ++z) {
				auto bid = chunks.read_block(x,y,z, &chunk);

				if (bid == wg->bids[B_AIR] && below == wg->bids[B_EARTH]) {
					block(x, y, z, below);
				}

				below = bid;
			}
		}
	}
#endif

	void object_pass (Chunks& chunks, Chunk& chunk, Neighbours& neighbours, WorldGenerator const* wg) {
		ZoneScoped;
		
		OSN::Noise<2> noise(wg->seed);

		int3 chunkpos = chunk.pos * CHUNK_SIZE;

		// write block with coord relative to this chunk, writes outside of this chunk are ignored
		auto write_block = [&] (int x, int y, int z, BlockID bid) -> void {
			chunks.write_block(x,y,z, wg->bids[bid]);
		};
		auto replace_block = [&] (int x, int y, int z, BlockID val) { // for tree placing
			auto bid = chunks.read_block(x,y,z);

			if (bid == wg->bids[B_AIR] || (val == B_TREE_LOG && bid == wg->bids[B_LEAVES])) { // replace air and water with tree log, replace leaves with tree log
				chunks.write_block(x,y,z, wg->bids[val]);
			}
		};

		auto place_block_ellipsoid = [&] (float3 const& center, float3 const& radius, BlockID bid) {
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
			float leaf_r = 3.2f;

			for (int i=0; i<tree_height; ++i)
				replace_block(x,y, z + i, B_TREE_LOG);

			place_block_ellipsoid(float3(x + 0.5f, y + 0.5f, z + tree_height-0.5f), float3(leaf_r, leaf_r, tree_height/2.5f), B_LEAVES);
		};

		iter_growable_blocks(chunks, chunk, neighbours, wg, [&] (int x, int y, int z, block_id below) {
			// convert to world coords
			x += chunkpos.x;
			y += chunkpos.y;
			z += chunkpos.z;

			write_block(x,y,z-1, B_GRASS);

			// get a 'random' but deterministic value based on block position
			uint64_t h = hash(int3(x,y,z), wg->seed);

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

			//
			float2 pos2 = float2((float)x, (float)y);
			float tree_density  = noise_tree_density (*wg, noise, pos2);
			float grass_density = noise_grass_density(*wg, noise, pos2);

			if (chunks.blue_noise_tex.sample(x,y,z) < tree_density) {
				place_tree(x,y,z);
			}
			else if (chance(grass_density)) {
				write_block(x,y,z, B_TALLGRASS);
			}
			else if (chance(0.003f)) {
				write_block(x,y,z, B_TORCH);
			}
		});
	}
}

void WorldgenJob::execute () {
	assert(phase == 1);
	
	worldgen::noise_pass(*this);
}

