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

block_id* read_subchunk (RemeshChunkJob& j, uint32_t subchunk_i, Chunk const* chunk, block_id& sparse) {
	if (!chunk) {
		sparse = B_NULL;
		return nullptr;
	}

	if (chunk->flags & Chunk::SPARSE_VOXELS) {
		sparse = (block_id)chunk->voxel_data;
		return nullptr; // sparse chunk
	}

	auto& dc = j.dense_chunks[chunk->voxel_data];
	if (dc.is_subchunk_sparse(subchunk_i)) {
		sparse = (block_id)dc.sparse_data[subchunk_i];
		return nullptr; // sparse subchunk
	}

	return j.dense_subchunks[ dc.sparse_data[subchunk_i] ].voxels; // dense subchunk
}

struct Subc {
	block_id* ptr;
	block_id sparse;

	block_id read (uint32_t idx) {
		return ptr ? ptr[idx] : sparse;
	}
};

void mesh_chunk (RemeshChunkJob& j) {
	ZoneScoped;

	uint32_t subchunk_i = 0;
	for (int sz = 0; sz < CHUNK_SIZE; sz += SUBCHUNK_SIZE)
	for (int sy = 0; sy < CHUNK_SIZE; sy += SUBCHUNK_SIZE)
	for (int sx = 0; sx < CHUNK_SIZE; sx += SUBCHUNK_SIZE) {

		Subc sc, scx, scy, scz;
		sc .ptr = read_subchunk(j, subchunk_i, j.chunk, sc.sparse);
		scx.ptr = read_subchunk(j, sx > 0 ? subchunk_i-SCX : subchunk_i + SCX*(SUBCHUNK_COUNT-1), sx > 0 ? j.chunk : j.chunk_nx, scx.sparse);
		scy.ptr = read_subchunk(j, sy > 0 ? subchunk_i-SCY : subchunk_i + SCY*(SUBCHUNK_COUNT-1), sy > 0 ? j.chunk : j.chunk_ny, scy.sparse);
		scz.ptr = read_subchunk(j, sz > 0 ? subchunk_i-SCZ : subchunk_i + SCZ*(SUBCHUNK_COUNT-1), sz > 0 ? j.chunk : j.chunk_nz, scz.sparse);

		if (!sc.ptr) {
			// X faces
			if (!scx.ptr && scx.sparse == sc.sparse) {
				// both subchunks sparse and cannot generate any faces
			} else {
				uint32_t block_i = 0;
				for (    int z=0; z<SUBCHUNK_SIZE; ++z) {
					for (int y=0; y<SUBCHUNK_SIZE; ++y) {
						block_id prev = scx.read(block_i + (SUBCHUNK_SIZE-1)*BX);
						block_id bid = sc.sparse;
						if (bid != prev)
							face<0>(j, 0+sx, y+sy, z+sz, bid, prev);

						block_i += BY;
					}
				}
			}

			// Y faces
			if (!scy.ptr && scy.sparse == sc.sparse) {
				// both subchunks sparse and cannot generate any faces
			} else {
				uint32_t block_i = 0;
				for (    int z=0; z<SUBCHUNK_SIZE; ++z) {
					for (int x=0; x<SUBCHUNK_SIZE; ++x) {
						block_id prev = scy.read(block_i + (SUBCHUNK_SIZE-1)*BY);
						block_id bid = sc.sparse;

						if (bid != prev)
							face<1>(j, x+sx, 0+sy, z+sz, bid, prev);

						block_i += BX;
					}
					block_i += BZ - BX*SUBCHUNK_SIZE;
				}
			}

			// Z faces
			if (!scz.ptr && scz.sparse == sc.sparse) {
				// both subchunks sparse and cannot generate any faces
			} else {
				uint32_t block_i = 0;
				for (    int y=0; y<SUBCHUNK_SIZE; ++y) {
					for (int x=0; x<SUBCHUNK_SIZE; ++x) {
						block_id prev = scz.read(block_i + (SUBCHUNK_SIZE-1)*BZ);
						block_id bid = sc.sparse;

						if (bid != prev)
							face<2>(j, x+sx, y+sy, 0+sz, bid, prev);

						block_i += BX;
					}
				}
			}

			// Block meshes
			block_id b = sc.sparse;
			auto& bm = j.block_meshes[b];
			if (bm >= 0) {
				for (int z=0; z<SUBCHUNK_SIZE; ++z)
				for (int y=0; y<SUBCHUNK_SIZE; ++y)
				for (int x=0; x<SUBCHUNK_SIZE; ++x) {
					block_mesh(j, x+sx, y+sy, z+sz, b, bm);
				}
			}
		} else {
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
				block_id b  = sc.ptr[block_i];

				auto& bm = j.block_meshes[b];
				if (bm >= 0) {
					block_mesh(j, x+sx, y+sy, z+sz, b, bm);
				}

				block_i++;
			}
		}

		subchunk_i++;
	}
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
