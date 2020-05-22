#include "chunk_mesher.hpp"
#include "world_generator.hpp"

static constexpr int offs (int3 offset) {
	return offset.z * CHUNK_LAYER_OFFS + offset.y * CHUNK_ROW_OFFS + offset.x;
}

struct Chunk_Mesher {
	bool alpha_test;

	UnsafeVector<ChunkMesh::Vertex>* opaque_vertices;
	UnsafeVector<ChunkMesh::Vertex>* tranparent_vertices;

	Block* cur;

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
		auto* ptr = cur + offs(vert_pos);
		
		int total = 0;

		for (auto offset : face_offsets[face]) {
			total += (ptr + offset)->block_light;
		}
		
		return (total * 255) / (4 * MAX_LIGHT_LEVEL);
	}
	inline uint8 calc_sky_light (BlockFace face, int3 vert_pos) {
		auto* ptr = cur + offs(vert_pos);

		int total = 0;

		for (auto offset : face_offsets[face]) {
			total += (ptr + offset)->sky_light;
		}

		return (total * 255) / (4 * MAX_LIGHT_LEVEL);
	}

	void face_nx (UnsafeVector<ChunkMesh::Vertex>* out);
	void face_px (UnsafeVector<ChunkMesh::Vertex>* out);
	void face_ny (UnsafeVector<ChunkMesh::Vertex>* out);
	void face_py (UnsafeVector<ChunkMesh::Vertex>* out);
	void face_nz (UnsafeVector<ChunkMesh::Vertex>* out);
	void face_pz (UnsafeVector<ChunkMesh::Vertex>* out);

	void cube_opaque ();
	void cube_transperant ();
	void block_mesh (BlockMeshInfo info, int variant, float2 rand_offs);

	void mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, TileTextures const& tile_textures, WorldGenerator const& wg, Chunk* chunk, MeshingResult* res);
};
void mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, TileTextures const& tile_textures, WorldGenerator const& wg, Chunk* chunk, MeshingResult* res) {
	OPTICK_EVENT();

	Chunk_Mesher cm;
	return cm.mesh_chunk(chunks, graphics, tile_textures, wg, chunk, res);
}

void Chunk_Mesher::mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, TileTextures const& tile_textures, WorldGenerator const& wg, Chunk* chunk, MeshingResult* res) {
	alpha_test = graphics.alpha_test;

	opaque_vertices = &res->opaque_vertices;
	tranparent_vertices = &res->tranparent_vertices;
	block_meshes = &tile_textures.block_meshes;

	// reserve enough mesh memory for the average chunk to avoid reallocation
	int reserve = CHUNK_DIM * CHUNK_DIM * 6 * 6 * 4; // 64*64 blocks * 6 faces * 6 vertices * 4 
	float grow_fac = 4;

	*opaque_vertices	 = UnsafeVector<ChunkMesh::Vertex>(reserve, grow_fac);
	*tranparent_vertices = UnsafeVector<ChunkMesh::Vertex>(reserve, grow_fac);

	bpos chunk_pos_world = chunk->chunk_pos_world();

	int3 i = 0;
	for (i.z=0; i.z<CHUNK_DIM; ++i.z) {
		OPTICK_EVENT("z layer");
		OPTICK_TAG("z", i.z);

		for (i.y=0; i.y<CHUNK_DIM; ++i.y) {

			cur = &chunk->blocks[i.z + 1][i.y + 1][1];

			for (i.x=0; i.x<CHUNK_DIM; ++i.x) {

				block_pos = (float3)i;

				if (cur->id != B_AIR) {
					tile = tile_textures.block_tile_info[cur->id];

					// get a 'random' but deterministic value based on block position
					uint64_t h = hash(i + chunk_pos_world) ^ wg.seed;

					// get a random determinisitc 2d offset
					float2 rand_offs;
					rand_offs.x = (float)((h >>  0) & 0xffffffffull) / (float)((1ull << 32) - 1); // [0, 1]
					rand_offs.y = (float)((h >> 32) & 0xffffffffull) / (float)((1ull << 32) - 1); // [0, 1]
					rand_offs = rand_offs * 2 - 1; // [0,1] -> [-1,+1]

					// get a random deterministic variant
					float rand_val = (float)(h & 0xffffffffull) / (float)(1ull << 32); // [0, 1)
					int variant = tile.variants > 1 ? floori(rand_val * (float)tile.variants) : 0; // [0, tile.variants)

					if (tile_textures.block_meshes_info[cur->id].offset >= 0) {
						block_mesh(tile_textures.block_meshes_info[cur->id], variant, rand_offs);
					} else {
						if (blocks.transparency[cur->id] == TM_TRANSPARENT)
							cube_transperant();
						else
							cube_opaque();
					}
				}

				cur++;
			}
		}
	}
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

#define QUAD(a,b,c,d) \
			*ptr++ = a; *ptr++ = b; *ptr++ = d; \
			*ptr++ = d; *ptr++ = b; *ptr++ = c;
#define QUAD_ALTERNATE(a,b,c,d) \
			*ptr++ = b; *ptr++ = c; *ptr++ = a; \
			*ptr++ = a; *ptr++ = c; *ptr++ = d; 

#define FACE \
		size_t offs = out->size; \
		out->resize(offs + 6); \
		auto* ptr = &(*out)[offs]; \
		if (vert[0].sky_light + vert[2].sky_light < vert[1].sky_light + vert[3].sky_light) { \
			QUAD(vert[0], vert[1], vert[2], vert[3]); \
		} else { \
			QUAD_ALTERNATE(vert[0], vert[1], vert[2], vert[3]); \
		}
//#define FACE QUAD(vert[0], vert[1], vert[2], vert[3]);

void Chunk_Mesher::face_nx (UnsafeVector<ChunkMesh::Vertex>* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(0,1,0, 0,0, BF_NEG_X),
		VERT(0,0,0, 1,0, BF_NEG_X),
		VERT(0,0,1, 1,1, BF_NEG_X),
		VERT(0,1,1, 0,1, BF_NEG_X),
	};
 	FACE
}
void Chunk_Mesher::face_px (UnsafeVector<ChunkMesh::Vertex>* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(1,0,0, 0,0, BF_POS_X),
		VERT(1,1,0, 1,0, BF_POS_X),
		VERT(1,1,1, 1,1, BF_POS_X),
		VERT(1,0,1, 0,1, BF_POS_X),
	};
	FACE
}
void Chunk_Mesher::face_ny (UnsafeVector<ChunkMesh::Vertex>* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(0,0,0, 0,0, BF_NEG_Y),
		VERT(1,0,0, 1,0, BF_NEG_Y),
		VERT(1,0,1, 1,1, BF_NEG_Y),
		VERT(0,0,1, 0,1, BF_NEG_Y),
	};
	FACE
}
void Chunk_Mesher::face_py (UnsafeVector<ChunkMesh::Vertex>* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(1,1,0, 0,0, BF_POS_Y),
		VERT(0,1,0, 1,0, BF_POS_Y),
		VERT(0,1,1, 1,1, BF_POS_Y),
		VERT(1,1,1, 0,1, BF_POS_Y),
	};
	FACE
}
void Chunk_Mesher::face_nz (UnsafeVector<ChunkMesh::Vertex>* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(0,1,0, 0,0, BF_NEG_Z),
		VERT(1,1,0, 1,0, BF_NEG_Z),
		VERT(1,0,0, 1,1, BF_NEG_Z),
		VERT(0,0,0, 0,1, BF_NEG_Z),
	};
	FACE
}
void Chunk_Mesher::face_pz (UnsafeVector<ChunkMesh::Vertex>* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(0,0,1, 0,0, BF_POS_Z),
		VERT(1,0,1, 1,0, BF_POS_Z),
		VERT(1,1,1, 1,1, BF_POS_Z),
		VERT(0,1,1, 0,1, BF_POS_Z),
	};
	FACE
}

void Chunk_Mesher::cube_opaque () {
	if (!bt_is_opaque((cur -                1)->id)) face_nx(opaque_vertices);
	if (!bt_is_opaque((cur +                1)->id)) face_px(opaque_vertices);
	if (!bt_is_opaque((cur -   CHUNK_ROW_OFFS)->id)) face_ny(opaque_vertices);
	if (!bt_is_opaque((cur +   CHUNK_ROW_OFFS)->id)) face_py(opaque_vertices);
	if (!bt_is_opaque((cur - CHUNK_LAYER_OFFS)->id)) face_nz(opaque_vertices);
	if (!bt_is_opaque((cur + CHUNK_LAYER_OFFS)->id)) face_pz(opaque_vertices);
}
void Chunk_Mesher::cube_transperant () {
	block_id bt;

	bt = (cur -                1)->id;
	if (!bt_is_opaque(bt) && bt != cur->id) face_nx(tranparent_vertices);

	bt = (cur +                1)->id;
	if (!bt_is_opaque(bt) && bt != cur->id) face_px(tranparent_vertices);

	bt = (cur -   CHUNK_ROW_OFFS)->id;
	if (!bt_is_opaque(bt) && bt != cur->id) face_ny(tranparent_vertices);

	bt = (cur +   CHUNK_ROW_OFFS)->id;
	if (!bt_is_opaque(bt) && bt != cur->id) face_py(tranparent_vertices);

	bt = (cur - CHUNK_LAYER_OFFS)->id;
	if (!bt_is_opaque(bt) && bt != cur->id) face_nz(tranparent_vertices);

	bt = (cur + CHUNK_LAYER_OFFS)->id;
	if (!bt_is_opaque(bt) && bt != cur->id) face_pz(tranparent_vertices);
}
void Chunk_Mesher::block_mesh (BlockMeshInfo info, int variant, float2 rand_offs) {
	OPTICK_EVENT();

	size_t offs = opaque_vertices->size;
	opaque_vertices->resize(offs + info.size);
	auto* ptr = &(*opaque_vertices)[offs];

	for (int i=0; i<info.size; ++i) {
		auto v = (*block_meshes)[info.offset + i];

		ptr->pos_model = v.pos_model + block_pos + 0.5f + float3(rand_offs * 0.25f, 0);
		ptr->uv = v.uv * tile.uv_size + tile.uv_pos;

		ptr->tex_indx = tile.base_index + variant;
		ptr->block_light = cur->block_light * 255 / MAX_LIGHT_LEVEL;
		ptr->sky_light = cur->sky_light * 255 / MAX_LIGHT_LEVEL;
		ptr->hp = cur->hp;

		ptr++;
	}
}
