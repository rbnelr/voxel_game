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
		float depth = wg->base_depth;
		
		//float sphere = (length(pos) - 180) * 1.0f; // for VCT dev
		//depth = max(sphere, 0.0f);

		float3 p = pos;
		// make noise 'flatter' 0.7 to flatten world
		p.z /= 0.7;

		float seed = 0;
		apply_noise_layers(depth, seed, p, wg->large_noise);
		
		// smaller ridges in wall
		float cut = noise(pos / float3(4,4,1), 15, seed++);
		depth -= max(cut * 4, 0.0f);

		return depth;
	}

	BlockID NoisePass::cave_noise (float3 const& pos, float large_noise, float3 const& normal) {
		float depth = large_noise;
		
		// small scale
		float seed = (float)wg->large_noise.size();

		apply_noise_layers(depth, seed, pos, wg->small_noise);

		float modifer = noise01(pos / float3(1,1,3), 14, seed++);
		
		//float ground = wall;
		//float ground = clamp(0.3f - abs(normal.z));

		// soil cover
		float ground = clamp(map(normal.z, wg->ground_ang, 1.0f));
		depth += wg->earth_overhang_stren * clamp(ground * 3); // create small overhangs of earth

		float beach_lo = wg->water_level - (2 + modifer*2.5f);
		float beach_hi = wg->water_level + (0.2f + modifer*1.2f);
		bool beach = pos.z >= beach_lo && pos.z < beach_hi;

		if (depth > 0) {

			if (depth < wg->earth_depth * ground) {// thinner earth layer on steeper slopes
				if (beach) return B_SAND;
				if (pos.z >= wg->water_level) {
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
		
		// air & water

		//water_level += (int)pos.x / 20; // for vct dev

		////float wall_thickness = max(-pos.y, 0.0f) * 0.05f; // for vct dev
		//float wall_thickness = 1.0f; // for vct dev
		//if (abs(pos.x) <= wall_thickness/2) {
		//	return B_STONE;
		//}

		if (pos.z >= wg->water_level) {
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

#if 0
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
	
	auto chance (double& rand, double prob) {
		double probd = prob;

		bool happened = rand <= probd;
		// transform uniform random value such that [0,prob] becomes [0,1]
		// so that successive checks are independent of the result of this one
		// Note: this effectively removes bits from the value (50% prob cuts 1 bit, 1% change cuts 99% of number space)
		if (happened)	rand = rand / probd;
		else			rand = (rand - probd) / (1.0 - probd);
		return happened;
	}

	void object_pass (Chunks& chunks, chunk_id cid, Neighbours& neighbours, WorldGenerator const* wg) {
		ZoneScoped;
		
		OSN::Noise<2> noise (wg->seed);
		OSN::Noise<3> noise3 (wg->seed);

		int3 chunkpos = chunks.chunks[cid].pos * CHUNK_SIZE;

		// write block with coord relative to this chunk, writes outside of this chunk are ignored
		auto write_block = [&] (int x, int y, int z, BlockID bid) -> void {
			int bx, by, bz;
			int cx, cy, cz;
			CHUNK_BLOCK_POS(x,y,z, cx,cy,cz, bx,by,bz);

			if (abs(cz) <= 1 || abs(cy) <= 1 || abs(cx) <= 1)
				chunks.write_block(bx,by,bz, neighbours.neighbours[cz+1][cy+1][cx+1], wg->bids[bid]);
		};
		auto read_block = [&] (int x, int y, int z) -> block_id {
			int bx, by, bz;
			int cx, cy, cz;
			CHUNK_BLOCK_POS(x,y,z, cx,cy,cz, bx,by,bz);

			if (abs(cz) > 1 || abs(cy) > 1 || abs(cx) > 1)
				return B_NULL;
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

			for (int z=start.z; z<end.z; ++z)
			for (int y=start.y; y<end.y; ++y)
			for (int x=start.x; x<end.x; ++x) {
				// check dist^2 of block to ellipsoid center,  /radius turns radius 1 sphere into desired ellipsoid
				if (length_sqr(((float3)int3(x,y,z) + 0.5f - center) / radius) <= 1)
					replace_block(x,y,z, bid);
			}
		};
		auto place_tree = [&] (int x, int y, int z, float rand1, float rand2) {
			float height = lerp(6, 18, rand1);
			float r = lerp(0.6f, 1.4f, rand2);

			float leaf_z =     height * 0.7f;
			float leaf_r = min(height * 0.33f * r, 4.0f);
			float leaf_h =     height * 0.33f;

			int trunk_h = roundi(leaf_z);

			float3 leaf_center = float3(x + 0.5f, y + 0.5f, z + leaf_z);
			float3 leaf_radius = float3(leaf_r,leaf_r,leaf_h);

			{
				for (int i=0; i<trunk_h; ++i) {
					block_id bid = read_block(x,y, z + i);
					if (bid != wg->bids[B_AIR] && bid != wg->bids[B_LEAVES])
						return;
				}

				int3 start = roundi((float3)leaf_center -leaf_radius);
				int3 end   = roundi((float3)leaf_center +leaf_radius);
				
				int count = 0;
				for (int cz=start.z; cz<end.z; ++cz)
				for (int cy=start.y; cy<end.y; ++cy)
				for (int cx=start.x; cx<end.x; ++cx) {
					if (length_sqr(((float3)int3(cx,cy,cz) + 0.5f - leaf_center) / leaf_radius) <= 1) {
						block_id bid = read_block(cx,cy,cz);
						if (!(bid == wg->bids[B_AIR] || bid == wg->bids[B_LEAVES]))
							count++;
					}
				}

				float volume = 4.0f/3.0f * PI * round(leaf_radius.x) * round(leaf_radius.y) * round(leaf_radius.z);
				if ((float)count / volume > 0.05f)
					return;
			}
			
			for (int i=0; i<trunk_h; ++i)
				replace_block(x,y, z + i, B_TREE_LOG);

			place_block_ellipsoid(leaf_center, leaf_radius, B_LEAVES);
		};

		for (int z=0; z<CHUNK_SIZE; ++z)
		for (int y=0; y<CHUNK_SIZE; ++y)
		for (int x=0; x<CHUNK_SIZE; ++x) {
			auto bid = chunks.read_block(x,y,z, cid);

			if (bid == wg->bids[B_AIR]) {
				auto below = read_block(x,y,z-1);
				auto above = read_block(x,y,z+1);

				int wx = x + chunkpos.x;
				int wy = y + chunkpos.y;
				int wz = z + chunkpos.z;

				auto place_stalac = [&] (int x, int y, int z, float rand1, float rand2) {
					if (wx > -5) return; // for vct dev

					float scale = lerp(0.4f, 2.5f, rand1);

					int variation = clamp(floori(rand2 * 6), 0,5);
					static constexpr BlockID BIDS[] = { B_CRYSTAL, B_CRYSTAL2, B_CRYSTAL3, B_CRYSTAL4, B_CRYSTAL5, B_CRYSTAL6 };
					auto bid = BIDS[variation];

					float base_r = 3 * scale;
					int h = roundi(20 * scale);
					for (int oz=-5; oz<h; ++oz) {

						int w = ceili(base_r)+1;
						for (int ox=-w; ox<=w; ++ox)
						for (int oy=-w; oy<=w; ++oy) {
							float r = 1.0f - (float)oz/(float)h;
							r = lerp(0.5f, base_r, powf(r, 1.4f) + 0.0001f);
							r *= map(noise3.eval((float)(wx+ox) * 20, (float)(wy+oy) * 20, (float)(wz+oz) * 4), -1.0f, +1.0f, 0.6f, 1.4f);

							if (length_sqr((float2)int2(ox,oy) / r) <= 1) {
								replace_block(x+ox, y+oy, z-oz, bid);
							}
						}
					}
				};

				if (below == wg->bids[B_EARTH]) {

					write_block(x,y,z-1, B_GRASS);

					// get a 'random' but deterministic value based on block position and face
					uint64_t h = hash(int3(wx*6 +BF_TOP, wy,wz), wg->seed);

					double rand = (double)h * (1.0 / (double)(uint64_t)-1); // uniform in [0, 1]
					float rand1 = (float)(h & 0xffffffff) * (1.0f / (float)(uint32_t)-1); // uniform in [0, 1]
					float rand2 = (float)(h >> 32)        * (1.0f / (float)(uint32_t)-1); // uniform in [0, 1]

					float tree_density = noise_tree_density(*wg, noise, float2((float)wx, (float)wy));
					float grass_density = noise_grass_density(*wg, noise, float2((float)wx, (float)wy));

					if (chunks.blue_noise_tex.sample(wx,wy,wz) < tree_density) {
						place_tree(x,y,z, rand1, rand2);
					}
					else if (chunks.blue_noise_tex.sample(wx+17,wy-13,wz+3) < (grass_density * 0.05f)) {
						write_block(x,y,z, B_GLOWSHROOM);
					}
					else if (!wg->disable_grass && chance(rand, (double)grass_density)) {
						write_block(x,y,z, B_TALLGRASS);
					}
					else if (!wg->disable_grass && chance(rand, 0.003)) {
						write_block(x,y,z, B_TORCH);
					}
				}


				if (above == wg->bids[B_STONE] && wg->stalac) {

					// get a 'random' but deterministic value based on block position and face
					uint64_t h = hash(int3(wx*6 +BF_BOTTOM, wy,wz), wg->seed);

					float rand1 = (float)(h & 0xffffffff) * (1.0f / (float)(uint32_t)-1); // uniform in [0, 1]
					float rand2 = (float)(h >> 32)        * (1.0f / (float)(uint32_t)-1); // uniform in [0, 1]

					if (chunks.blue_noise_tex.sample(wx,wy,wz) < wg->stalac_dens * 0.001f) {
						place_stalac(x,y,z, rand1, rand2);
					}
				}
			}
		}
	}
}

void WorldgenJob::execute () {
	noise_pass.generate();
}

