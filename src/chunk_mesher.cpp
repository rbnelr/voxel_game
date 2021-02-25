#include "common.hpp"
#include "chunk_mesher.hpp"
#include "world_generator.hpp"
#include "assets.hpp"
#include "player.hpp"

void block_mesh (RemeshChunkJob& j, int x, int y, int z, block_id id, int meshid) {
	// get a 'random' but deterministic value based on block position
	uint64_t h = hash(int3(x,y,z)) ^ j.chunk_seed;

	float rand1 = (float)( h        & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // uniform in [0, 1)
	float rand2 = (float)((h >> 32) & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // uniform in [0, 1)

																						// random 2d offset
	float rand_offsx = rand1;
	float rand_offsy = rand2;

	auto& tile = j.block_tiles[id];

	// get a random deterministic variant
	int variant = (int)(rand1 * (float)tile.variants); // [0, tile.variants)

	int texid = tile.calc_tex_index((BlockFace)0, variant);

	auto& info = j.block_meshes_meshes[meshid];

	float posx = (float)x + (rand_offsx * 2 - 1) * 0.25f * info.offs_strength; // [0,1] -> [-1,+1]
	float posy = (float)y + (rand_offsy * 2 - 1) * 0.25f * info.offs_strength; // [0,1] -> [-1,+1]
	float posz = (float)z;

	int16_t fixd_posx = (int16_t)roundi(posx * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posy = (int16_t)roundi(posy * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posz = (int16_t)roundi(posz * BlockMeshInstance::FIXEDPOINT_FAC);

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

__forceinline void face (RemeshChunkJob& j, int x, int y, int z, block_id id, ChunkMeshData* mesh, BlockFace facei) {
	auto* v = mesh->push();
	if (!v) return;

	int16_t fixd_posx = (int16_t)(x * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posy = (int16_t)(y * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posz = (int16_t)(z * BlockMeshInstance::FIXEDPOINT_FAC);

	v->posx = fixd_posx;
	v->posy = fixd_posy;
	v->posz = fixd_posz;
	v->texid = j.block_tiles[id].calc_tex_index(facei, 0);
	v->meshid = facei;
}

template <int AXIS>
void face (RemeshChunkJob& j, int x, int y, int z, block_id id, block_id nid) {
	if (!j.mesh_world_border && (id == B_NULL || nid == B_NULL))
		return;

	auto& b = j.block_types[id];
	auto& nb = j.block_types[nid];

	// generate face of our voxel that faces negative direction neighbour
	if (b.collision != CM_GAS && nb.transparency != TM_OPAQUE && j.block_meshes[id] < 0) {
		auto* mesh = b.transparency == TM_TRANSPARENT ? &j.tranparent_vertices : &j.opaque_vertices;
		face(j, x,y,z, id, mesh, (BlockFace)(BF_NEG_X + AXIS*2));
	}

	// generate face of negative direction neighbour that faces this voxel
	if (nb.collision != CM_GAS && b.transparency != TM_OPAQUE && j.block_meshes[nid] < 0 && nid != B_NULL) {
		auto* mesh = nb.transparency == TM_TRANSPARENT ? &j.tranparent_vertices : &j.opaque_vertices;
		if (AXIS == 0) x -= 1;
		if (AXIS == 1) y -= 1;
		if (AXIS == 2) z -= 1;
		face(j, x,y,z, nid, mesh, (BlockFace)(BF_POS_X + AXIS*2));
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

#include "immintrin.h"

struct _Chunk {
	int			sparse;
	uint32_t	voxel_data;
};
_Chunk get_chunk (Chunk const* chunk) {
	if (chunk == nullptr)
		return { true, (uint32_t)B_NULL };
	
	bool sparse = (chunk->flags & Chunk::SPARSE_VOXELS) != 0;
	return { sparse, chunk->voxel_data };
}

struct _Subchunk {
	int			sparse;
	block_id*	ptr;

	block_id read (uint32_t block_i) {
		return ptr[!sparse ? block_i : 0];
	}
};
_Subchunk get_subchunk (RemeshChunkJob& j, uint32_t subchunk_i, _Chunk& chunk) {
	if (chunk.sparse)
		return { true, (block_id*)&chunk.voxel_data };

	auto& dc = j.dense_chunks[chunk.voxel_data];
	if (dc.is_subchunk_sparse(subchunk_i))
		return { true, (block_id*)&dc.sparse_data[subchunk_i] }; // sparse subchunk

	return { false, j.dense_subchunks[ dc.sparse_data[subchunk_i] ].voxels }; // dense subchunk
}

// Chunk meshing optimized for speed
// works directly with sparse voxel storage
// makes use of sparseness to _very_ quickly mesh chunks that contain mostly sparse subchunks
//  numbers: meshing can be as fast as 32us for mostly sparse chunks compared to 0.5ms-2ms for normal chunks
//    thats a 30x speedup for air and stone chunks! 
//    and even ragular chunks get some speedup due to empty regions, although currently the overhead makes me slightly slower than a dense 3d arary version was if I remember ~.5ms timings correctly

void mesh_chunk (RemeshChunkJob& j) {
	ZoneScoped;

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

		auto sc  = get_subchunk(j, subchunk_i, chunk);
		auto scx = get_subchunk(j, subchunk_i + subc_offs_cx, *subc_chunkx);
		auto scy = get_subchunk(j, subchunk_i + subc_offs_cy, *subc_chunky);
		auto scz = get_subchunk(j, subchunk_i + subc_offs_cz, *subc_chunkz);

		if (sc.sparse) {
			block_id bid = *sc.ptr;

			// X faces
			if (scx.sparse && *scx.ptr == bid) {
				// both subchunks sparse and cannot generate any faces
			} else {
				uint32_t block_i = 0;
				for (    int z=0; z<SUBCHUNK_SIZE; ++z) {
					for (int y=0; y<SUBCHUNK_SIZE; ++y) {
						block_id prev = scx.read(block_i + (SUBCHUNK_SIZE-1)*BX);
						if (bid != prev)
							face<0>(j, 0+sx, y+sy, z+sz, bid, prev);

						block_i += BY;
					}
				}
			}

			// Y faces
			if (scy.sparse && *scy.ptr == bid) {
				// both subchunks sparse and cannot generate any faces
			} else {
				uint32_t block_i = 0;
				for (    int z=0; z<SUBCHUNK_SIZE; ++z) {
					for (int x=0; x<SUBCHUNK_SIZE; ++x) {
						block_id prev = scy.read(block_i + (SUBCHUNK_SIZE-1)*BY);
						if (bid != prev)
							face<1>(j, x+sx, 0+sy, z+sz, bid, prev);

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
				for (    int y=0; y<SUBCHUNK_SIZE; ++y) {
					for (int x=0; x<SUBCHUNK_SIZE; ++x) {
						block_id prev = scz.read(block_i + (SUBCHUNK_SIZE-1)*BZ);
						if (bid != prev)
							face<2>(j, x+sx, y+sy, 0+sz, bid, prev);

						block_i += BX;
					}
				}
			}

			// Block meshes
			auto& bm = j.block_meshes[bid];
			if (bm >= 0) {
				for (int z=0; z<SUBCHUNK_SIZE; ++z)
				for (int y=0; y<SUBCHUNK_SIZE; ++y)
				for (int x=0; x<SUBCHUNK_SIZE; ++x) {
					block_mesh(j, x+sx, y+sy, z+sz, bid, bm);
				}
			}
		} else {
			_dense_subchunks++;

			// Seperate loop version
		#if 0
			// X faces
			uint32_t block_i = 0;
			for (    int z=0; z<SUBCHUNK_SIZE; ++z) {
				for (int y=0; y<SUBCHUNK_SIZE; ++y) {
					block_id prev = scx.read(block_i + (SUBCHUNK_SIZE-1)*BX);

					block_id bid0 = sc.ptr[block_i + 0*BX];
					block_id bid1 = sc.ptr[block_i + 1*BX];
					block_id bid2 = sc.ptr[block_i + 2*BX];
					block_id bid3 = sc.ptr[block_i + 3*BX];

					if (bid0 != prev)	face<0>(j, 0+sx, y+sy, z+sz, bid0, prev);
					if (bid1 != bid0)	face<0>(j, 1+sx, y+sy, z+sz, bid1, bid0);
					if (bid2 != bid1)	face<0>(j, 2+sx, y+sy, z+sz, bid2, bid1);
					if (bid3 != bid2)	face<0>(j, 3+sx, y+sy, z+sz, bid3, bid2);

					block_i += BY;
				}
			}

			// Y faces
			block_i = 0;
			for (    int z=0; z<SUBCHUNK_SIZE; ++z) {
				for (int x=0; x<SUBCHUNK_SIZE; ++x) {
					block_id prev = scy.read(block_i + (SUBCHUNK_SIZE-1)*BY);

					block_id bid0 = sc.ptr[block_i + 0*BY];
					block_id bid1 = sc.ptr[block_i + 1*BY];
					block_id bid2 = sc.ptr[block_i + 2*BY];
					block_id bid3 = sc.ptr[block_i + 3*BY];

					if (bid0 != prev)	face<1>(j, x+sx, 0+sy, z+sz, bid0, prev);
					if (bid1 != bid0)	face<1>(j, x+sx, 1+sy, z+sz, bid1, bid0);
					if (bid2 != bid1)	face<1>(j, x+sx, 2+sy, z+sz, bid2, bid1);
					if (bid3 != bid2)	face<1>(j, x+sx, 3+sy, z+sz, bid3, bid2);

					block_i += BX;
				}
				block_i += BZ - BX*SUBCHUNK_SIZE;
			}

			// Z faces
			block_i = 0;
			for (    int y=0; y<SUBCHUNK_SIZE; ++y) {
				for (int x=0; x<SUBCHUNK_SIZE; ++x) {
					block_id prev = scz.read(block_i + (SUBCHUNK_SIZE-1)*BZ);

					block_id bid0 = sc.ptr[block_i + 0*BZ];
					block_id bid1 = sc.ptr[block_i + 1*BZ];
					block_id bid2 = sc.ptr[block_i + 2*BZ];
					block_id bid3 = sc.ptr[block_i + 3*BZ];

					if (bid0 != prev)	face<2>(j, x+sx, y+sy, 0+sz, bid0, prev);
					if (bid1 != bid0)	face<2>(j, x+sx, y+sy, 1+sz, bid1, bid0);
					if (bid2 != bid1)	face<2>(j, x+sx, y+sy, 2+sz, bid2, bid1);
					if (bid3 != bid2)	face<2>(j, x+sx, y+sy, 3+sz, bid3, bid2);

					block_i += BX;
				}
			}

			// Block meshes
			block_i = 0;
			for (int z=0; z<SUBCHUNK_SIZE; ++z)
			for (int y=0; y<SUBCHUNK_SIZE; ++y)
			for (int x=0; x<SUBCHUNK_SIZE; ++x) {
				block_id bid  = sc.ptr[block_i];

				auto& bm = j.block_meshes[bid];
				if (bm >= 0) block_mesh(j, x+sx, y+sy, z+sz, bid, bm);

				block_i++;
			}
		#endif
			// Straight single loop version
		#if 0
			uint32_t block_i = 0;
			for (int z=0; z<SUBCHUNK_SIZE; ++z)
			for (int y=0; y<SUBCHUNK_SIZE; ++y)
			for (int x=0; x<SUBCHUNK_SIZE; ++x) {
				block_id bid = sc.ptr[block_i];

				block_id prevx = x > 0 ? sc.ptr[block_i - BX] : scx.read(block_i + (SUBCHUNK_SIZE-1)*BX);
				block_id prevy = y > 0 ? sc.ptr[block_i - BY] : scy.read(block_i + (SUBCHUNK_SIZE-1)*BY);
				block_id prevz = z > 0 ? sc.ptr[block_i - BZ] : scz.read(block_i + (SUBCHUNK_SIZE-1)*BZ);

				if (bid != prevx) face<0>(j, x+sx, y+sy, z+sz, bid, prevx);
				if (bid != prevy) face<1>(j, x+sx, y+sy, z+sz, bid, prevy);
				if (bid != prevz) face<2>(j, x+sx, y+sy, z+sz, bid, prevz);

				auto& bm = j.block_meshes[bid];
				if (bm >= 0) block_mesh(j, x+sx, y+sy, z+sz, bid, bm);

				block_i++;
			}

		#endif
			// Fully unrolled for elimination of ternaries in loop version
		#if 0
			uint32_t block_i = 0;
			{ // z == 0
				{ // y == 0
					{ // x == 0
						block_id bid = sc.ptr[block_i];

						block_id prevx = scx.read(block_i + (SUBCHUNK_SIZE-1)*BX);
						block_id prevy = scy.read(block_i + (SUBCHUNK_SIZE-1)*BY);
						block_id prevz = scz.read(block_i + (SUBCHUNK_SIZE-1)*BZ);

						if (bid != prevx) face<0>(j, 0+sx, 0+sy, 0+sz, bid, prevx);
						if (bid != prevy) face<1>(j, 0+sx, 0+sy, 0+sz, bid, prevy);
						if (bid != prevz) face<2>(j, 0+sx, 0+sy, 0+sz, bid, prevz);

						auto& bm = j.block_meshes[bid];
						if (bm >= 0) block_mesh(j, 0+sx, 0+sy, 0+sz, bid, bm);

						block_i++;
					}
					for (int x=1; x<SUBCHUNK_SIZE; ++x) {
						block_id bid = sc.ptr[block_i];

						block_id prevx = sc.ptr[block_i - BX];
						block_id prevy = scy.read(block_i + (SUBCHUNK_SIZE-1)*BY);
						block_id prevz = scz.read(block_i + (SUBCHUNK_SIZE-1)*BZ);

						if (bid != prevx) face<0>(j, x+sx, 0+sy, 0+sz, bid, prevx);
						if (bid != prevy) face<1>(j, x+sx, 0+sy, 0+sz, bid, prevy);
						if (bid != prevz) face<2>(j, x+sx, 0+sy, 0+sz, bid, prevz);

						auto& bm = j.block_meshes[bid];
						if (bm >= 0) block_mesh(j, x+sx, 0+sy, 0+sz, bid, bm);

						block_i++;
					}
				}
				for (int y=1; y<SUBCHUNK_SIZE; ++y) {
					{ // x == 0
						block_id bid = sc.ptr[block_i];

						block_id prevx = scx.read(block_i + (SUBCHUNK_SIZE-1)*BX);
						block_id prevy = sc.ptr[block_i - BY];
						block_id prevz = scz.read(block_i + (SUBCHUNK_SIZE-1)*BZ);

						if (bid != prevx) face<0>(j, 0+sx, y+sy, 0+sz, bid, prevx);
						if (bid != prevy) face<1>(j, 0+sx, y+sy, 0+sz, bid, prevy);
						if (bid != prevz) face<2>(j, 0+sx, y+sy, 0+sz, bid, prevz);

						auto& bm = j.block_meshes[bid];
						if (bm >= 0) block_mesh(j, 0+sx, y+sy, 0+sz, bid, bm);

						block_i++;
					}
					for (int x=1; x<SUBCHUNK_SIZE; ++x) {
						block_id bid = sc.ptr[block_i];

						block_id prevx = sc.ptr[block_i - BX];
						block_id prevy = sc.ptr[block_i - BY];
						block_id prevz = scz.read(block_i + (SUBCHUNK_SIZE-1)*BZ);

						if (bid != prevx) face<0>(j, x+sx, y+sy, 0+sz, bid, prevx);
						if (bid != prevy) face<1>(j, x+sx, y+sy, 0+sz, bid, prevy);
						if (bid != prevz) face<2>(j, x+sx, y+sy, 0+sz, bid, prevz);

						auto& bm = j.block_meshes[bid];
						if (bm >= 0) block_mesh(j, x+sx, y+sy, 0+sz, bid, bm);

						block_i++;
					}
				}
			}
			for (int z=1; z<SUBCHUNK_SIZE; ++z) {
				{ // y == 0
					{ // x == 0
						block_id bid = sc.ptr[block_i];

						block_id prevx = scx.read(block_i + (SUBCHUNK_SIZE-1)*BX);
						block_id prevy = scy.read(block_i + (SUBCHUNK_SIZE-1)*BY);
						block_id prevz = sc.ptr[block_i - BZ];

						if (bid != prevx) face<0>(j, 0+sx, 0+sy, z+sz, bid, prevx);
						if (bid != prevy) face<1>(j, 0+sx, 0+sy, z+sz, bid, prevy);
						if (bid != prevz) face<2>(j, 0+sx, 0+sy, z+sz, bid, prevz);

						auto& bm = j.block_meshes[bid];
						if (bm >= 0) block_mesh(j, 0+sx, 0+sy, z+sz, bid, bm);

						block_i++;
					}
					for (int x=1; x<SUBCHUNK_SIZE; ++x) {
						block_id bid = sc.ptr[block_i];

						block_id prevx = sc.ptr[block_i - BX];
						block_id prevy = scy.read(block_i + (SUBCHUNK_SIZE-1)*BY);
						block_id prevz = sc.ptr[block_i - BZ];

						if (bid != prevx) face<0>(j, x+sx, 0+sy, z+sz, bid, prevx);
						if (bid != prevy) face<1>(j, x+sx, 0+sy, z+sz, bid, prevy);
						if (bid != prevz) face<2>(j, x+sx, 0+sy, z+sz, bid, prevz);

						auto& bm = j.block_meshes[bid];
						if (bm >= 0) block_mesh(j, x+sx, 0+sy, z+sz, bid, bm);

						block_i++;
					}
				}
				for (int y=1; y<SUBCHUNK_SIZE; ++y) {
					{ // x == 0
						block_id bid = sc.ptr[block_i];

						block_id prevx = scx.read(block_i + (SUBCHUNK_SIZE-1)*BX);
						block_id prevy = sc.ptr[block_i - BY];
						block_id prevz = sc.ptr[block_i - BZ];

						if (bid != prevx) face<0>(j, 0+sx, y+sy, z+sz, bid, prevx);
						if (bid != prevy) face<1>(j, 0+sx, y+sy, z+sz, bid, prevy);
						if (bid != prevz) face<2>(j, 0+sx, y+sy, z+sz, bid, prevz);

						auto& bm = j.block_meshes[bid];
						if (bm >= 0) block_mesh(j, 0+sx, y+sy, z+sz, bid, bm);

						block_i++;
					}
					for (int x=1; x<SUBCHUNK_SIZE; ++x) {
						block_id bid = sc.ptr[block_i];

						block_id prevx = sc.ptr[block_i - BX];
						block_id prevy = sc.ptr[block_i - BY];
						block_id prevz = sc.ptr[block_i - BZ];

						if (bid != prevx) face<0>(j, x+sx, y+sy, z+sz, bid, prevx);
						if (bid != prevy) face<1>(j, x+sx, y+sy, z+sz, bid, prevy);
						if (bid != prevz) face<2>(j, x+sx, y+sy, z+sz, bid, prevz);

						auto& bm = j.block_meshes[bid];
						if (bm >= 0) block_mesh(j, x+sx, y+sy, z+sz, bid, bm);

						block_i++;
					}
				}
			}
		#endif

		#if 1
			uint32_t block_i = 0;
			for (int z=0; z<SUBCHUNK_SIZE; ++z)
			for (int y=0; y<SUBCHUNK_SIZE; ++y) {

				block_id prevx = scx.read(block_i + (SUBCHUNK_SIZE-1)*BX);

				block_id bid0 = sc.ptr[block_i+0];
				block_id bid1 = sc.ptr[block_i+1];
				block_id bid2 = sc.ptr[block_i+2];
				block_id bid3 = sc.ptr[block_i+3];

				block_id prevy0 = y > 0 ? sc.ptr[block_i+0 - BY] : scy.read(block_i+0 + (SUBCHUNK_SIZE-1)*BY);
				block_id prevy1 = y > 0 ? sc.ptr[block_i+1 - BY] : scy.read(block_i+1 + (SUBCHUNK_SIZE-1)*BY);
				block_id prevy2 = y > 0 ? sc.ptr[block_i+2 - BY] : scy.read(block_i+2 + (SUBCHUNK_SIZE-1)*BY);
				block_id prevy3 = y > 0 ? sc.ptr[block_i+3 - BY] : scy.read(block_i+3 + (SUBCHUNK_SIZE-1)*BY);

				block_id prevz0 = z > 0 ? sc.ptr[block_i+0 - BZ] : scz.read(block_i+0 + (SUBCHUNK_SIZE-1)*BZ);
				block_id prevz1 = z > 0 ? sc.ptr[block_i+1 - BZ] : scz.read(block_i+1 + (SUBCHUNK_SIZE-1)*BZ);
				block_id prevz2 = z > 0 ? sc.ptr[block_i+2 - BZ] : scz.read(block_i+2 + (SUBCHUNK_SIZE-1)*BZ);
				block_id prevz3 = z > 0 ? sc.ptr[block_i+3 - BZ] : scz.read(block_i+3 + (SUBCHUNK_SIZE-1)*BZ);

				if (bid0 != prevx ) face<0>(j, 0+sx, y+sy, z+sz, bid0, prevx );
				if (bid0 != prevy0) face<1>(j, 0+sx, y+sy, z+sz, bid0, prevy0);
				if (bid0 != prevz0) face<2>(j, 0+sx, y+sy, z+sz, bid0, prevz0);
				if (j.block_meshes[bid0] >= 0) block_mesh(j, 0+sx, y+sy, z+sz, bid0, j.block_meshes[bid0]);

				if (bid1 != bid0  ) face<0>(j, 1+sx, y+sy, z+sz, bid1, bid0  );
				if (bid1 != prevy1) face<1>(j, 1+sx, y+sy, z+sz, bid1, prevy1);
				if (bid1 != prevz1) face<2>(j, 1+sx, y+sy, z+sz, bid1, prevz1);
				if (j.block_meshes[bid1] >= 0) block_mesh(j, 1+sx, y+sy, z+sz, bid1, j.block_meshes[bid1]);

				if (bid2 != bid1  ) face<0>(j, 2+sx, y+sy, z+sz, bid2, bid1  );
				if (bid2 != prevy2) face<1>(j, 2+sx, y+sy, z+sz, bid2, prevy2);
				if (bid2 != prevz2) face<2>(j, 2+sx, y+sy, z+sz, bid2, prevz2);
				if (j.block_meshes[bid2] >= 0) block_mesh(j, 2+sx, y+sy, z+sz, bid2, j.block_meshes[bid2]);

				if (bid3 != bid2  ) face<0>(j, 3+sx, y+sy, z+sz, bid3, bid2  );
				if (bid3 != prevy3) face<1>(j, 3+sx, y+sy, z+sz, bid3, prevy3);
				if (bid3 != prevz3) face<2>(j, 3+sx, y+sy, z+sz, bid3, prevz3);
				if (j.block_meshes[bid3] >= 0) block_mesh(j, 3+sx, y+sy, z+sz, bid3, j.block_meshes[bid3]);

				block_i += 4;
			}
		#endif

		}

		subchunk_i++;
	}
	}
	}

	ZoneValue(_dense_subchunks);
}

void RemeshChunkJob::execute () {
	mesh_chunk(*this);
}

Chunk const* get_neighbour_blocks (Chunks& chunks, Chunk* chunk, int neighbour) {
	int3 pos = chunk->pos;
	pos[neighbour] -= 1;

	//auto nid = j.chunks->chunks_arr.checked_get(pos.x, pos.y, pos.z);
	auto nid = chunks.query_chunk(pos);
	if (nid != U16_NULL && chunks[nid].flags != 0) {
		return &chunks[nid];
	}
	return nullptr;
}

RemeshChunkJob::RemeshChunkJob (Chunks& chunks, Chunk* chunk, WorldGenerator const& wg, bool mesh_world_border) {
	block_types			= g_assets.block_types.blocks.data();
	block_meshes		= g_assets.block_meshes.block_meshes.data();
	block_meshes_meshes	= g_assets.block_meshes.meshes.data();
	block_tiles			= g_assets.block_tiles.data();

	this->dense_chunks		= &chunks.dense_chunks[0];
	this->dense_subchunks	= &chunks.dense_subchunks[0];

	this->chunk = chunk;
	this->chunk_nx = get_neighbour_blocks(chunks, chunk, 0);
	this->chunk_ny = get_neighbour_blocks(chunks, chunk, 1);
	this->chunk_nz = get_neighbour_blocks(chunks, chunk, 2);

	this->mesh_world_border = mesh_world_border;
	chunk_seed = wg.seed ^ hash(chunk->pos * CHUNK_SIZE);
}
