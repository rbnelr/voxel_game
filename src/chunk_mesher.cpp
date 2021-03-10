#include "common.hpp"
#include "chunk_mesher.hpp"
#include "world_generator.hpp"
#include "assets.hpp"
#include "player.hpp"

struct CallCtx {
	RemeshChunkJob& j;
	int			x, y, z;
	
	void block_mesh (block_id bid, int meshid) const {
		// get a 'random' but deterministic value based on block position
		uint64_t h = hash(int3(x,y,z)) ^ j.chunk_seed;

		float rand1 = (float)( h        & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // uniform in [0, 1)
		float rand2 = (float)((h >> 32) & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // uniform in [0, 1)

		// random 2d offset
		float rand_offsx = rand1;
		float rand_offsy = rand2;

		auto& tile = j.block_tiles[bid];

		// get a random deterministic variant
		int variant = (int)(rand1 * (float)tile.variants); // [0, tile.variants)

		int texid = tile.calc_tex_index((BlockFace)0, variant);

		auto& info = j.block_meshes_meshes[meshid];

		float posx = (float)x + (rand_offsx * 2 - 1) * 0.25f * info.offs_strength; // [0,1] -> [-1,+1]
		float posy = (float)y + (rand_offsy * 2 - 1) * 0.25f * info.offs_strength; // [0,1] -> [-1,+1]
		float posz = (float)z;

		int16_t fixd_posx = (int16_t)roundi(posx * BlockMeshInstance_FIXEDPOINT_FAC);
		int16_t fixd_posy = (int16_t)roundi(posy * BlockMeshInstance_FIXEDPOINT_FAC);
		int16_t fixd_posz = (int16_t)roundi(posz * BlockMeshInstance_FIXEDPOINT_FAC);

		for (int meshid=info.index; meshid < info.index + info.length; ++meshid) {
			auto* v = j.opaque_vertices.push();
			if (!v) return;
			v->posx = fixd_posx;
			v->posy = fixd_posy;
			v->posz = fixd_posz;
			v->texid = texid;
			v->meshid = meshid;
		}
	}

	static __forceinline void face (RemeshChunkJob& j, block_id bid, int x, int y, int z, ChunkMeshData* mesh, BlockFace facei) {
		auto* v = mesh->push();
		if (!v) return;

		v->posx = (int16_t)(x << BlockMeshInstance_FIXEDPOINT_SHIFT);
		v->posy = (int16_t)(y << BlockMeshInstance_FIXEDPOINT_SHIFT);
		v->posz = (int16_t)(z << BlockMeshInstance_FIXEDPOINT_SHIFT);
		v->texid = j.block_tiles[bid].calc_tex_index(facei, 0);
		v->meshid = facei;
	}

	template <int AXIS>
	void face (block_id bid, block_id nid) const {
		if (!j.mesh_world_border && (bid == B_NULL || nid == B_NULL))
			return;

		auto& b  = j.block_types[bid];
		auto& nb = j.block_types[nid];

		// generate face of our voxel that faces negative direction neighbour
		if (b.collision != CM_GAS && nb.transparency != TM_OPAQUE && j.block_meshes[bid] < 0) {
			auto* mesh = b.transparency == TM_TRANSPARENT ? &j.transp_vertices : &j.opaque_vertices;
			face(j, bid, x,y,z, mesh, (BlockFace)(BF_NEG_X + AXIS*2));
		}

		// generate face of negative direction neighbour that faces this voxel
		if (nb.collision != CM_GAS && b.transparency != TM_OPAQUE && j.block_meshes[nid] < 0 && nid != B_NULL) {
			auto* mesh = nb.transparency == TM_TRANSPARENT ? &j.transp_vertices : &j.opaque_vertices;
			int nx = AXIS == 0 ? x-1 : x;
			int ny = AXIS == 1 ? y-1 : y;
			int nz = AXIS == 2 ? z-1 : z;
			face(j, nid, nx,ny,nz, mesh, (BlockFace)(BF_POS_X + AXIS*2));
		}
	}
	
	// offsets of subchunk 3d neighbours in flat buffer
	#define SCX  1
	#define SCY  SUBCHUNK_COUNT
	#define SCZ (SUBCHUNK_COUNT * SUBCHUNK_COUNT)

	// offsets of block 3d neighbours in flat buffer
	#define BX  1
	#define BY  SUBCHUNK_SIZE
	#define BZ (SUBCHUNK_SIZE * SUBCHUNK_SIZE)

	struct _Chunk {
		int			sparse;
		uint32_t	voxel_data;
	};
	_Chunk get_chunk (chunk_id cid) {
		if (cid == U16_NULL)
			return { true, (uint32_t)B_NULL };
	
		return { false, cid };
	}

	struct _Subchunk {
		int			sparse;
		block_id*	ptr;

		block_id read (uint32_t block_i) {
			return ptr[!sparse ? block_i : 0];
		}
	};
	_Subchunk get_subchunk (uint32_t subchunk_i, _Chunk& chunk) {
		if (chunk.sparse)
			return { true, (block_id*)&chunk.voxel_data };

		auto& dc = j.chunk_voxels[chunk.voxel_data];
		if (dc.is_subchunk_sparse(subchunk_i))
			return { true, (block_id*)&dc.sparse_data[subchunk_i] }; // sparse subchunk

		return { false, j.subchunks[ dc.sparse_data[subchunk_i] ].voxels }; // dense subchunk
	}

	// Chunk meshing optimized for speed
	// works directly with sparse voxel storage
	// makes use of sparseness to _very_ quickly mesh chunks that contain mostly sparse subchunks
	//  numbers: meshing can be as fast as 32us for mostly sparse chunks compared to 0.5ms-2ms for normal chunks
	//    thats a 30x speedup for air and stone chunks! 
	//    and even ragular chunks get some speedup due to empty regions, although currently the overhead makes me slightly slower than a dense 3d arary version was if I remember ~.5ms timings correctly

	void mesh_chunk () {
		ZoneScopedN("mesh_chunk");

		int _dense_subchunks = 0;

		// voxel_data of relevant chunks
		auto chunk  = get_chunk(j.chunk   );
		auto chunkx = get_chunk(j.chunk_nx);
		auto chunky = get_chunk(j.chunk_ny);
		auto chunkz = get_chunk(j.chunk_nz);

		uint32_t subchunk_i = 0;

		for (int sz = 0; sz < CHUNK_SIZE; sz += SUBCHUNK_SIZE) {
			int subc_offs_cz = sz > 0 ? -SCZ : SCZ*(SUBCHUNK_COUNT-1);
			_Chunk* subc_chunkz = sz > 0 ? &chunk : &chunkz;

		for (int sy = 0; sy < CHUNK_SIZE; sy += SUBCHUNK_SIZE) {
			int subc_offs_cy = sy > 0 ? -SCY : SCY*(SUBCHUNK_COUNT-1);
			_Chunk* subc_chunky = sy > 0 ? &chunk : &chunky;

		for (int sx = 0; sx < CHUNK_SIZE; sx += SUBCHUNK_SIZE) {
			int subc_offs_cx = sx > 0 ? -SCX : SCX*(SUBCHUNK_COUNT-1);
			_Chunk* subc_chunkx = sx > 0 ? &chunk : &chunkx;

			auto sc  = get_subchunk(subchunk_i, chunk);
			auto scx = get_subchunk(subchunk_i + subc_offs_cx, *subc_chunkx);
			auto scy = get_subchunk(subchunk_i + subc_offs_cy, *subc_chunky);
			auto scz = get_subchunk(subchunk_i + subc_offs_cz, *subc_chunkz);

			if (sc.sparse) {
				block_id bid = *sc.ptr;

				// X faces
				if (scx.sparse && *scx.ptr == bid) {
					// both subchunks sparse and cannot generate any faces
				} else {
					uint32_t block_i = 0;

					x = sx;
					for (    int bz=0; bz<SUBCHUNK_SIZE; ++bz) {	z = sz + bz;
						for (int by=0; by<SUBCHUNK_SIZE; ++by) {	y = sy + by;

							block_id prev = scx.read(block_i + (SUBCHUNK_SIZE-1)*BX);
							if (bid != prev) face<0>(bid, prev);

							block_i += BY;
						}
					}
				}

				// Y faces
				if (scy.sparse && *scy.ptr == bid) {
					// both subchunks sparse and cannot generate any faces
				} else {
					uint32_t block_i = 0;

					y = sy;
					for (    int bz=0; bz<SUBCHUNK_SIZE; ++bz) {	z = sz + bz;
						for (int bx=0; bx<SUBCHUNK_SIZE; ++bx) {	x = sx + bx;

							block_id prev = scy.read(block_i + (SUBCHUNK_SIZE-1)*BY);
							if (bid != prev) face<1>(bid, prev);

							block_i += BX;
						}
						block_i += BZ - BX*SUBCHUNK_SIZE;
					}
				}

				// Z faces
				if (scz.sparse && *scz.ptr == bid) {
					// both subchunks sparse and cannot generate any faces
				} else {
					uint32_t block_i = 0;

					z = sz;
					for (    int by=0; by<SUBCHUNK_SIZE; ++by) {	y = sy + by;
						for (int bx=0; bx<SUBCHUNK_SIZE; ++bx) {	x = sx + bx;
							block_id prev = scz.read(block_i + (SUBCHUNK_SIZE-1)*BZ);
							if (bid != prev) face<2>(bid, prev);

							block_i += BX;
						}
					}
				}

				// Block meshes
				auto& bm = j.block_meshes[bid];
				if (bm >= 0) {
					for (        int bz=0; bz<SUBCHUNK_SIZE; ++bz) {	z = sz + bz;
						for (    int by=0; by<SUBCHUNK_SIZE; ++by) {	y = sy + by;
							for (int bx=0; bx<SUBCHUNK_SIZE; ++bx) {	x = sx + bx;
								block_mesh(bid, bm);
							}
						}
					}
				}
			} else {
				_dense_subchunks++;

				//ZoneScopedN("mesh dense subchunk");

				// Straight single loop version
			#if 0
				uint32_t block_i = 0;
				for (int bz=0; bz<SUBCHUNK_SIZE; ++bz) {
					z = sz + bz;
					for (int by=0; by<SUBCHUNK_SIZE; ++by) {
						y = sy + by;
						for (int bx=0; bx<SUBCHUNK_SIZE; ++bx) {
							x = sx + bx;

							block_id bid = sc.ptr[block_i];

							block_id prevz = bz > 0 ? sc.ptr[block_i - BZ] : scz.ptr[scz.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BZ];
							block_id prevy = by > 0 ? sc.ptr[block_i - BY] : scy.ptr[scy.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BY];
							block_id prevx = bx > 0 ? sc.ptr[block_i - BX] : scx.ptr[scx.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BX];

							if (bid != prevx) face<0>(bid, prevx);
							if (bid != prevy) face<1>(bid, prevy);
							if (bid != prevz) face<2>(bid, prevz);

							auto& bm = j.block_meshes[bid];
							if (bm >= 0) block_mesh(bid, bm);

							block_i++;
						}
					}
				}

			#endif

			#if 1
				// write single sparse value in small dummy buffer to be able to offset ptr without branching
				block_id scy_dummy[SUBCHUNK_SIZE];
				block_id* scyptr = scy.ptr;
				if (scy.sparse) {
					auto val = *scyptr;
					for (int i=0; i<SUBCHUNK_SIZE; ++i)
						scy_dummy[i] = val;
					// modify copy of ptr, not original because doing that somehow made sparse subchunk code slower; doing copy makes it faster
					// maybe clobbered value that could be reused in next iteration somehow?
					scyptr = scy_dummy;
				}

				// write single sparse value in small dummy buffer to be able to offset ptr without branching
				block_id scz_dummy[SUBCHUNK_SIZE];
				block_id* sczptr = scz.ptr;
				if (scz.sparse) {
					auto val = *sczptr;
					for (int i=0; i<SUBCHUNK_SIZE; ++i)
						scz_dummy[i] = val;
					sczptr = scz_dummy;
				}

				uint32_t block_i = 0;

				auto* bms = j.block_meshes;

				for (    int bz=0; bz<SUBCHUNK_SIZE; ++bz) {	z = sz + bz;
					for (int by=0; by<SUBCHUNK_SIZE; ++by) {	y = sy + by;

						block_id* pbid = &sc.ptr[block_i];

						block_id* prevz = bz > 0 ? pbid-BZ : &sczptr[scz.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BZ];
						block_id* prevy = by > 0 ? pbid-BY : &scyptr[scy.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BY];
						block_id  prevx =                    scx.ptr[scx.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BX];

						x = sx;

						for (int bx=0; bx<SUBCHUNK_SIZE; bx += 4) {
							block_id bid = pbid[bx+0];
							if (bid != prevx      ) face<0>(bid, prevx      );
							if (bid != prevy[bx+0]) face<1>(bid, prevy[bx+0]);
							if (bid != prevz[bx+0]) face<2>(bid, prevz[bx+0]);
							if (bms[bid] >= 0) block_mesh(bid, bms[bid]);
							++x;
							prevx = bid;

							bid = pbid[bx+1];
							if (bid != prevx      ) face<0>(bid, prevx      );
							if (bid != prevy[bx+1]) face<1>(bid, prevy[bx+1]);
							if (bid != prevz[bx+1]) face<2>(bid, prevz[bx+1]);
							if (bms[bid] >= 0) block_mesh(bid, bms[bid]);
							++x;
							prevx = bid;

							bid = pbid[bx+2];
							if (bid != prevx      ) face<0>(bid, prevx      );
							if (bid != prevy[bx+2]) face<1>(bid, prevy[bx+2]);
							if (bid != prevz[bx+2]) face<2>(bid, prevz[bx+2]);
							if (bms[bid] >= 0) block_mesh(bid, bms[bid]);
							++x;
							prevx = bid;

							bid = pbid[bx+3];
							if (bid != prevx    ) face<0>(bid, prevx        );
							if (bid != prevy[bx+3]) face<1>(bid, prevy[bx+3]);
							if (bid != prevz[bx+3]) face<2>(bid, prevz[bx+3]);
							if (bms[bid] >= 0) block_mesh(bid, bms[bid]);
							++x;
							prevx = bid;
						}

						block_i += SUBCHUNK_SIZE;
					}
				}
			#endif

			#if 0
				uint32_t block_i = 0;

				{
					z = sz;
					{
						y = sy;
						{
							x = sx;
							block_id bid = sc.ptr[block_i];

							block_id prevx = scx.ptr[scx.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BX];
							if (bid != prevx) face<0>(bid, prevx);

							block_id prevy = scy.ptr[scy.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BY];
							if (bid != prevy) face<1>(bid, prevy);

							block_id prevz = scz.ptr[scz.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BZ];
							if (bid != prevz) face<2>(bid, prevz);

							auto& bm = j.block_meshes[bid];
							if (bm >= 0) block_mesh(bid, bm);

							block_i++;
							x++;
						}

						for (int bx=1; bx<SUBCHUNK_SIZE; ++bx) {
							block_id bid = sc.ptr[block_i];

							block_id prevx = sc.ptr[block_i - BX];
							if (bid != prevx) face<0>(bid, prevx);

							block_id prevy = scy.ptr[scy.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BY];
							if (bid != prevy) face<1>(bid, prevy);

							block_id prevz = scz.ptr[scz.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BZ];
							if (bid != prevz) face<2>(bid, prevz);

							auto& bm = j.block_meshes[bid];
							if (bm >= 0) block_mesh(bid, bm);

							block_i++;
							x++;
						}
						y++;
					}

					for (int by=1; by<SUBCHUNK_SIZE; ++by) {
						{
							x = sx;
							block_id bid = sc.ptr[block_i];

							block_id prevx = scx.ptr[scx.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BX];
							if (bid != prevx) face<0>(bid, prevx);

							block_id prevy = sc.ptr[block_i - BY];
							if (bid != prevy) face<1>(bid, prevy);

							block_id prevz = scz.ptr[scz.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BZ];
							if (bid != prevz) face<2>(bid, prevz);

							auto& bm = j.block_meshes[bid];
							if (bm >= 0) block_mesh(bid, bm);

							block_i++;
							x++;
						}

						for (int bx=1; bx<SUBCHUNK_SIZE; ++bx) {
							block_id bid = sc.ptr[block_i];

							block_id prevx = sc.ptr[block_i - BX];
							if (bid != prevx) face<0>(bid, prevx);

							block_id prevy = sc.ptr[block_i - BY];
							if (bid != prevy) face<1>(bid, prevy);

							block_id prevz = scz.ptr[scz.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BZ];
							if (bid != prevz) face<2>(bid, prevz);

							auto& bm = j.block_meshes[bid];
							if (bm >= 0) block_mesh(bid, bm);

							block_i++;
							x++;
						}
						y++;
					}
					z++;
				}

				for (int bz=1; bz<SUBCHUNK_SIZE; ++bz) {
					{
						y = sy;
						{
							x = sx;
							block_id bid = sc.ptr[block_i];

							block_id prevx = scx.ptr[scx.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BX];
							if (bid != prevx) face<0>(bid, prevx);

							block_id prevy = scy.ptr[scy.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BY];
							if (bid != prevy) face<1>(bid, prevy);

							block_id prevz = sc.ptr[block_i - BZ];
							if (bid != prevz) face<2>(bid, prevz);

							auto& bm = j.block_meshes[bid];
							if (bm >= 0) block_mesh(bid, bm);

							block_i++;
							x++;
						}

						for (int bx=1; bx<SUBCHUNK_SIZE; ++bx) {
							block_id bid = sc.ptr[block_i];

							block_id prevx = sc.ptr[block_i - BX];
							if (bid != prevx) face<0>(bid, prevx);

							block_id prevy = scy.ptr[scy.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BY];
							if (bid != prevy) face<1>(bid, prevy);

							block_id prevz = sc.ptr[block_i - BZ];
							if (bid != prevz) face<2>(bid, prevz);

							auto& bm = j.block_meshes[bid];
							if (bm >= 0) block_mesh(bid, bm);

							block_i++;
							x++;
						}
						y++;
					}

					for (int by=1; by<SUBCHUNK_SIZE; ++by) {
						{
							x = sx;
							block_id bid = sc.ptr[block_i];

							block_id prevx = scx.ptr[scx.sparse ? 0 : block_i + (SUBCHUNK_SIZE-1)*BX];
							if (bid != prevx) face<0>(bid, prevx);

							block_id prevy = sc.ptr[block_i - BY];
							if (bid != prevy) face<1>(bid, prevy);

							block_id prevz = sc.ptr[block_i - BZ];
							if (bid != prevz) face<2>(bid, prevz);

							auto& bm = j.block_meshes[bid];
							if (bm >= 0) block_mesh(bid, bm);

							block_i++;
							x++;
						}

						for (int bx=1; bx<SUBCHUNK_SIZE; ++bx) {
							block_id bid = sc.ptr[block_i];

							block_id prevx = sc.ptr[block_i - BX];
							if (bid != prevx) face<0>(bid, prevx);

							block_id prevy = sc.ptr[block_i - BY];
							if (bid != prevy) face<1>(bid, prevy);

							block_id prevz = sc.ptr[block_i - BZ];
							if (bid != prevz) face<2>(bid, prevz);

							auto& bm = j.block_meshes[bid];
							if (bm >= 0) block_mesh(bid, bm);

							block_i++;
							x++;
						}
						y++;
					}
					z++;
				}
			#endif

			}

			subchunk_i++;
		}
		}
		}

		ZoneValue(_dense_subchunks);
	}
};

void RemeshChunkJob::execute () {
	CallCtx ctx = { *this };
	ctx.mesh_chunk();
}

chunk_id get_neighbour_blocks (Chunks& chunks, Chunk& chunk, int neighbour) {
	int3 pos = chunk.pos;
	pos[neighbour] -= 1;

	//auto nid = j.chunks->chunks_arr.checked_get(pos.x, pos.y, pos.z);
	auto nid = chunks.query_chunk(pos);
	if (nid != U16_NULL && chunks[nid].flags != 0) {
		return nid;
	}
	return U16_NULL;
}

RemeshChunkJob::RemeshChunkJob (Chunks& chunks, chunk_id cid, WorldGenerator const& wg, bool mesh_world_border) {
	block_types			= g_assets.block_types.blocks.data();
	block_meshes		= g_assets.block_meshes.block_meshes.data();
	block_meshes_meshes	= g_assets.block_meshes.meshes.data();
	block_tiles			= g_assets.block_tiles.data();

	this->chunk_voxels		= chunks.chunk_voxels.arr;
	this->subchunks			= chunks.subchunks.arr;

	auto& chunk = chunks.chunks[cid];

	this->chunk    = cid;
	this->chunk_nx = get_neighbour_blocks(chunks, chunk, 0);
	this->chunk_ny = get_neighbour_blocks(chunks, chunk, 1);
	this->chunk_nz = get_neighbour_blocks(chunks, chunk, 2);

	this->mesh_world_border = mesh_world_border;
	chunk_seed = wg.seed ^ hash(chunk.pos * CHUNK_SIZE);
}
