#include "chunk_mesher.hpp"

struct Chunk_Mesher {
	bool alpha_test;

	ChunkMesh::Vertex* opaque_vertices;
	ChunkMesh::Vertex* tranparent_vertices;

	Block* cur;

	// per block
	uint8v3 block_pos;

	BlockTileInfo tile;

	bool bt_is_opaque (block_id id) {
		auto t = BLOCK_PROPS[id].transparency;
		if (t == TM_OPAQUE)
			return true;

		if (!alpha_test)
			return t == TM_ALPHA_TEST;

		return false;
	}

	uint8 find_light_level (BlockFace face) {
		static constexpr int offsets[] = {
			-1, +1,
			-CHUNK_ROW_OFFS, +CHUNK_ROW_OFFS,
			-CHUNK_LAYER_OFFS, +CHUNK_LAYER_OFFS
		};
		
		return (cur + offsets[face])->light_level;
	}

	ChunkMesh::Vertex* face_nx (ChunkMesh::Vertex* verts);
	ChunkMesh::Vertex* face_px (ChunkMesh::Vertex* verts);
	ChunkMesh::Vertex* face_ny (ChunkMesh::Vertex* verts);
	ChunkMesh::Vertex* face_py (ChunkMesh::Vertex* verts);
	ChunkMesh::Vertex* face_nz (ChunkMesh::Vertex* verts);
	ChunkMesh::Vertex* face_pz (ChunkMesh::Vertex* verts);

	void cube_opaque ();
	void cube_transperant ();

	void mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, TileTextures const& tile_textures, Chunk* chunk, MeshingResult* res);
};
void mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, TileTextures const& tile_textures, Chunk* chunk, MeshingResult* res) {
	Chunk_Mesher cm;
	return cm.mesh_chunk(chunks, graphics, tile_textures, chunk, res);
}

void Chunk_Mesher::mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, TileTextures const& tile_textures, Chunk* chunk, MeshingResult* res) {
	alpha_test = graphics.alpha_test;

	opaque_vertices = res->opaque_vertices.ptr;
	tranparent_vertices = res->tranparent_vertices.ptr;

	for (block_pos.z=0; block_pos.z<CHUNK_DIM_Z; ++block_pos.z) {
		for (block_pos.y=0; block_pos.y<CHUNK_DIM_Y; ++block_pos.y) {

			cur = &chunk->blocks[block_pos.z + 1][block_pos.y + 1][1];

			for (block_pos.x=0; block_pos.x<CHUNK_DIM_X; ++block_pos.x) {

				if (cur->id != B_AIR) {
					tile = tile_textures.block_tile_info[cur->id];

					if (BLOCK_PROPS[cur->id].transparency == TM_TRANSPARENT)
						cube_transperant();
					else
						cube_opaque();
				}

				cur++;
			}
		}
	}

	res->opaque_count = (unsigned)(opaque_vertices - res->opaque_vertices.ptr);
	res->tranparent_count = (unsigned)(tranparent_vertices - res->tranparent_vertices.ptr);
}

#define XL (block_pos.x)
#define YL (block_pos.y)
#define ZL (block_pos.z)
#define XH (block_pos.x +1)
#define YH (block_pos.y +1)
#define ZH (block_pos.z +1)

// uint8v3	pos_model;
// uint8v2	uv;
// uint8	tex_indx;
// uint8	light_level;
// uint8	hp;

#define VERT(x,y,z, u,v, face) \
		{ uint8v3(x,y,z), uint8v2(u,v), (uint8)tile.calc_texture_index(face), find_light_level(face), cur->hp }

#define QUAD(a,b,c,d)	do { \
			*out++ = a; *out++ = b; *out++ = d; \
			*out++ = d; *out++ = b; *out++ = c; \
		} while (0)
#define QUAD_ALTERNATE(a,b,c,d)	do { \
			*out++ = b; *out++ = c; *out++ = a; \
			*out++ = a; *out++ = c; *out++ = d; \
		} while (0)

//#define FACE \
//		if (vert[0].light_level +vert[2].light_level < vert[1].light_level +vert[3].light_level) \
//			QUAD(vert[0], vert[1], vert[2], vert[3]); \
//		else \
//			QUAD_ALTERNATE(vert[0], vert[1], vert[2], vert[3]);
#define FACE QUAD(vert[0], vert[1], vert[2], vert[3]);

ChunkMesh::Vertex* Chunk_Mesher::face_nx (ChunkMesh::Vertex* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(XL,YH,ZL, 0,0, BF_NEG_X),
		VERT(XL,YL,ZL, 1,0, BF_NEG_X),
		VERT(XL,YL,ZH, 1,1, BF_NEG_X),
		VERT(XL,YH,ZH, 0,1, BF_NEG_X),
	};
	FACE
	return out;
}
ChunkMesh::Vertex* Chunk_Mesher::face_px (ChunkMesh::Vertex* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(XH,YL,ZL, 0,0, BF_POS_X),
		VERT(XH,YH,ZL, 1,0, BF_POS_X),
		VERT(XH,YH,ZH, 1,1, BF_POS_X),
		VERT(XH,YL,ZH, 0,1, BF_POS_X),
	};
	FACE
	return out;
}
ChunkMesh::Vertex* Chunk_Mesher::face_ny (ChunkMesh::Vertex* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(XL,YL,ZL, 0,0, BF_NEG_Y),
		VERT(XH,YL,ZL, 1,0, BF_NEG_Y),
		VERT(XH,YL,ZH, 1,1, BF_NEG_Y),
		VERT(XL,YL,ZH, 0,1, BF_NEG_Y),
	};
	FACE
	return out;
}
ChunkMesh::Vertex* Chunk_Mesher::face_py (ChunkMesh::Vertex* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(XH,YH,ZL, 0,0, BF_POS_Y),
		VERT(XL,YH,ZL, 1,0, BF_POS_Y),
		VERT(XL,YH,ZH, 1,1, BF_POS_Y),
		VERT(XH,YH,ZH, 0,1, BF_POS_Y),
	};
	FACE
	return out;
}
ChunkMesh::Vertex* Chunk_Mesher::face_nz (ChunkMesh::Vertex* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(XL,YH,ZL, 0,0, BF_NEG_Z),
		VERT(XH,YH,ZL, 1,0, BF_NEG_Z),
		VERT(XH,YL,ZL, 1,1, BF_NEG_Z),
		VERT(XL,YL,ZL, 0,1, BF_NEG_Z),
	};
	FACE
	return out;
}
ChunkMesh::Vertex* Chunk_Mesher::face_pz (ChunkMesh::Vertex* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(XL,YL,ZH, 0,0, BF_POS_Z),
		VERT(XH,YL,ZH, 1,0, BF_POS_Z),
		VERT(XH,YH,ZH, 1,1, BF_POS_Z),
		VERT(XL,YH,ZH, 0,1, BF_POS_Z),
	};
	FACE
	return out;
}

void Chunk_Mesher::cube_opaque () {
	if (!bt_is_opaque((cur -                1)->id)) opaque_vertices = face_nx(opaque_vertices);
	if (!bt_is_opaque((cur +                1)->id)) opaque_vertices = face_px(opaque_vertices);
	if (!bt_is_opaque((cur -   CHUNK_ROW_OFFS)->id)) opaque_vertices = face_ny(opaque_vertices);
	if (!bt_is_opaque((cur +   CHUNK_ROW_OFFS)->id)) opaque_vertices = face_py(opaque_vertices);
	if (!bt_is_opaque((cur - CHUNK_LAYER_OFFS)->id)) opaque_vertices = face_nz(opaque_vertices);
	if (!bt_is_opaque((cur + CHUNK_LAYER_OFFS)->id)) opaque_vertices = face_pz(opaque_vertices);
};
void Chunk_Mesher::cube_transperant () {
	block_id bt;

	bt = (cur -                1)->id;
	if (!bt_is_opaque(bt) && bt != cur->id) tranparent_vertices = face_nx(tranparent_vertices);

	bt = (cur +                1)->id;
	if (!bt_is_opaque(bt) && bt != cur->id) tranparent_vertices = face_px(tranparent_vertices);

	bt = (cur -   CHUNK_ROW_OFFS)->id;
	if (!bt_is_opaque(bt) && bt != cur->id) tranparent_vertices = face_ny(tranparent_vertices);

	bt = (cur +   CHUNK_ROW_OFFS)->id;
	if (!bt_is_opaque(bt) && bt != cur->id) tranparent_vertices = face_py(tranparent_vertices);

	bt = (cur - CHUNK_LAYER_OFFS)->id;
	if (!bt_is_opaque(bt) && bt != cur->id) tranparent_vertices = face_nz(tranparent_vertices);

	bt = (cur + CHUNK_LAYER_OFFS)->id;
	if (!bt_is_opaque(bt) && bt != cur->id) tranparent_vertices = face_pz(tranparent_vertices);
};
