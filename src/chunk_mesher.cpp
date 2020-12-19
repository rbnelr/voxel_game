#include "common.hpp"
#include "chunk_mesher.hpp"
#include "world_generator.hpp"
#include "graphics.hpp"
#include "player.hpp"

//#define NOINLINE __declspec(noinline)
#define NOINLINE

NOINLINE void block_mesh (RemeshChunkJob& j, int idx, block_id id, int meshid) {
	int block_pos_z = idx / CHUNK_LAYER_OFFS;
	int block_pos_y = idx % CHUNK_LAYER_OFFS / CHUNK_ROW_OFFS;
	int block_pos_x = idx % CHUNK_ROW_OFFS;

	// get a 'random' but deterministic value based on block position
	uint64_t h = hash(int3(block_pos_x, block_pos_y, block_pos_z)) ^ j.chunk_seed;
	
	// get a random determinisitc 2d offset
	float rand1 = (float)( h        & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // [0, 1)
	float rand2 = (float)((h >> 32) & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // [0, 1)
		
	float rand_offsx = rand1;
	float rand_offsy = rand2;
		
	auto& tile = j.block_tiles[id];

	// get a random deterministic variant
	int variant = (int)(rand1 * (float)tile.variants); // [0, tile.variants)
		
	int texid = tile.calc_tex_index((BlockFace)0, variant);

	float posx = (float)block_pos_x + (rand_offsx * 2 - 1) * 0.25f; // [0,1] -> [-1,+1]
	float posy = (float)block_pos_y + (rand_offsy * 2 - 1) * 0.25f; // [0,1] -> [-1,+1]
	float posz = (float)block_pos_z;

	int16_t fixd_posx = (int16_t)roundi(posx * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posy = (int16_t)roundi(posy * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posz = (int16_t)roundi(posz * BlockMeshInstance::FIXEDPOINT_FAC);

	auto& info = j.block_mesh_info[meshid];

	for (int meshid=info.offset; meshid < info.offset + info.length; ++meshid) {
		auto* v = j.mesh.opaque_vertices.push();
		v->posx = fixd_posx;
		v->posy = fixd_posy;
		v->posz = fixd_posz;
		v->texid = texid;
		v->meshid = meshid;
	}
}

block_id const* get_neighbour_blocks (RemeshChunkJob& j, int3 offs) {
	auto* nc = j.chunks.query_chunk(j.chunk->pos + offs);
	if (!nc || (nc->flags & Chunk::LOADED) == 0)
		return g_null_chunk.ids; // return dummy chunk data with B_NULL to avoid nullptr check in performance-critical code for non-loaded neighbours
	return nc->voxels->ids;
}

void face (RemeshChunkJob& j, int3 block_pos, block_id id, ChunkMeshData& mesh, BlockFace facei) {
	auto* v = mesh.push();

	int16_t fixd_posx = (int16_t)(block_pos.x * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posy = (int16_t)(block_pos.y * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posz = (int16_t)(block_pos.z * BlockMeshInstance::FIXEDPOINT_FAC);

	v->posx = fixd_posx;
	v->posy = fixd_posy;
	v->posz = fixd_posz;
	v->texid = j.block_tiles[id].calc_tex_index(facei, 0);
	v->meshid = facei;
}

template <int AXIS>
NOINLINE void face (RemeshChunkJob& j, int idx, block_id id, block_id nid) {
	int z = idx / CHUNK_LAYER_OFFS;
	int y = idx % CHUNK_LAYER_OFFS / CHUNK_ROW_OFFS;
	int x = idx % CHUNK_ROW_OFFS;
	
	auto& b = g_blocks.blocks[id];
	auto& nb = g_blocks.blocks[nid];

	if (nb.collision != CM_GAS && b.transparency != TM_OPAQUE && j.block_meshes[nid] < 0 && nid != B_NULL) {
		auto& mesh = nb.transparency == TM_TRANSPARENT ? j.mesh.tranparent_vertices : j.mesh.opaque_vertices;
		int3 pos = int3(x,y,z);
		pos[AXIS] -= 1;
		face(j, pos, nid, mesh, (BlockFace)(BF_POS_X + AXIS*2));
	}

	if (b.collision != CM_GAS && nb.transparency != TM_OPAQUE && j.block_meshes[id] < 0 && id != B_NULL) {
		auto& mesh = b.transparency == TM_TRANSPARENT ? j.mesh.tranparent_vertices : j.mesh.opaque_vertices;
		face(j, int3(x,y,z), id, mesh, (BlockFace)(BF_NEG_X + AXIS*2));
	}
}

void mesh_chunk (RemeshChunkJob& j) {
	ZoneScoped;

	// neighbour chunks (can be null)
	auto const* nc_nx = get_neighbour_blocks(j, int3(-1,0,0));
	auto const* nc_ny = get_neighbour_blocks(j, int3(0,-1,0));
	auto const* nc_nz = get_neighbour_blocks(j, int3(0,0,-1));

	auto const* ptr = j.chunk->voxels->ids;

#if 0
	int idx = 0;

	block_id const* prevz = nc_nz + CHUNK_SIZE*CHUNK_LAYER_OFFS;
	for (int z=0; z<CHUNK_SIZE; ++z) {
		
		block_id const* prevy = nc_ny + CHUNK_SIZE*CHUNK_ROW_OFFS;
		for (int y=0; y<CHUNK_SIZE; ++y) {
			
			block_id prevx = nc_nx[idx + CHUNK_SIZE-1];
			for (int x=0; x<CHUNK_SIZE; ++x) {

				block_id id = ptr[idx];
	
				{ // X
					block_id nid = prevx;
					prevx = id;

					if (nid != id)
						face_x(j, idx, id, nid);
				}
				{ // Y
					block_id nid = prevy[idx - CHUNK_ROW_OFFS];

					if (nid != id)
						face_y(j, idx, id, nid);
				}
				{ // Z
					block_id nid = prevz[idx - CHUNK_LAYER_OFFS];

					if (nid != id)
						face_z(j, idx, id, nid);
				}
	
				if (j.block_meshes[id] >= 0)
					block_mesh(j, idx, id, j.block_meshes[id]);
	
				idx++;
			}
			prevy = ptr;
		}
		prevz = ptr;
	}
#else
	for (int idx=0; idx < CHUNK_SIZE*CHUNK_SIZE*CHUNK_SIZE; ++idx) {
		block_id id = ptr[idx];

		int x = idx % CHUNK_SIZE;
		block_id const* prevx = x != 0 ? ptr : (nc_nx + CHUNK_SIZE);
		{ // X
			block_id nid = prevx[idx - 1];
			if (nid != id)
				j.xfaces[j.xfaces_count++] = { idx, id, nid };
		}

		int y = idx % CHUNK_LAYER_OFFS / CHUNK_ROW_OFFS;
		block_id const* prevy = y != 0 ? ptr : (nc_ny + CHUNK_SIZE*CHUNK_ROW_OFFS);
		{ // Y
			block_id nid = prevy[idx - CHUNK_ROW_OFFS];
			if (nid != id)
				j.yfaces[j.yfaces_count++] = { idx, id, nid };
		}

		int z = idx / CHUNK_LAYER_OFFS;
		block_id const* prevz = z != 0 ? ptr : (nc_nz + CHUNK_SIZE*CHUNK_LAYER_OFFS);
		{ // Z
			block_id nid = prevz[idx - CHUNK_LAYER_OFFS];
			if (nid != id)
				j.zfaces[j.zfaces_count++] = { idx, id, nid };
		}

		if (j.block_meshes[id] >= 0)
			j.mesh_voxels[j.mesh_voxels_count++] = { idx, id };
	}

	for (int i=0; i<j.xfaces_count; ++i)
		face<0>(j, j.xfaces[i].idx, j.xfaces[i].id, j.xfaces[i].nid);

	for (int i=0; i<j.yfaces_count; ++i)
		face<1>(j, j.yfaces[i].idx, j.yfaces[i].id, j.yfaces[i].nid);

	for (int i=0; i<j.zfaces_count; ++i)
		face<2>(j, j.zfaces[i].idx, j.zfaces[i].id, j.zfaces[i].nid);

	for (int i=0; i<j.mesh_voxels_count; ++i)
		block_mesh(j, j.mesh_voxels[i].idx, j.mesh_voxels[i].id, j.block_meshes[j.mesh_voxels[i].id]);
#endif
}

RemeshChunkJob::RemeshChunkJob (Chunk* chunk, Chunks& chunks, Assets const& assets, WorldGenerator const& wg):
		chunk{chunk}, chunks{chunks},
		block_mesh_info{assets.block_mesh_info.data()},
		block_meshes{assets.block_meshes.data()},
		block_tiles{assets.block_tiles.data()} {

	chunk_seed = wg.seed ^ hash(chunk->pos * CHUNK_SIZE);
}

void RemeshChunkJob::execute () {
	mesh_chunk(*this);
}
