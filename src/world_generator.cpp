#include "common.hpp"
#include "world_generator.hpp"
#include "blocks.hpp"
#include "chunks.hpp"

#include "immintrin.h"

__m128 lerp (__m128 a, __m128 b, float t) {
	__m128 sub = _mm_sub_ps(b, a);
	__m128 mul = _mm_mul_ps(sub, _mm_set_ps1(t)); 
	__m128 add = _mm_add_ps(mul, a); 
	return add;
}

namespace worldgen {
	float NoisePass::calc_large_noise (float3 const& pos) {
		float3 p = pos;
		// make noise 'flatter' 0.7 to flatten world
		p.z /= 0.7;

		float depth = wg->base_depth;

		float seed = 0;
		for (auto& n : wg->large_noise) {
			float val = noise(p, n.period, seed++) * n.strength;
			if (n.cutoff) val = max(val, n.cutoff_val);

			depth += val;
		}
		
		// smaller ridges in wall
		float cut = noise(pos / float3(4,4,1), 15, seed++);
		depth -= max(cut * 4, 0.0f);

		return depth;
	}

	BlockID NoisePass::cave_noise (float3 const& pos, float large_noise, float3 const& normal) {
		int water_level = 23;

		float depth = large_noise;
		
		// small scale
		float seed = (float)wg->small_noise.size();

		for (auto& n : wg->small_noise) {
			float val = noise(pos, n.period, seed++) * n.strength;
			if (n.cutoff) val = max(val, n.cutoff_val);
		
			depth += val;
		}

		float modifer = noise01(pos / float3(1,1,3), 14, seed++);
		
		//float ground = wall;
		//float ground = clamp(0.3f - abs(normal.z));

		// soil cover
		float ground = clamp(map(normal.z, wg->ground_ang, 1.0f));
		depth += wg->earth_overhang_stren * clamp(ground * 3); // create small overhangs of earth

		float beach_lo = water_level - (2 + modifer*2.5f);
		float beach_hi = water_level + (0.2f + modifer*1.2f);
		bool beach = pos.z >= beach_lo && pos.z < beach_hi;

		if (depth > 0) {

			if (depth < wg->earth_depth * ground) {// thinner earth layer on steeper slopes
				if (beach) return B_SAND;
				if (pos.z >= water_level) {
					return B_EARTH;
				} else {
					return B_GRAVEL;
				}
			}

			// rock

			// erode rock
			if (normal.z > -0.6f) {
				float stren = max(modifer - 0.1f, 0.0f);
				//stren = stren * stren;
			
				depth -= max(noise(pos / float3(1,1,3), 3, seed++) * 8 * stren, 0.0f);
			}

			if (depth > 0) { // eroded cuts air into the 'base' depth, thus exposing depth > 0
				if (depth < wg->rock_depth) {
					if (modifer < 0.1f)	return B_URANIUM;
					return B_STONE; 
				} else {
					if (modifer > 0.8f)		return B_MAGMA;
					return B_HARDSTONE;
				}
			}
		}

		// stalactites
		if (wg->stalac) {
			float stalac = noise01(pos / float3(1,1,64), 6, 3) * clamp(map(normal.z, -0.2f, -0.9f));
			stalac = clamp(map(stalac, 0.4f, 1.0f));
			stalac = stalac*stalac*stalac * wg->stalac_stren;

			if ((depth + stalac) > 0)
				//return B_STONE;
				return B_CRYSTAL;
		}
		
		// air & water
		if (pos.z >= water_level) {
			return B_AIR;
		} else {
			return B_WATER;
		}
	}

	float large_noise_get_val (__m128 n) {
		return _mm_cvtss_f32(n);
	}
	float3 large_noise_normalize_derivative (__m128 n) {
		__m128 dot = _mm_dp_ps(n, n, 0xef); // deriv x*x + y*y + z*z; in all lanes
		__m128 sqrt = _mm_rsqrt_ps(dot);
		__m128 res = _mm_mul_ps(n, sqrt);

		return *(float3*)&res.m128_f32[1];
	}

	void NoisePass::generate () {
		ZoneScoped;

		int3 chunkpos = chunk_pos * CHUNK_SIZE;

		{ // large noise generate
			ZoneScopedN("large noise generate");

			float3 pos_world;

			for (int z=0; z<LARGE_NOISE_COUNT; ++z) {
				pos_world.z = (float)(z * LARGE_NOISE_SIZE + chunkpos.z);

				for (int y=0; y<LARGE_NOISE_COUNT; ++y) {
					pos_world.y = (float)(y * LARGE_NOISE_SIZE + chunkpos.y);

					for (int x=0; x<LARGE_NOISE_COUNT; ++x) {
						pos_world.x = (float)(x * LARGE_NOISE_SIZE + chunkpos.x);

						float val = calc_large_noise(pos_world);
						
						// calculate negative numerical derivative, with 1 block offsets
						float dx = val - calc_large_noise(pos_world + float3(1,0,0));
						float dy = val - calc_large_noise(pos_world + float3(0,1,0));
						float dz = val - calc_large_noise(pos_world + float3(0,0,1));

						_mm_store_ps(large_noise[z][y][x], _mm_set_ps(dz,dy,dx, val));
					}
				}
			}
		}

		{ // 3d noise generate
			ZoneScopedN("3d noise generate");

			float3 pos_world;

			for (int lz=0; lz<LARGE_NOISE_CHUNK_SIZE; ++lz)
			for (int ly=0; ly<LARGE_NOISE_CHUNK_SIZE; ++ly)
			for (int lx=0; lx<LARGE_NOISE_CHUNK_SIZE; ++lx) {

				auto ln000 = _mm_load_ps(large_noise[lz  ][ly  ][lx  ]);
				auto ln001 = _mm_load_ps(large_noise[lz  ][ly  ][lx+1]);
				auto ln010 = _mm_load_ps(large_noise[lz  ][ly+1][lx  ]);
				auto ln011 = _mm_load_ps(large_noise[lz  ][ly+1][lx+1]);
				auto ln100 = _mm_load_ps(large_noise[lz+1][ly  ][lx  ]);
				auto ln101 = _mm_load_ps(large_noise[lz+1][ly  ][lx+1]);
				auto ln110 = _mm_load_ps(large_noise[lz+1][ly+1][lx  ]);
				auto ln111 = _mm_load_ps(large_noise[lz+1][ly+1][lx+1]);

				for (int z=0; z<LARGE_NOISE_SIZE; z++) {
					
					// interpolate low-res lege noise to get values for individual blocks
					float tz = (float)z / LARGE_NOISE_SIZE;
					auto ln00 = lerp(ln000, ln100, tz);
					auto ln01 = lerp(ln001, ln101, tz);
					auto ln10 = lerp(ln010, ln110, tz);
					auto ln11 = lerp(ln011, ln111, tz);

					int cz = lz * LARGE_NOISE_SIZE + z;
					pos_world.z = (float)(cz + chunkpos.z);
					
					for (int y=0; y<LARGE_NOISE_SIZE; y++) {
						
						float ty = (float)y / LARGE_NOISE_SIZE;
						auto ln0  = lerp(ln00, ln10, ty);
						auto ln1  = lerp(ln01, ln11, ty);

						int cy = ly * LARGE_NOISE_SIZE + y;
						pos_world.y = (float)(cy + chunkpos.y);

						for (int x=0; x<LARGE_NOISE_SIZE; x++) {

							float tx = (float)x / LARGE_NOISE_SIZE;
							auto ln    = lerp(ln0, ln1, tx);

							int cx = lx * LARGE_NOISE_SIZE + x;
							pos_world.x = (float)(cx + chunkpos.x);

							auto large_noise = large_noise_get_val(ln);

							BlockID bid = B_UNBREAKIUM;
							if (large_noise < wg->max_depth) {
								float3 normal = large_noise_normalize_derivative(ln);

								bid = cave_noise(pos_world, large_noise, normal);
							}
							voxels[cz][cy][cx] = wg->bids[bid];
						}
					}
				}
			}
		}

	}

#if 1
#define SCZ (SUBCHUNK_COUNT*SUBCHUNK_COUNT)

#define BZ (SUBCHUNK_SIZE*SUBCHUNK_SIZE)
#define BY SUBCHUNK_SIZE

	template <typename FUNC>
	void iter_growable_blocks (Chunks& chunks, chunk_id cid, Neighbours& neighbours, WorldGenerator const* wg, FUNC block) {
		block_id air   = wg->bids[B_AIR];
		block_id earth = wg->bids[B_EARTH];
		
		auto& vox   = chunks.chunk_voxels[cid];
		auto& voxnz = chunks.chunk_voxels[neighbours.get(0,0,-1)];

		uint32_t subchunk_i = 0;
		for (int sz=0; sz<CHUNK_SIZE; sz += SUBCHUNK_SIZE) {
			ChunkVoxels& prev_vox = sz > 0 ? vox : voxnz;
			uint32_t    prev_offs = sz > 0 ? -SCZ : +SCZ * (SUBCHUNK_COUNT-1); // offset

			for (int sy=0; sy<CHUNK_SIZE; sy += SUBCHUNK_SIZE)
			for (int sx=0; sx<CHUNK_SIZE; sx += SUBCHUNK_SIZE) {

				uint32_t prev_subchunk_i = subchunk_i + prev_offs;

				uint32_t  prev_val    = prev_vox.subchunks[prev_subchunk_i];
				uint32_t  val         = vox     .subchunks[subchunk_i];

				if (!(prev_val & SUBC_SPARSE_BIT) || !(val & SUBC_SPARSE_BIT) || (prev_val == earth && val == air)) {
					uint32_t block_i = 0;
					for (int by=0; by<SUBCHUNK_SIZE; ++by)
					for (int bx=0; bx<SUBCHUNK_SIZE; ++bx) {

						block_id below = (prev_val & SUBC_SPARSE_BIT) ? (block_id)(prev_val & ~SUBC_SPARSE_BIT) : chunks.subchunks[prev_val].voxels[block_i + BZ * (SUBCHUNK_SIZE-1)];
						
						for (int bz=0; bz < ((val & SUBC_SPARSE_BIT) ? 1 : SUBCHUNK_SIZE); ++bz) {
							block_id bid = (val & SUBC_SPARSE_BIT) ? (block_id)(val & ~SUBC_SPARSE_BIT) : chunks.subchunks[val].voxels[block_i + bz*BZ];

							if (bid == air && below == earth)
								block(sx+bx, sy+by, sz+bz, below);

							below = bid;
						}

						block_i++;
					}
				}

				subchunk_i++;
			}
		}
	}
#else
	template <typename FUNC>
	void iter_growable_blocks (Chunks& chunks, chunk_id cid, Neighbours& neighbours, WorldGenerator const* wg, FUNC block) {
		for (int y=0; y<CHUNK_SIZE; ++y)
		for (int x=0; x<CHUNK_SIZE; ++x) {
			auto below = chunks.read_block(x,y, CHUNK_SIZE-1, neighbours.get(0,0,-1));

			for (int z=0; z<CHUNK_SIZE; ++z) {
				auto bid = chunks.read_block(x,y,z, cid);

				if (bid == wg->bids[B_AIR] && below == wg->bids[B_EARTH]) {
					block(x, y, z, below);
				}

				below = bid;
			}
		}
	}
#endif

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
	
	void object_pass (Chunks& chunks, chunk_id cid, Neighbours& neighbours, WorldGenerator const* wg) {
		ZoneScoped;
		
		OSN::Noise<2> noise(wg->seed);

		int3 chunkpos = chunks.chunks[cid].pos * CHUNK_SIZE;

		// write block with coord relative to this chunk, writes outside of this chunk are ignored
		auto write_block = [&] (int x, int y, int z, BlockID bid) -> void {
			int bx, by, bz;
			int cx, cy, cz;
			CHUNK_BLOCK_POS(x,y,z, cx,cy,cz, bx,by,bz);

			chunks.write_block(bx,by,bz, neighbours.neighbours[cz+1][cy+1][cx+1], wg->bids[bid]);
		};
		auto read_block = [&] (int x, int y, int z) -> block_id {
			int bx, by, bz;
			int cx, cy, cz;
			CHUNK_BLOCK_POS(x,y,z, cx,cy,cz, bx,by,bz);

			return chunks.read_block(bx,by,bz, neighbours.neighbours[cz+1][cy+1][cx+1]);
		};
		auto replace_block = [&] (int x, int y, int z, BlockID val) { // for tree placing
			auto bid = read_block(x,y,z);

			if (bid == wg->bids[B_AIR] || (val == B_TREE_LOG && bid == wg->bids[B_LEAVES])) { // replace air and water with tree log, replace leaves with tree log
				write_block(x,y,z, val);
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
		auto place_tree = [&] (int x, int y, int z, float height, float r) {
			float leaf_r = 4 * r;

			int h = roundi(height);
			for (int i=0; i<h; ++i)
				replace_block(x,y, z + i, B_TREE_LOG);

			place_block_ellipsoid(float3(x + 0.5f, y + 0.5f, z + height-0.5f), float3(leaf_r, leaf_r, height/2.5f), B_LEAVES);
		};

		iter_growable_blocks(chunks, cid, neighbours, wg, [&] (int x, int y, int z, block_id below) {
			write_block(x,y,z-1, B_GRASS);

			int3 wpos;
			wpos.x = x + chunkpos.x;
			wpos.y = y + chunkpos.y;
			wpos.z = z + chunkpos.z;

			// get a 'random' but deterministic value based on block position
			uint64_t h = hash(wpos, wg->seed);

			double rand = (double)h * (1.0 / (double)(uint64_t)-1); // uniform in [0, 1]
			float rand1 = (float)(h & 0xffffffff) * (1.0f / (float)(uint32_t)-1); // uniform in [0, 1]
			float rand2 = (float)(h >> 32)        * (1.0f / (float)(uint32_t)-1); // uniform in [0, 1]

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
			float2 pos2 = float2((float)wpos.x, (float)wpos.y);
			float tree_density  = noise_tree_density (*wg, noise, pos2);
			float grass_density = noise_grass_density(*wg, noise, pos2);

			if (chunks.blue_noise_tex.sample(wpos.x, wpos.y, wpos.z) < tree_density) {
				place_tree(x,y,z, lerp(6, 10, rand1), lerp(0.8f, 1.2f, rand2));
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
	noise_pass.generate();
}

