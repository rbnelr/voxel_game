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

NOINLINE void block_mesh (RemeshChunkJob& j, block_id id, int meshid, int idx) {
	int3 pos = pos_from_idx(idx);

	// get a 'random' but deterministic value based on block position
	uint64_t h = hash(pos) ^ j.chunk_seed;
	
	// get a random determinisitc 2d offset
	float rand1 = (float)( h        & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // [0, 1)
	float rand2 = (float)((h >> 32) & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // [0, 1)
		
	float rand_offsx = rand1;
	float rand_offsy = rand2;
		
	auto& tile = g_assets.block_tiles[id];

	// get a random deterministic variant
	int variant = (int)(rand1 * (float)tile.variants); // [0, tile.variants)
		
	int texid = tile.calc_tex_index((BlockFace)0, variant);

	float posx = (float)pos.x + (rand_offsx * 2 - 1) * 0.25f; // [0,1] -> [-1,+1]
	float posy = (float)pos.y + (rand_offsy * 2 - 1) * 0.25f; // [0,1] -> [-1,+1]
	float posz = (float)pos.z;

	int16_t fixd_posx = (int16_t)roundi(posx * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posy = (int16_t)roundi(posy * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posz = (int16_t)roundi(posz * BlockMeshInstance::FIXEDPOINT_FAC);

	auto& info = g_assets.block_meshes.meshes[meshid];

	for (int meshid=info.offset; meshid < info.offset + info.length; ++meshid) {
		auto* v = j.mesh.opaque_vertices.push();
		if (!v) return;
		v->posx = fixd_posx;
		v->posy = fixd_posy;
		v->posz = fixd_posz;
		v->texid = texid;
		v->meshid = meshid;
	}
}

void face (RemeshChunkJob& j, block_id id, ChunkMeshData& mesh, BlockFace facei, int3 pos) {
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
NOINLINE void face (RemeshChunkJob& j, block_id id, block_id nid, int idx) {
	int3 pos = pos_from_idx(idx);
	
	if (!j.mesh_world_border && (id == B_NULL || nid == B_NULL))
		return;

	auto& b = g_assets.block_types[id];
	auto& nb = g_assets.block_types[nid];

	// generate face of our voxel that faces negative direction neighbour
	if (b.collision != CM_GAS && nb.transparency != TM_OPAQUE && g_assets.block_meshes.block_meshes[id] < 0) {
		auto& mesh = b.transparency == TM_TRANSPARENT ? j.mesh.tranparent_vertices : j.mesh.opaque_vertices;
		face(j, id, mesh, (BlockFace)(BF_NEG_X + AXIS*2), pos);
	}

	// generate face of negative direction neighbour that faces this voxel
	if (nb.collision != CM_GAS && b.transparency != TM_OPAQUE && g_assets.block_meshes.block_meshes[nid] < 0 && nid != B_NULL) {
		auto& mesh = nb.transparency == TM_TRANSPARENT ? j.mesh.tranparent_vertices : j.mesh.opaque_vertices;
		pos[AXIS] -= 1;
		face(j, nid, mesh, (BlockFace)(BF_POS_X + AXIS*2), pos);
	}
}

#if 0 // 2-Level sparse solution
block_id const* get_neighbour_blocks (RemeshChunkJob& j, int neighbour, block_id* sparse_id) {
	*sparse_id = B_NULL;

	auto nid = j.chunk->neighbours[neighbour];
	if (nid != U16_NULL) {

		auto& nc = j.chunks[nid];
		if (nc.flags & Chunk::LOADED) {

			if (!nc.voxels.is_sparse()) {
				return nc.voxels.ids;
			}
			*sparse_id = nc.voxels.sparse_id;
		}
	}
	return nullptr;
}

void mesh_chunk (RemeshChunkJob& j) {
	ZoneScoped;

	// neighbour chunks (can be null)
	block_id sid_nx, sid_ny, sid_nz; 
	auto const* nc_nx = get_neighbour_blocks(j, 0, &sid_nx);
	auto const* nc_ny = get_neighbour_blocks(j, 2, &sid_ny);
	auto const* nc_nz = get_neighbour_blocks(j, 4, &sid_nz);

	auto const* ptr = j.chunk->voxels.ids;

#define XOFFS 1
#define YOFFS CHUNK_ROW_OFFS
#define ZOFFS CHUNK_LAYER_OFFS

#if 0 //// Slow, general solution
	int idx = 0;

	// fast meshing without neighbour chunk access
	for (int z=0; z<CHUNK_SIZE; ++z) {
		for (int y=0; y<CHUNK_SIZE; ++y) {
			for (int x=0; x<CHUNK_SIZE; ++x) {

				block_id id = ptr[idx];

				{ // X
					block_id nid = x > 0 ? ptr[idx - XOFFS] :
						(nc_nx ? nc_nx[idx + (CHUNK_SIZE-1)*XOFFS] : sid_nx);
					if (nid != id)
						face<0>(j, id, nid, idx);
				}
				{ // Y
					block_id nid = y > 0 ? ptr[idx - YOFFS] :
						(nc_ny ? nc_ny[idx + (CHUNK_SIZE-1)*YOFFS] : sid_ny);
					if (nid != id)
						face<1>(j, id, nid, idx);
				}
				{ // Z
					block_id nid = z > 0 ? ptr[idx - ZOFFS] :
						(nc_nz ? nc_nz[idx + (CHUNK_SIZE-1)*ZOFFS] : sid_nz);
					if (nid != id)
						face<2>(j, id, nid, idx);
				}

				if (g_assets.block_meshes.block_meshes[id] >= 0)
					block_mesh(j, id, g_assets.block_meshes.block_meshes[id], idx);

				idx++;
			}
		}
	}
#else
	//// Manually unwrapped version where all x==0 y==0 z==0 are own code path, to avoid expensive if in core loop
	// while still keeping flexible code in border voxels to allows fast code
	// Code is super bloated and macro'd, but generates code that runs almost as fast as code without any neighbour handling
	
	#define BODY(X,Y,Z) {													\
		block_id id = ptr[idx];												\
		{																	\
			block_id nid = X;												\
			if (nid != id) face<0>(j, id, nid, idx);						\
		}																	\
		{																	\
			block_id nid = Y;												\
			if (nid != id) face<1>(j, id, nid, idx);						\
		}																	\
		{																	\
			block_id nid = Z;												\
			if (nid != id) face<2>(j, id, nid, idx);						\
		}																	\
		if (g_assets.block_meshes.block_meshes[id] >= 0)					\
			block_mesh(j, id, g_assets.block_meshes.block_meshes[id], idx);	\
																			\
		idx++;																\
	}

#define NEIGHBOURX (nc_nx ? nc_nx[idx + (CHUNK_SIZE-1)*XOFFS] : sid_nx)
#define NEIGHBOURY (nc_ny ? nc_ny[idx + (CHUNK_SIZE-1)*YOFFS] : sid_ny)
#define NEIGHBOURZ (nc_nz ? nc_nz[idx + (CHUNK_SIZE-1)*ZOFFS] : sid_nz)

#define OFFSX ptr[idx - XOFFS]
#define OFFSY ptr[idx - YOFFS]
#define OFFSZ ptr[idx - ZOFFS]
	
	int idx = 0;

	{ // z == 0
		{ // y == 0
			{ // x == 0
				BODY(NEIGHBOURX, NEIGHBOURY, NEIGHBOURZ)
			}
			for (int x=1; x<CHUNK_SIZE; ++x) { // xyz != 0
				BODY(OFFSX, NEIGHBOURY, NEIGHBOURZ )
			}
		}

		for (int y=1; y<CHUNK_SIZE; ++y) {
			{ // x == 0
				BODY(NEIGHBOURX, OFFSY, NEIGHBOURZ)
			}
			for (int x=1; x<CHUNK_SIZE; ++x) { // xyz != 0
				BODY(OFFSX, OFFSY, NEIGHBOURZ)
			}
		}
	}

	for (int z=1; z<CHUNK_SIZE; ++z) {
		{ // y == 0
			{ // x == 0
				BODY(NEIGHBOURX, NEIGHBOURY, OFFSZ)
			}
			for (int x=1; x<CHUNK_SIZE; ++x) { // xyz != 0
				BODY(OFFSX, NEIGHBOURY, OFFSZ)
			}
		}

		for (int y=1; y<CHUNK_SIZE; ++y) {
			{ // x == 0
				BODY(NEIGHBOURX, OFFSY, OFFSZ)
			}
			for (int x=1; x<CHUNK_SIZE; ++x) { // core loop; xyz != 0
				BODY(OFFSX, OFFSY, OFFSZ)
			}
		}
	}
#endif
}
#endif

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

void mesh_chunk (RemeshChunkJob& j) {
	ZoneScoped;

	// neighbour chunks (can be null)
	auto const* nc_nx = get_neighbour_blocks(j, 0);
	auto const* nc_ny = get_neighbour_blocks(j, 1);
	auto const* nc_nz = get_neighbour_blocks(j, 2);

	int idx = 0;

	// fast meshing without neighbour chunk access
	for (int z=0; z<CHUNK_SIZE; ++z) {
		for (int y=0; y<CHUNK_SIZE; ++y) {
			for (int x=0; x<CHUNK_SIZE; ++x) {

				block_id id = j.chunks->read_block(x,y,z, j.chunk);

				// TODO: 

				{ // X
					block_id nid = x > 0 ? j.chunks->read_block(x-1, y, z, j.chunk) : (nc_nx ? j.chunks->read_block(CHUNK_SIZE-1, y, z, nc_nx) : B_NULL);
					if (nid != id)
						face<0>(j, id, nid, idx);
				}
				{ // Y
					block_id nid = y > 0 ? j.chunks->read_block(x, y-1, z, j.chunk) : (nc_ny ? j.chunks->read_block(x, CHUNK_SIZE-1, z, nc_ny) : B_NULL);
					if (nid != id)
						face<1>(j, id, nid, idx);
				}
				{ // Z
					block_id nid = z > 0 ? j.chunks->read_block(x, y, z-1, j.chunk) : (nc_nz ? j.chunks->read_block(x, y, CHUNK_SIZE-1, nc_nz) : B_NULL);
					if (nid != id)
						face<2>(j, id, nid, idx);
				}

				if (g_assets.block_meshes.block_meshes[id] >= 0)
					block_mesh(j, id, g_assets.block_meshes.block_meshes[id], idx);

				idx++;
			}
		}
	}
}

RemeshChunkJob::RemeshChunkJob (Chunk* chunk, Chunks* chunks, WorldGenerator const& wg, bool mesh_world_border):
		chunk{chunk}, chunks{chunks},
		mesh_world_border{mesh_world_border} {

	chunk_seed = wg.seed ^ hash(chunk->pos * CHUNK_SIZE);
}

void RemeshChunkJob::execute () {
	mesh_chunk(*this);
}
