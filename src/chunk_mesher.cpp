#include "common.hpp"
#include "chunk_mesher.hpp"
#include "world_generator.hpp"
#include "graphics.hpp"
#include "player.hpp"

void face (ChunkMesh* mesh, block_id id, int3 block_pos, BlockFace facei, BlockTile const& tile) {
	auto& b = g_blocks.blocks[id];
	if (id == B_NULL || b.collision == CM_GAS) return;

	auto& out = b.transparency == TM_TRANSPARENT ? mesh->tranparent_vertices : mesh->opaque_vertices;

	auto* v = out.push();

	int16_t fixd_posx = (int16_t)(block_pos.x * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posy = (int16_t)(block_pos.y * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posz = (int16_t)(block_pos.z * BlockMeshInstance::FIXEDPOINT_FAC);

	v->posx = fixd_posx;
	v->posy = fixd_posy;
	v->posz = fixd_posz;
	v->texid = tile.calc_tex_index(facei, 0);
	v->meshid = facei;
}

void block_mesh (ChunkMesh* mesh, BlockMeshes::Mesh& info, BlockTile const& tile, int3 block_pos, uint64_t chunk_seed) {
	// get a 'random' but deterministic value based on block position
	uint64_t h = hash(block_pos) ^ chunk_seed;
		
	// get a random determinisitc 2d offset
	float rand1 = (float)( h        & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // [0, 1)
	float rand2 = (float)((h >> 32) & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // [0, 1)
		
	float rand_offsx = rand1;
	float rand_offsy = rand2;
		
	// get a random deterministic variant
	int variant = (int)(rand1 * (float)tile.variants); // [0, tile.variants)
		
	int texid = tile.calc_tex_index((BlockFace)0, variant);

	float posx = (float)block_pos.x + (rand_offsx * 2 - 1) * 0.25f; // [0,1] -> [-1,+1]
	float posy = (float)block_pos.y + (rand_offsy * 2 - 1) * 0.25f; // [0,1] -> [-1,+1]
	float posz = (float)block_pos.z;

	int16_t fixd_posx = (int16_t)roundi(posx * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posy = (int16_t)roundi(posy * BlockMeshInstance::FIXEDPOINT_FAC);
	int16_t fixd_posz = (int16_t)roundi(posz * BlockMeshInstance::FIXEDPOINT_FAC);

	for (int meshid=info.offset; meshid < info.offset + info.length; ++meshid) {
		auto* v = mesh->opaque_vertices.push();
		v->posx = fixd_posx;
		v->posy = fixd_posy;
		v->posz = fixd_posz;
		v->texid = texid;
		v->meshid = meshid;
	}
}

void mesh_chunk (Assets const& assets, WorldGenerator const& wg, Chunks& chunks, Chunk* chunk, ChunkMesh* mesh) {
	ZoneScoped;
	
	uint64_t chunk_seed = wg.seed ^ hash(chunk->pos * CHUNK_SIZE);

	// neighbour chunks (can be null)
	auto get_neighbour_blocks = [&] (int3 offs) -> block_id* {
		auto* nc = chunks.query_chunk(chunk->pos + offs);
		if (!nc) return nullptr;
		return nc->blocks->ids;
	};
	auto* nc_nx = get_neighbour_blocks(int3(-1,0,0));
	auto* nc_ny = get_neighbour_blocks(int3(0,-1,0));
	auto* nc_nz = get_neighbour_blocks(int3(0,0,-1));

	auto* ptr = chunk->blocks->ids;

	int idx = 0;

	idx += CHUNK_LAYER_OFFS;
	for (int z=1; z<CHUNK_SIZE; ++z) {

		idx += CHUNK_ROW_OFFS;
		for (int y=1; y<CHUNK_SIZE; ++y) {

			idx += 1;
			for (int x=1; x<CHUNK_SIZE; ++x) {
	
				block_id id = ptr[idx];
	
				auto& tile = assets.block_tiles[id];
				auto mesh_idx = assets.block_meshes[id];
	
				auto transparency = g_blocks.blocks[id].transparency;
	
				{ // +X
					block_id nid = ptr[idx - 1];
					if (nid != id) {
						if (transparency != TM_OPAQUE && assets.block_meshes[nid] < 0)
							face(mesh, nid, int3(x-1,y,z), BF_POS_X, assets.block_tiles[nid]);
						if (g_blocks.blocks[nid].transparency != TM_OPAQUE && mesh_idx < 0)
							face(mesh, id, int3(x,y,z), BF_NEG_X, tile);
					}
				}
				{ // +Y
					block_id nid = ptr[idx - CHUNK_ROW_OFFS];
					if (nid != id) {
						if (transparency != TM_OPAQUE && assets.block_meshes[nid] < 0)
							face(mesh, nid, int3(x,y-1,z), BF_POS_Y, assets.block_tiles[nid]);
						if (g_blocks.blocks[nid].transparency != TM_OPAQUE && mesh_idx < 0)
							face(mesh, id, int3(x,y,z), BF_NEG_Y, tile);
					}
				}
				{ // +Z
					block_id nid = ptr[idx - CHUNK_LAYER_OFFS];
					if (nid != id) {
						if (transparency != TM_OPAQUE && assets.block_meshes[nid] < 0)
							face(mesh, nid, int3(x,y,z-1), BF_POS_Z, assets.block_tiles[nid]);
						if (g_blocks.blocks[nid].transparency != TM_OPAQUE && mesh_idx < 0)
							face(mesh, id, int3(x,y,z), BF_NEG_Z, tile);
					}
				}
	
				if (mesh_idx >= 0) {
					auto mesh_info = assets.block_mesh_info[mesh_idx];
					block_mesh(mesh, mesh_info, tile, int3(x,y,z), chunk_seed);
				}
	
				idx++;
			}
		}
	}
}
