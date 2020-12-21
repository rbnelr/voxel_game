#include "common.hpp"
#include "chunk_mesher.hpp"
#include "world_generator.hpp"
#include "graphics.hpp"
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
		
	auto& tile = j.block_tiles[id];

	// get a random deterministic variant
	int variant = (int)(rand1 * (float)tile.variants); // [0, tile.variants)
		
	int texid = tile.calc_tex_index((BlockFace)0, variant);

	float posx = (float)pos.x + (rand_offsx * 2 - 1) * 0.25f; // [0,1] -> [-1,+1]
	float posy = (float)pos.y + (rand_offsy * 2 - 1) * 0.25f; // [0,1] -> [-1,+1]
	float posz = (float)pos.z;

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
	if (nc == nullptr || (nc->flags & Chunk::LOADED) == 0 || nc->voxels.ids == nullptr) // TODO: somehow handle sparse chunkvoxels here
		return g_null_chunk; // return dummy chunk data with B_NULL to avoid nullptr check in performance-critical code for non-loaded neighbours
	return nc->voxels.ids;
}

void face (RemeshChunkJob& j, block_id id, ChunkMeshData& mesh, BlockFace facei, int3 pos) {
	auto* v = mesh.push();

	int16_t fixd_posx = (int16_t)(pos.x * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posy = (int16_t)(pos.y * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posz = (int16_t)(pos.z * BlockMeshInstance::FIXEDPOINT_FAC);

	v->posx = fixd_posx;
	v->posy = fixd_posy;
	v->posz = fixd_posz;
	v->texid = j.block_tiles[id].calc_tex_index(facei, 0);
	v->meshid = facei;
}

template <int AXIS>
NOINLINE void face (RemeshChunkJob& j, block_id id, block_id nid, int idx) {
	int3 pos = pos_from_idx(idx);
	
	if (!j.draw_world_border && (id == B_NULL || nid == B_NULL))
		return;

	auto& b = g_blocks.blocks[id];
	auto& nb = g_blocks.blocks[nid];

	// generate face of our voxel that faces negative direction neighbour
	if (b.collision != CM_GAS && nb.transparency != TM_OPAQUE && j.block_meshes[id] < 0) {
		auto& mesh = b.transparency == TM_TRANSPARENT ? j.mesh.tranparent_vertices : j.mesh.opaque_vertices;
		face(j, id, mesh, (BlockFace)(BF_NEG_X + AXIS*2), pos);
	}

	// generate face of negative direction neighbour that faces this voxel
	if (nb.collision != CM_GAS && b.transparency != TM_OPAQUE && j.block_meshes[nid] < 0 && nid != B_NULL) {
		auto& mesh = nb.transparency == TM_TRANSPARENT ? j.mesh.tranparent_vertices : j.mesh.opaque_vertices;
		pos[AXIS] -= 1;
		face(j, nid, mesh, (BlockFace)(BF_POS_X + AXIS*2), pos);
	}
}

void mesh_chunk (RemeshChunkJob& j) {
	ZoneScoped;

	// neighbour chunks (can be null)
	auto const* nc_nx = get_neighbour_blocks(j, int3(-1,0,0));
	auto const* nc_ny = get_neighbour_blocks(j, int3(0,-1,0));
	auto const* nc_nz = get_neighbour_blocks(j, int3(0,0,-1));
	assert(nc_nx && nc_ny && nc_nz);

	j.chunk->voxels._validate_ids();

	auto const* ptr = j.chunk->voxels.ids;

#if 1
	int idx = 0;

	for (int z=0; z<CHUNK_SIZE; ++z) {
		for (int y=0; y<CHUNK_SIZE; ++y) {
			for (int x=0; x<CHUNK_SIZE; ++x) {

				block_id id = ptr[idx];

				if (x > 0) { // X
					block_id nid = ptr[idx - 1];
					if (nid != id)
						face<0>(j, id, nid, idx);
				}
				if (y > 0) { // Y
					block_id nid = ptr[idx - CHUNK_ROW_OFFS];
					if (nid != id)
						face<1>(j, id, nid, idx);
				}
				if (z > 0) { // Z
					block_id nid = ptr[idx - CHUNK_LAYER_OFFS];
					if (nid != id)
						face<2>(j, id, nid, idx);
				}

				if (j.block_meshes[id] >= 0)
					block_mesh(j, id, j.block_meshes[id], idx);

				idx++;
			}
		}
	}
#elif 0
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
					if (nid != id)
						face<0>(j, id, nid, idx);
				}
				{ // Y
					block_id nid = prevy[idx - CHUNK_ROW_OFFS];
					if (nid != id)
						face<1>(j, id, nid, idx);
				}
				{ // Z
					block_id nid = prevz[idx - CHUNK_LAYER_OFFS];
					if (nid != id)
						face<2>(j, id, nid, idx);
				}
	
				if (j.block_meshes[id] >= 0)
					block_mesh(j, id, j.block_meshes[id], idx);

				prevx = id;
				idx++;
			}
			prevy = ptr;
		}
		prevz = ptr;
	}
#else
	{
		int idx = 0;
		for (int z=0; z<CHUNK_SIZE; ++z) {
			for (int y=0; y<CHUNK_SIZE; ++y) {
			
				block_id prevx = nc_nx[idx + CHUNK_SIZE-1];
				for (int x=0; x<CHUNK_SIZE; ++x) {

					block_id id = ptr[idx];
				
					{ // X
						block_id nid = prevx;
						if (nid != id)
							face<0>(j, id, nid, idx);
					}
	
					if (j.block_meshes[id] >= 0)
						block_mesh(j, id, j.block_meshes[id], idx);

					prevx = id;
					idx++;
				}
			}
		}
	}

	{
		for (int z=0; z<CHUNK_SIZE; ++z) {
			for (int x=0; x<CHUNK_SIZE; ++x) {
				int idx = z * CHUNK_LAYER_OFFS + x;

				block_id prevy = nc_ny[idx + (CHUNK_SIZE-1)*CHUNK_ROW_OFFS];
				for (int y=0; y<CHUNK_SIZE; ++y) {
	
					block_id id = ptr[idx];
				
					{ // Y
						block_id nid = prevy;
						if (nid != id)
							face<1>(j, id, nid, idx);
					}
	
					if (j.block_meshes[id] >= 0)
						block_mesh(j, id, j.block_meshes[id], idx);
	
					prevy = id;

					idx += CHUNK_ROW_OFFS;
				}
			}
		}
	}

	{
		for (int y=0; y<CHUNK_SIZE; ++y) {
			for (int x=0; x<CHUNK_SIZE; ++x) {
				int idx = y * CHUNK_ROW_OFFS + x;
	
				block_id prevz = nc_nz[idx + (CHUNK_SIZE-1)*CHUNK_LAYER_OFFS];
				for (int z=0; z<CHUNK_SIZE; ++z) {
	
					block_id id = ptr[idx];
				
					{ // Z
						block_id nid = prevz;
						if (nid != id)
							face<2>(j, id, nid, idx);
					}
	
					if (j.block_meshes[id] >= 0)
						block_mesh(j, id, j.block_meshes[id], idx);
	
					prevz = id;
					idx += CHUNK_LAYER_OFFS;
				}
			}
		}
	}
#endif
}

RemeshChunkJob::RemeshChunkJob (Chunk* chunk, Chunks& chunks, Assets const& assets, WorldGenerator const& wg, bool draw_world_border):
		chunk{chunk}, chunks{chunks},
		block_mesh_info{assets.block_mesh_info.data()},
		block_meshes{assets.block_meshes.data()},
		block_tiles{assets.block_tiles.data()},
		draw_world_border{draw_world_border} {

	chunk_seed = wg.seed ^ hash(chunk->pos * CHUNK_SIZE);
}

void RemeshChunkJob::execute () {
	mesh_chunk(*this);
}
