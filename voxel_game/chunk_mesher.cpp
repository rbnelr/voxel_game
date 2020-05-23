#include "chunk_mesher.hpp"
#include "world_generator.hpp"
#include "util/timer.hpp"
#include "dear_imgui.hpp"

static constexpr int offs (int3 offset) {
	return offset.z * CHUNK_LAYER_OFFS + offset.y * CHUNK_ROW_OFFS + offset.x;
}

struct Chunk_Mesher {
	bool alpha_test;

	UnsafeVector<ChunkMesh::Vertex>* opaque_vertices;
	UnsafeVector<ChunkMesh::Vertex>* tranparent_vertices;

	ChunkData* chunk_data;

	uint64_t cur;

	// per block
	float3 block_pos;

	BlockTileInfo tile;
	std::vector<BlockMeshVertex> const* block_meshes;

	bool bt_is_opaque (block_id id) {
		auto t = blocks.transparency[id];
		if (t == TM_OPAQUE)
			return true;

		if (!alpha_test)
			return t == TM_ALPHA_TEST;

		return false;
	}

	static constexpr int face_offsets[6][4] = {
		{ offs(int3(-1, 0, 0)), offs(int3(-1,-1, 0)), offs(int3(-1,-1,-1)), offs(int3(-1, 0,-1)) },
		{ offs(int3( 0, 0, 0)), offs(int3( 0,-1, 0)), offs(int3( 0,-1,-1)), offs(int3( 0, 0,-1)) },
		{ offs(int3( 0,-1, 0)), offs(int3(-1,-1, 0)), offs(int3(-1,-1,-1)), offs(int3( 0,-1,-1)) },
		{ offs(int3( 0, 0, 0)), offs(int3(-1, 0, 0)), offs(int3(-1, 0,-1)), offs(int3( 0, 0,-1)) },
		{ offs(int3( 0, 0,-1)), offs(int3(-1, 0,-1)), offs(int3(-1,-1,-1)), offs(int3( 0,-1,-1)) },
		{ offs(int3( 0, 0, 0)), offs(int3(-1, 0, 0)), offs(int3(-1,-1, 0)), offs(int3( 0,-1, 0)) },
	};

	inline uint8 calc_block_light (BlockFace face, int3 vert_pos) {
		auto* block_light = &chunk_data->block_light[cur + offs(vert_pos)];
		
		int total = 0;

		for (auto offset : face_offsets[face]) {
			total += block_light[offset];
		}
		
		return (total * 255) / (4 * MAX_LIGHT_LEVEL);
	}
	inline uint8 calc_sky_light (BlockFace face, int3 vert_pos) {
		auto* sky_light = &chunk_data->sky_light[cur + offs(vert_pos)];

		int total = 0;

		for (auto offset : face_offsets[face]) {
			total += sky_light[offset];
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
		  calc_block_light(face, int3(x,y,z)), calc_sky_light(face, int3(x,y,z)), cur->hp }

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

	static constexpr int offsets[6] = {
		-1,
		+1,
		-CHUNK_ROW_OFFS,
		+CHUNK_ROW_OFFS,
		-CHUNK_LAYER_OFFS,
		+CHUNK_LAYER_OFFS,
	};

	void face (UnsafeVector<ChunkMesh::Vertex>* out, BlockFace facei) {

		size_t offs = out->size;
		out->resize(offs + 6);
		auto* ptr = &(*out)[offs];

		ChunkMesh::Vertex vert[4];

		float3 const* pf = posf[facei];
		int3 const* p = pos[facei];

		auto hp = chunk_data->hp[cur];

		for (int i=0; i<4; ++i)
			vert[i].pos_model	= block_pos + pf[i];
		for (int i=0; i<4; ++i)
			vert[i].uv			= uv[i];
		for (int i=0; i<4; ++i)
			vert[i].tex_indx	= (uint8)tile.calc_texture_index(facei);
		for (int i=0; i<4; ++i)
			vert[i].block_light	= calc_block_light(facei, p[i]);
		for (int i=0; i<4; ++i)
			vert[i].sky_light	= calc_sky_light(facei, p[i]);
		for (int i=0; i<4; ++i)
			vert[i].hp			= hp;

		bool b = vert[0].sky_light + vert[2].sky_light >= vert[1].sky_light + vert[3].sky_light;

		int const* order = tri_oder[(int)b];
		for (int i=0; i<6; ++i) {
			*ptr++ = vert[order[i]];
		}
	}

	void cube_opaque () {
		for (int i=0; i<6; ++i) {
			block_id n = chunk_data->id[cur + offsets[i]];
			if (!bt_is_opaque(n))
				face(opaque_vertices, (BlockFace)i);
		}
	}
	void cube_transperant () {
		for (int i=0; i<6; ++i) {
			block_id n = chunk_data->id[cur + offsets[i]];
			block_id b = chunk_data->id[cur];
			if (!bt_is_opaque(n) && n != b)
				face(tranparent_vertices, (BlockFace)i);
		}
	}
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

		size_t offs = opaque_vertices->size;
		opaque_vertices->resize(offs + info.size);
		auto* ptr = &(*opaque_vertices)[offs];

		for (int i=0; i<info.size; ++i) {
			auto v = (*block_meshes)[info.offset + i];

			ptr->pos_model = v.pos_model + block_pos + 0.5f + float3(rand_offs * 0.25f, 0);
			ptr->uv = v.uv * tile.uv_size + tile.uv_pos;

			ptr->tex_indx = tile.base_index + variant;
			ptr->block_light = chunk_data->block_light[cur] * 255 / MAX_LIGHT_LEVEL;
			ptr->sky_light = chunk_data->sky_light[cur] * 255 / MAX_LIGHT_LEVEL;
			ptr->hp = chunk_data->hp[cur];

			ptr++;
		}
	}

	void mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, TileTextures const& tile_textures, WorldGenerator const& wg, Chunk* chunk, MeshingResult* res) {
		alpha_test = graphics.alpha_test;

		opaque_vertices = &res->opaque_vertices;
		tranparent_vertices = &res->tranparent_vertices;
		block_meshes = &tile_textures.block_meshes;

		chunk_data = chunk->blocks.get();

		// reserve enough mesh memory for the average chunk to avoid reallocation
		int reserve = CHUNK_DIM * CHUNK_DIM * 6 * 6 * 4; // 64*64 blocks * 6 faces * 6 vertices * 4 
		float grow_fac = 4;

		*opaque_vertices	 = UnsafeVector<ChunkMesh::Vertex>(reserve, grow_fac);
		*tranparent_vertices = UnsafeVector<ChunkMesh::Vertex>(reserve, grow_fac);

		bpos chunk_pos_world = chunk->chunk_pos_world();

		//auto _a = get_timestamp();
		//uint64_t sum = 0;

		int3 i = 0;
		for (i.z=0; i.z<CHUNK_DIM; ++i.z) {
			for (i.y=0; i.y<CHUNK_DIM; ++i.y) {

				i.x = 0;
				cur = ChunkData::pos_to_index(i);

				for (; i.x<CHUNK_DIM; ++i.x) {

					auto id = chunk_data->id[cur];

					if (id != B_AIR) {
						//auto _b = get_timestamp();

						block_pos = (float3)i;

						tile = tile_textures.block_tile_info[id];
						auto mesh_info = tile_textures.block_meshes_info[id];

						if (mesh_info.offset < 0) {
							if (blocks.transparency[id] == TM_TRANSPARENT)
								cube_transperant();
							else
								cube_opaque();
						} else {

							block_mesh(mesh_info, i + chunk->chunk_pos_world(), wg.seed);
						}

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
void mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, TileTextures const& tile_textures, WorldGenerator const& wg, Chunk* chunk, MeshingResult* res) {
	OPTICK_EVENT();

	Chunk_Mesher cm;
	return cm.mesh_chunk(chunks, graphics, tile_textures, wg, chunk, res);
}
