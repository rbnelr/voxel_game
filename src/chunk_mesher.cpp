#include "common.hpp"
#include "chunk_mesher.hpp"
#include "world_generator.hpp"
#include "graphics.hpp"
#include "player.hpp"

static constexpr int offs (int3 offset) {
	return offset.z * CHUNK_LAYER_OFFS + offset.y * CHUNK_ROW_OFFS + offset.x;
}

struct ThreadChunkMesher {
	ChunkData* chunk_data;
	ChunkMesh* mesh;

	uint64_t cur;

	// per block
	int3 block_pos;
	int3 chunk_origin;

	BlockTile tile;
	//std::vector<BlockMeshVertex> const* block_meshes;

	bool is_opaque (block_id id) {
		return g_blocks.blocks[id].transparency == TM_OPAQUE;
	}

	static constexpr int offsets[6] = {
		-1,
		+1,
		-CHUNK_ROW_OFFS,
		+CHUNK_ROW_OFFS,
		-CHUNK_LAYER_OFFS,
		+CHUNK_LAYER_OFFS,
	};

	void face (MeshData* out, BlockFace facei) {
		auto* v = out->push();

		int16_t fixd_posx = (int16_t)(block_pos.x * BlockMeshInstance::FIXEDPOINT_FAC);
		int16_t fixd_posy = (int16_t)(block_pos.y * BlockMeshInstance::FIXEDPOINT_FAC);
		int16_t fixd_posz = (int16_t)(block_pos.z * BlockMeshInstance::FIXEDPOINT_FAC);

		v->posx = fixd_posx;
		v->posy = fixd_posy;
		v->posz = fixd_posz;
		v->texid = tile.calc_tex_index(facei, 0);
		v->meshid = facei;
	}

	void cube_opaque () {
		for (int i=0; i<6; ++i) {
			block_id n = chunk_data->id[cur + offsets[i]];
			if (!is_opaque(n))
				face(&mesh->opaque_vertices, (BlockFace)i);
		}
	}
	void cube_transperant () {
		for (int i=0; i<6; ++i) {
			block_id n = chunk_data->id[cur + offsets[i]];
			block_id b = chunk_data->id[cur];
			if (!is_opaque(n) && n != b)
				face(&mesh->tranparent_vertices, (BlockFace)i);
		}
	}

	void block_mesh (BlockMeshes::Mesh& info, uint64_t world_seed) {
		// get a 'random' but deterministic value based on block position
		uint64_t h = hash(block_pos + chunk_origin) ^ world_seed;
		
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

	void mesh_chunk (Assets const& assets, WorldGenerator const& wg, Chunk* chunk, ChunkMesh* mesh) {
		ZoneScoped;
		
		//block_meshes = &graphics.tile_textures.block_meshes;

		chunk_data = chunk->blocks.get();
		this->mesh = mesh;

		chunk_origin = chunk->pos * CHUNK_SIZE;

		//auto _a = get_timestamp();
		//uint64_t sum = 0;

		for (block_pos.z=0; block_pos.z<CHUNK_SIZE; ++block_pos.z) {
			for (block_pos.y=0; block_pos.y<CHUNK_SIZE; ++block_pos.y) {

				block_pos.x = 0;
				cur = ChunkData::pos_to_index(block_pos);

				for (; block_pos.x<CHUNK_SIZE; ++block_pos.x) {

					auto id = chunk_data->id[cur];

					if (g_blocks.blocks[id].collision != CM_GAS) {

						tile = assets.block_tiles[id];
						auto mesh_idx = assets.block_meshes[id];

						if (mesh_idx < 0) {
							if (g_blocks.blocks[id].transparency == TM_TRANSPARENT)
								cube_transperant();
							else
								cube_opaque();
						} else {
							auto mesh_info = assets.block_mesh_info[mesh_idx];
							block_mesh(mesh_info, wg.seed);
						}

					}

					cur++;
				}
			}
		}

	}
};

void mesh_chunk (Assets const& assets, WorldGenerator const& wg, Chunk* chunk, ChunkMesh* mesh) {
	ThreadChunkMesher cm;
	return cm.mesh_chunk(assets, wg, chunk, mesh);
}
