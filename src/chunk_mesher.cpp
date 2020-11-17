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
	uint8v3 block_pos;

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
		v->pos = block_pos;
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

#if 0
	void block_mesh (BlockMeshInfo info, int3 block_pos_world, uint64_t world_seed) {
		//OPTICK_EVENT();

		// get a 'random' but deterministic value based on block position
		uint64_t h = hash(block_pos_world) ^ world_seed;

		// get a random determinisitc 2d offset
		float rand_val = (float)(h & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // [0, 1)

		float2 rand_offs;
		rand_offs.x = rand_val;
		rand_offs.y = (float)((h >> 32) & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // [0, 1)
		rand_offs = rand_offs * 2 - 1; // [0,1] -> [-1,+1]

									   // get a random deterministic variant
		int variant = tile.variants > 1 ? (int)(rand_val * (float)tile.variants) : 0; // [0, tile.variants)

		for (int i=0; i<info.size; ++i) {
			auto v = (*block_meshes)[info.offset + i];

			auto ptr = opaque_vertices->push();

			ptr->pos_model = v.pos_model + block_pos + 0.5f + float3(rand_offs * 0.25f, 0);
			ptr->uv = v.uv * tile.uv_size + tile.uv_pos;

			ptr->tex_indx = tile.base_index + variant;
			ptr->block_light = chunk_data->block_light[cur] * 255 / MAX_LIGHT_LEVEL;
			ptr->sky_light = chunk_data->sky_light[cur] * 255 / MAX_LIGHT_LEVEL;
			ptr->hp = chunk_data->hp[cur];
		}
	}
#endif

	void mesh_chunk (Assets const& assets, WorldGenerator const& wg, Chunk* chunk, ChunkMesh* mesh) {
		ZoneScoped;
		
		//block_meshes = &graphics.tile_textures.block_meshes;

		chunk_data = chunk->blocks.get();
		this->mesh = mesh;

		int3 chunk_pos_world = chunk->pos * CHUNK_SIZE;

		//auto _a = get_timestamp();
		//uint64_t sum = 0;

		int3 i = 0;
		for (i.z=0; i.z<CHUNK_SIZE; ++i.z) {
			for (i.y=0; i.y<CHUNK_SIZE; ++i.y) {

				i.x = 0;
				cur = ChunkData::pos_to_index(i);

				for (; i.x<CHUNK_SIZE; ++i.x) {

					auto id = chunk_data->id[cur];

					if (g_blocks.blocks[id].collision != CM_GAS) {
						//auto _b = get_timestamp();

						block_pos = (uint8v3)i;

						tile = assets.block_tiles[id];
						//auto mesh_info = graphics.tile_textures.block_meshes_info[id];

						//if (mesh_info.offset < 0) {
							if (g_blocks.blocks[id].transparency == TM_TRANSPARENT)
								cube_transperant();
							else
								cube_opaque();
						//} else {
						//
						//	block_mesh(mesh_info, i + chunk->chunk_pos_world(), wg.seed);
						//}

						//sum += get_timestamp() - _b;
					}

					cur++;
				}
			}
		}

		//auto total = get_timestamp() - _a;

		//logf("mesh_chunk total: %7.3f vs %7.3f = %7.3f", (float)total, (float)sum, (float)sum / (float)total);
	}
};

void mesh_chunk (Assets const& assets, WorldGenerator const& wg, Chunk* chunk, ChunkMesh* mesh) {
	ThreadChunkMesher cm;
	return cm.mesh_chunk(assets, wg, chunk, mesh);
}
