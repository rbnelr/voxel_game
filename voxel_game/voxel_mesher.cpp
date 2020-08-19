#include "stdafx.hpp"
#include "voxel_mesher.hpp"
#include "world_generator.hpp"

#if 0
namespace meshing {
struct Chunk_Mesher {
	Page* opaque_vertices;
	Page* tranparent_vertices;

	ChunkData* neighbours[3][3][3];

	Block get_block (int3 pos) {
		int3 int3;
		int3 chunk = get_chunk_from_block_pos(pos, &int3);

		ChunkData* data = neighbours[chunk.z+1][chunk.y+1][chunk.x+1];

		if (data == nullptr)
			return { B_NULL, 0, 0, 0 };

		return data->get(int3);
	}

	// per block
	float3 block_pos;

	BlockTileInfo tile;
	std::vector<BlockMeshVertex> const* block_meshes;

	bool bt_is_opaque (block_id id) {
		auto t = blocks.transparency[id];
		if (t == TM_OPAQUE)
			return true;

		return false;
	}

	static constexpr int3 face_offsets[6][4] = {
		{ int3(-1, 0, 0), int3(-1,-1, 0), int3(-1,-1,-1), int3(-1, 0,-1) },
		{ int3( 0, 0, 0), int3( 0,-1, 0), int3( 0,-1,-1), int3( 0, 0,-1) },
		{ int3( 0,-1, 0), int3(-1,-1, 0), int3(-1,-1,-1), int3( 0,-1,-1) },
		{ int3( 0, 0, 0), int3(-1, 0, 0), int3(-1, 0,-1), int3( 0, 0,-1) },
		{ int3( 0, 0,-1), int3(-1, 0,-1), int3(-1,-1,-1), int3( 0,-1,-1) },
		{ int3( 0, 0, 0), int3(-1, 0, 0), int3(-1,-1, 0), int3( 0,-1, 0) },
	};

	inline uint8 calc_block_light (BlockFace face, int3 vert_pos, int3 pos) {
		int total = 0;

		for (auto offset : face_offsets[face]) {
			total += get_block(pos + vert_pos + offset).block_light;
		}
		
		return (total * 255) / (4 * MAX_LIGHT_LEVEL);
	}
	inline uint8 calc_sky_light (BlockFace face, int3 vert_pos, int3 pos) {
		int total = 0;

		for (auto offset : face_offsets[face]) {
			total += get_block(pos + vert_pos + offset).sky_light;
		}

		return (total * 255) / (4 * MAX_LIGHT_LEVEL);
	}

	// float3	pos_model;
	// float2	uv;
	// uint8	tex_indx;
	// uint8	block_light;
	// uint8	sky_light;
	// uint8	hp;

#define VERT(x,y,z, u,v, face) \
		{ block_pos + float3(x,y,z), float2(u,v), (uint8)tile.calc_texture_index(face), \
		  calc_block_light(face, int3(x,y,z)), calc_sky_light(face, int3(x,y,z)), block.hp }

#define POS \
	{ {0,1,0}, {0,0,0}, {0,0,1}, {0,1,1} }, \
	{ {1,0,0}, {1,1,0}, {1,1,1}, {1,0,1} }, \
	{ {0,0,0}, {1,0,0}, {1,0,1}, {0,0,1} }, \
	{ {1,1,0}, {0,1,0}, {0,1,1}, {1,1,1} }, \
	{ {0,1,0}, {1,1,0}, {1,0,0}, {0,0,0} }, \
	{ {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1} }, \

	static constexpr float3 posf[6][4] = { POS };
	static constexpr int3   pos[6][4]  = { POS };

	static constexpr float2 uv[4]   = { {0,0}, {1,0}, {1,1}, {0,1} };

	static constexpr int tri_oder[2][6] = {
		{ 0,1,3, 3,1,2 },
		{ 1,2,0, 0,2,3 },
	};

	static constexpr int3 offsets[6] = {
		int3(-1, 0, 0),
		int3(+1, 0, 0),
		int3(0, -1, 0),
		int3(0, +1, 0),
		int3(0, 0, -1),
		int3(0, 0, +1),
	};

	void face (MeshingData* out, BlockFace facei, Block block, int3 _pos) {

		ChunkMesh::Vertex vert[4];

		float3 const* pf = posf[facei];
		int3 const* p = pos[facei];

		auto hp = block.hp;

		for (int i=0; i<4; ++i)
			vert[i].pos_model	= block_pos + pf[i];
		for (int i=0; i<4; ++i)
			vert[i].uv			= uv[i];
		for (int i=0; i<4; ++i)
			vert[i].tex_indx	= (uint8)tile.calc_texture_index(facei);
		for (int i=0; i<4; ++i)
			vert[i].block_light	= calc_block_light(facei, p[i], _pos);
		for (int i=0; i<4; ++i)
			vert[i].sky_light	= calc_sky_light(facei, p[i], _pos);
		for (int i=0; i<4; ++i)
			vert[i].hp			= hp;

		bool b = vert[0].sky_light + vert[2].sky_light >= vert[1].sky_light + vert[3].sky_light;

		int const* order = tri_oder[(int)b];
		for (int i=0; i<6; ++i) {
			*out->push() = vert[order[i]];
		}
	}

	void cube_opaque (Block block, int3 pos) {
		for (int i=0; i<6; ++i) {
			block_id n = get_block(pos + offsets[i]).id;
			if (!bt_is_opaque(n))
				face(opaque_vertices, (BlockFace)i, block, pos);
		}
	}
	void cube_transperant (Block block, int3 pos) {
		for (int i=0; i<6; ++i) {
			block_id n = get_block(pos + offsets[i]).id;
			block_id b = block.id;
			if (!bt_is_opaque(n) && n != b)
				face(tranparent_vertices, (BlockFace)i, block, pos);
		}
	}
	void block_mesh (BlockMeshInfo info, Block block, int3 block_pos_world, uint64_t world_seed) {
		
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
			ptr->block_light = block.block_light * 255 / MAX_LIGHT_LEVEL;
			ptr->sky_light = block.sky_light * 255 / MAX_LIGHT_LEVEL;
			ptr->hp = block.hp;
		}
	}

	void mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, TileTextures const& tile_textures, WorldGenerator const& wg, Chunk* chunk, MeshingResult* res) {
		block_meshes = &tile_textures.block_meshes;

		for (int z=-1; z<=1; ++z) {
			for (int y=-1; y<=1; ++y) {
				for (int x=-1; x<=1; ++x) {
					Chunk* ch = x==0 && y==0 && z==0 ? chunk : chunks.query_chunk(chunk->coord + int3(x,y,z));
		
					neighbours[z+1][y+1][x+1] = ch ? ch->blocks.get() : nullptr;
				}
			}
		}

		neighbours[1][1][1] = chunk->blocks.get();

		opaque_vertices		= &res->opaque_vertices;
		tranparent_vertices	= &res->tranparent_vertices;

		opaque_vertices->init();
		tranparent_vertices->init();

		int3 chunk_pos_world = chunk->chunk_pos_world();

		//auto _a = get_timestamp();
		//uint64_t sum = 0;

		int3 i = 0;
		for (i.z=0; i.z<CHUNK_DIM; ++i.z) {
			for (i.y=0; i.y<CHUNK_DIM; ++i.y) {
				for (i.x=0; i.x<CHUNK_DIM; ++i.x) {

					auto b = get_block(i);

					if (b.id != B_AIR) {
						//auto _b = get_timestamp();

						block_pos = (float3)i;

						tile = tile_textures.block_tile_info[b.id];
						auto mesh_info = tile_textures.block_meshes_info[b.id];

						if (mesh_info.offset < 0) {
							if (blocks.transparency[b.id] == TM_TRANSPARENT)
								cube_transperant(b, i);
							else
								cube_opaque(b, i);
						} else {

							block_mesh(mesh_info, b, i + chunk->chunk_pos_world(), wg.seed);
						}

						//sum += get_timestamp() - _b;
					}
				}
			}
		}

		//auto total = get_timestamp() - _a;

		//logf("mesh_chunk total: %7.3f vs %7.3f = %7.3f", (float)total, (float)sum, (float)sum / (float)total);
	}
};

void mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, TileTextures const& tile_textures, WorldGenerator const& wg, Chunk* chunk, MeshingResult* res) {
	ZoneScopedN("mesh_chunk");

	Chunk_Mesher cm;
	return cm.mesh_chunk(chunks, graphics, tile_textures, wg, chunk, res);
}
}
#endif
