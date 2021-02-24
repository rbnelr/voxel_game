#include "common.hpp"
#include "chunk_mesher.hpp"
#include "world_generator.hpp"
#include "assets.hpp"
#include "player.hpp"

#define NOINLINE __declspec(noinline)

int3 pos_from_idx (int idx) {
	int3 pos;
	pos.x =  idx & CHUNK_SIZE_MASK;
	pos.y = (idx >> CHUNK_SIZE_SHIFT  ) & CHUNK_SIZE_MASK;
	pos.z = (idx >> CHUNK_SIZE_SHIFT*2) & CHUNK_SIZE_MASK;
	return pos;
}

NOINLINE void block_mesh (RemeshChunkJob& j, int3 const& pos, block_id id, int meshid) {
	// get a 'random' but deterministic value based on block position
	uint64_t h = hash(pos) ^ j.chunk_seed;
	
	float rand1 = (float)( h        & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // uniform in [0, 1)
	float rand2 = (float)((h >> 32) & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // uniform in [0, 1)

	// random 2d offset
	float rand_offsx = rand1;
	float rand_offsy = rand2;
		
	auto& tile = g_assets.block_tiles[id];

	// get a random deterministic variant
	int variant = (int)(rand1 * (float)tile.variants); // [0, tile.variants)
		
	int texid = tile.calc_tex_index((BlockFace)0, variant);

	auto& info = g_assets.block_meshes.meshes[meshid];

	float posx = (float)pos.x + (rand_offsx * 2 - 1) * 0.25f * info.offs_strength; // [0,1] -> [-1,+1]
	float posy = (float)pos.y + (rand_offsy * 2 - 1) * 0.25f * info.offs_strength; // [0,1] -> [-1,+1]
	float posz = (float)pos.z;

	int16_t fixd_posx = (int16_t)roundi(posx * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posy = (int16_t)roundi(posy * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posz = (int16_t)roundi(posz * BlockMeshInstance::FIXEDPOINT_FAC);

	for (int meshid=info.index; meshid < info.index + info.length; ++meshid) {
		auto* v = j.mesh.opaque_vertices.push();
		if (!v) return;
		v->posx = fixd_posx;
		v->posy = fixd_posy;
		v->posz = fixd_posz;
		v->texid = texid;
		v->meshid = meshid;
	}
}

void face (RemeshChunkJob& j, int3 const& pos, block_id id, ChunkMeshData& mesh, BlockFace facei) {
	auto* v = mesh.push();
	if (!v) return;

	int16_t fixd_posx = (int16_t)(pos.x * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posy = (int16_t)(pos.y * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posz = (int16_t)(pos.z * BlockMeshInstance::FIXEDPOINT_FAC);

	v->posx = fixd_posx;
	v->posy = fixd_posy;
	v->posz = fixd_posz;
	v->texid = g_assets.block_tiles[id].calc_tex_index(facei, 0);
	v->meshid = facei;
}

template <int AXIS>
NOINLINE void face (RemeshChunkJob& j, int3 const& pos, block_id id, block_id nid) {
	if (!j.mesh_world_border && (id == B_NULL || nid == B_NULL))
		return;

	auto& b = g_assets.block_types[id];
	auto& nb = g_assets.block_types[nid];

	// generate face of our voxel that faces negative direction neighbour
	if (b.collision != CM_GAS && nb.transparency != TM_OPAQUE && g_assets.block_meshes.block_meshes[id] < 0) {
		auto& mesh = b.transparency == TM_TRANSPARENT ? j.mesh.tranparent_vertices : j.mesh.opaque_vertices;
		face(j, pos, id, mesh, (BlockFace)(BF_NEG_X + AXIS*2));
	}

	// generate face of negative direction neighbour that faces this voxel
	if (nb.collision != CM_GAS && b.transparency != TM_OPAQUE && g_assets.block_meshes.block_meshes[nid] < 0 && nid != B_NULL) {
		auto& mesh = nb.transparency == TM_TRANSPARENT ? j.mesh.tranparent_vertices : j.mesh.opaque_vertices;
		int3 npos = pos;
		npos[AXIS] -= 1;
		face(j, npos, nid, mesh, (BlockFace)(BF_POS_X + AXIS*2));
	}
}

Chunk const* get_neighbour_blocks (RemeshChunkJob& j, int neighbour) {
	int3 pos = j.chunk->pos;
	pos[neighbour] -= 1;

	//auto nid = j.chunks->chunks_arr.checked_get(pos.x, pos.y, pos.z);
	auto nid = j.chunks->query_chunk(pos);
	if (nid != U16_NULL && (*j.chunks)[nid].flags != 0) {
		return &(*j.chunks)[nid];
	}
	return nullptr;
}

// offsets of subchunk 3d neighbours in flat buffer
#define SCX  1
#define SCY  SUBCHUNK_COUNT
#define SCZ (SUBCHUNK_COUNT * SUBCHUNK_COUNT)

// offsets of block 3d neighbours in flat buffer
#define BX  1
#define BY  SUBCHUNK_SIZE
#define BZ (SUBCHUNK_SIZE * SUBCHUNK_SIZE)

#if 0
block_id read_block (RemeshChunkJob& j, uint32_t subchunk_i, uint32_t block_i, Chunk const* chunk) {
	if (!chunk)
		return B_NULL;

	if (chunk->flags & Chunk::SPARSE_VOXELS)
		return (block_id)chunk->voxel_data; // sparse chunk

	auto& dc = j.chunks->dense_chunks[chunk->voxel_data];
	if (dc.is_subchunk_sparse(subchunk_i))
		return (block_id)dc.sparse_data[subchunk_i]; // sparse subchunk

	return j.chunks->dense_subchunks[ dc.sparse_data[subchunk_i] ].voxels[block_i];
}


void mesh_chunk (RemeshChunkJob& job) {
	ZoneScoped;

	{ // X
		ZoneScopedN("X");

		auto const* nc_nx = get_neighbour_blocks(job, 1);

		int3 pos;
		for (int sz=0; sz<CHUNK_SIZE; sz += SUBCHUNK_SIZE)
		for (int sy=0; sy<CHUNK_SIZE; sy += SUBCHUNK_SIZE) {
			uint32_t subchunk_i = SUBCHUNK_IDX(0, sy, sz);

			for (pos.z=sz; pos.z < sz+SUBCHUNK_SIZE; pos.z++)
			for (pos.y=sy; pos.y < sy+SUBCHUNK_SIZE; pos.y++) {
				uint32_t block_i = BLOCK_IDX(0, pos.y, pos.z);
				pos.x = 0;

				block_id nid = read_block(job, subchunk_i + (SUBCHUNK_COUNT-1)*SCX, block_i + (SUBCHUNK_SIZE-1)*BX, nc_nx);
			
				if (job.chunk->flags & Chunk::SPARSE_VOXELS) {
					block_id id = (block_id)job.chunk->voxel_data;

					if (nid != id)
						face<0>(job, pos, id, nid);

				} else {
					auto& dc = job.chunks->dense_chunks[job.chunk->voxel_data];

					for (uint32_t j=0; j<SUBCHUNK_COUNT; j++) {
						uint32_t subi = subchunk_i + j*SCX;
						if (dc.is_subchunk_sparse(subi)) {
							block_id id = (block_id)dc.sparse_data[subi];

							if (nid != id)
								face<0>(job, pos, id, nid);
							nid = id;

							pos.x += SUBCHUNK_SIZE;
						} else {
							auto* pid = &job.chunks->dense_subchunks[ dc.sparse_data[subi] ].voxels[block_i];

							for (uint32_t i=0; i<SUBCHUNK_SIZE; i++) {
								block_id id = pid[i*BX];

								if (nid != id)
									face<0>(job, pos, id, nid);
								nid = id;

								pos.x++;
							}
						}
					}
				}
			}
		}
	}

	{ // Y
		ZoneScopedN("Y");

		auto const* nc_ny = get_neighbour_blocks(job, 1);
		
		int3 pos;
		for (int sz=0; sz<CHUNK_SIZE; sz += SUBCHUNK_SIZE)
		for (int sx=0; sx<CHUNK_SIZE; sx += SUBCHUNK_SIZE) {
			uint32_t subchunk_i = SUBCHUNK_IDX(sx, 0, sz);

			for (pos.z=sz; pos.z < sz+SUBCHUNK_SIZE; pos.z++)
			for (pos.x=sx; pos.x < sx+SUBCHUNK_SIZE; pos.x++) {
				uint32_t block_i = BLOCK_IDX(pos.x, 0, pos.z);
				pos.y = 0;

				block_id nid = read_block(job, subchunk_i + (SUBCHUNK_COUNT-1)*SCY, block_i + (SUBCHUNK_SIZE-1)*BY, nc_ny);

				if (job.chunk->flags & Chunk::SPARSE_VOXELS) {
					block_id id = (block_id)job.chunk->voxel_data;

					if (nid != id)
						face<1>(job, pos, id, nid);

				} else {
					auto& dc = job.chunks->dense_chunks[job.chunk->voxel_data];

					for (uint32_t j=0; j<SUBCHUNK_COUNT; j++) {
						uint32_t subi = subchunk_i + j*SCY;
						if (dc.is_subchunk_sparse(subi)) {
							block_id id = (block_id)dc.sparse_data[subi];

							if (nid != id)
								face<1>(job, pos, id, nid);
							nid = id;

							pos.y += SUBCHUNK_SIZE;
						} else {
							auto* pid = &job.chunks->dense_subchunks[ dc.sparse_data[subi] ].voxels[block_i];

							for (uint32_t i=0; i<SUBCHUNK_SIZE; i++) {
								block_id id = pid[i*BY];

								if (nid != id)
									face<1>(job, pos, id, nid);
								nid = id;

								pos.y++;
							}
						}
					}
				}
			}
		}
	}

	{ // Z
		ZoneScopedN("Z");

		auto const* nc_nz = get_neighbour_blocks(job, 2);

		int3 pos;
		for (int sy=0; sy<CHUNK_SIZE; sy += SUBCHUNK_SIZE)
		for (int sx=0; sx<CHUNK_SIZE; sx += SUBCHUNK_SIZE) {
			uint32_t subchunk_i = SUBCHUNK_IDX(sx, sy, 0);
	
			for (pos.y=sy; pos.y < sy+SUBCHUNK_SIZE; pos.y++)
			for (pos.x=sx; pos.x < sx+SUBCHUNK_SIZE; pos.x++) {
				uint32_t block_i = BLOCK_IDX(pos.x, pos.y, 0);
				pos.z = 0;
						
				block_id nid = read_block(job, subchunk_i + (SUBCHUNK_COUNT-1)*SCZ, block_i + (SUBCHUNK_SIZE-1)*BZ, nc_nz);
	
				if (job.chunk->flags & Chunk::SPARSE_VOXELS) {
					block_id id = (block_id)job.chunk->voxel_data;
	
					if (nid != id)
						face<2>(job, pos, id, nid);

					if (g_assets.block_meshes.block_meshes[id] >= 0) {
						for (; pos.z < CHUNK_SIZE; pos.z++)
							block_mesh(job, pos, id, g_assets.block_meshes.block_meshes[id]);
					}
	
				} else {
					auto& dc = job.chunks->dense_chunks[job.chunk->voxel_data];
	
					for (uint32_t j=0; j<SUBCHUNK_COUNT; j++) {
						uint32_t subi = subchunk_i + j*SCZ;
						if (dc.is_subchunk_sparse(subi)) {
							block_id id = (block_id)dc.sparse_data[subi];
	
							if (nid != id)
								face<2>(job, pos, id, nid);

							if (g_assets.block_meshes.block_meshes[id] >= 0) {
								int end = pos.z + SUBCHUNK_SIZE;
								for (; pos.z < end; pos.z++)
									block_mesh(job, pos, id, g_assets.block_meshes.block_meshes[id]);
							}
	
							nid = id;

						} else {
							auto* pid = &job.chunks->dense_subchunks[ dc.sparse_data[subi] ].voxels[block_i];
	
							for (uint32_t i=0; i<SUBCHUNK_SIZE; i++) {
								block_id id = pid[i*BZ];
	
								if (nid != id)
									face<2>(job, pos, id, nid);
	
								if (g_assets.block_meshes.block_meshes[id] >= 0)
									block_mesh(job, pos, id, g_assets.block_meshes.block_meshes[id]);
	
								nid = id;
	
								pos.z++;
							}
						}
					}
				}
			}
		}
	}
}
#endif

#if 1
block_id* read_subchunk (RemeshChunkJob& j, uint32_t subchunk_i, Chunk const* chunk, block_id& sparse) {
	if (!chunk) {
		sparse = B_NULL;
		return nullptr;
	}

	if (chunk->flags & Chunk::SPARSE_VOXELS) {
		sparse = (block_id)chunk->voxel_data;
		return nullptr; // sparse chunk
	}

	auto& dc = j.chunks->dense_chunks[chunk->voxel_data];
	if (dc.is_subchunk_sparse(subchunk_i)) {
		sparse = (block_id)dc.sparse_data[subchunk_i];
		return nullptr; // sparse subchunk
	}

	return j.chunks->dense_subchunks[ dc.sparse_data[subchunk_i] ].voxels; // dense subchunk
}

void mesh_chunk (RemeshChunkJob& j) {
	ZoneScoped;

	// neighbour chunks (can be null)
	auto const* cx = get_neighbour_blocks(j, 0);
	auto const* cy = get_neighbour_blocks(j, 1);
	auto const* cz = get_neighbour_blocks(j, 2);

	//uint32_t subchunk_i = 0;
	for (int sz = 0; sz < CHUNK_SIZE; sz += SUBCHUNK_SIZE)
	for (int sy = 0; sy < CHUNK_SIZE; sy += SUBCHUNK_SIZE)
	for (int sx = 0; sx < CHUNK_SIZE; sx += SUBCHUNK_SIZE) {
		uint32_t subchunk_i = SUBCHUNK_IDX(sx,sy,sz);

		block_id scb;
		auto sc  = read_subchunk(j, subchunk_i, j.chunk, scb);

		block_id scbz;
		auto scz = read_subchunk(j, sz > 0 ? subchunk_i-SCZ : subchunk_i + SCZ*(SUBCHUNK_COUNT-1), sz > 0 ? j.chunk : cz, scbz);
		block_id scby;
		auto scy = read_subchunk(j, sy > 0 ? subchunk_i-SCY : subchunk_i + SCY*(SUBCHUNK_COUNT-1), sy > 0 ? j.chunk : cy, scby);
		block_id scbx;
		auto scx = read_subchunk(j, sx > 0 ? subchunk_i-SCX : subchunk_i + SCX*(SUBCHUNK_COUNT-1), sx > 0 ? j.chunk : cx, scbx);

		uint32_t block_i = 0;

		for (int z = sz; z < sz + SUBCHUNK_SIZE; ++z)
		for (int y = sy; y < sy + SUBCHUNK_SIZE; ++y)
		for (int x = sx; x < sx + SUBCHUNK_SIZE; ++x) {

			block_id b  = sc ? sc[block_i] : scb;

			block_id bz = (z & SUBCHUNK_MASK) != 0 ? (sc ? sc[block_i-BZ] : scb) : (scz ? scz[block_i + BZ*(SUBCHUNK_SIZE-1)] : scbz);
			block_id by = (y & SUBCHUNK_MASK) != 0 ? (sc ? sc[block_i-BY] : scb) : (scy ? scy[block_i + BY*(SUBCHUNK_SIZE-1)] : scby);
			block_id bx = (x & SUBCHUNK_MASK) != 0 ? (sc ? sc[block_i-BX] : scb) : (scx ? scx[block_i + BX*(SUBCHUNK_SIZE-1)] : scbx);
			
			int3 pos;
			pos.z = z;
			pos.y = y;
			pos.x = x;

			if (b != bx)	face<0>(j, pos, b, bx);
			if (b != by)	face<1>(j, pos, b, by);
			if (b != bz)	face<2>(j, pos, b, bz);

			auto& bm = g_assets.block_meshes.block_meshes[b];
			if (bm >= 0)	block_mesh(j, pos, b, bm);

			block_i++;
		}

		//subchunk_i++;
	}
}
#endif

RemeshChunkJob::RemeshChunkJob (Chunk* chunk, Chunks* chunks, WorldGenerator const& wg, bool mesh_world_border):
		chunk{chunk}, chunks{chunks},
		mesh_world_border{mesh_world_border} {

	chunk_seed = wg.seed ^ hash(chunk->pos * CHUNK_SIZE);
}

void RemeshChunkJob::execute () {
	mesh_chunk(*this);
}
