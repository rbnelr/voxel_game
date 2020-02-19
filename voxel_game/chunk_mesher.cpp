#include "chunk_mesher.hpp"

struct Chunk_Mesher {
	Chunk const& chunk;

	bool alpha_test;

	std::vector<ChunkMesh::Vertex>* opaque_vertices;
	std::vector<ChunkMesh::Vertex>* tranparent_vertices;

	// per block
	Block b;

	uint8 block_pos_x;
	uint8 block_pos_y;
	uint8 block_pos_z;

	BlockTileInfo tile;

	bool bt_is_opaque (block_id id) {
		auto t = BLOCK_PROPS[id].transparency;
		if (t == TM_OPAQUE)
			return true;

		if (!alpha_test)
			return t == TM_ALPHA_TEST;

		return false;
	}

	void face_nx (std::vector<ChunkMesh::Vertex>* verts);
	void face_px (std::vector<ChunkMesh::Vertex>* verts);
	void face_ny (std::vector<ChunkMesh::Vertex>* verts);
	void face_py (std::vector<ChunkMesh::Vertex>* verts);
	void face_nz (std::vector<ChunkMesh::Vertex>* verts);
	void face_pz (std::vector<ChunkMesh::Vertex>* verts);

	void cube_opaque ();
	void cube_transperant ();

	MeshingResult mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, TileTextures const& tile_textures, Chunk* chunk);
};

MeshingResult mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, TileTextures const& tile_textures, Chunk* chunk) {
	Chunk_Mesher cm = { *chunk };
	return cm.mesh_chunk(chunks, graphics, tile_textures, chunk);
}

MeshingResult Chunk_Mesher::mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, TileTextures const& tile_textures, Chunk* chunk) {
	MeshingResult res;

	alpha_test = graphics.alpha_test;

	opaque_vertices = &res.opaque_vertices;
	tranparent_vertices = &res.tranparent_vertices;

	{
		bpos i;
		for (i.z=0; i.z<CHUNK_DIM_Z; ++i.z) {
			for (i.y=0; i.y<CHUNK_DIM_Y; ++i.y) {
				for (i.x=0; i.x<CHUNK_DIM_X; ++i.x) {
					auto& block = chunk->get_block(i);

					if (block.id != B_AIR) {

						b = block;
						block_pos_x = (uint8)i.x;
						block_pos_y = (uint8)i.y;
						block_pos_z = (uint8)i.z;
						tile = tile_textures.block_tile_info[b.id];

						if (BLOCK_PROPS[block.id].transparency == TM_TRANSPARENT)
							cube_transperant();
						else
							cube_opaque();

					}
				}
			}
		}
	}

	return res;
}

#define XL (block_pos_x)
#define YL (block_pos_y)
#define ZL (block_pos_z)
#define XH (block_pos_x +1)
#define YH (block_pos_y +1)
#define ZH (block_pos_z +1)

// uint8v3	pos_model;
// uint8v2	uv;
// uint8	tex_indx;
// uint8	light_level;
// uint8	hp;

#define VERT(x,y,z, u,v, face) \
		{ uint8v3(x,y,z), uint8v2(u,v), (uint8)tile.calc_texture_index(face), b.light_level, b.hp }

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

void Chunk_Mesher::face_nx (std::vector<ChunkMesh::Vertex>* verts) {
	verts->resize(verts->size() + 6);
	ChunkMesh::Vertex* out = &(*verts)[verts->size() - 6];

	ChunkMesh::Vertex vert[4] = {
		VERT(XL,YH,ZL, 0,0, BF_NEG_X),
		VERT(XL,YL,ZL, 1,0, BF_NEG_X),
		VERT(XL,YL,ZH, 1,1, BF_NEG_X),
		VERT(XL,YH,ZH, 0,1, BF_NEG_X),
	};
	FACE
}
void Chunk_Mesher::face_px (std::vector<ChunkMesh::Vertex>* verts) {
	verts->resize(verts->size() + 6);
	ChunkMesh::Vertex* out = &(*verts)[verts->size() - 6];

	ChunkMesh::Vertex vert[4] = {
		VERT(XH,YL,ZL, 0,0, BF_POS_X),
		VERT(XH,YH,ZL, 1,0, BF_POS_X),
		VERT(XH,YH,ZH, 1,1, BF_POS_X),
		VERT(XH,YL,ZH, 0,1, BF_POS_X),
	};
	FACE
}
void Chunk_Mesher::face_ny (std::vector<ChunkMesh::Vertex>* verts) {
	verts->resize(verts->size() + 6);
	ChunkMesh::Vertex* out = &(*verts)[verts->size() - 6];

	ChunkMesh::Vertex vert[4] = {
		VERT(XL,YL,ZL, 0,0, BF_NEG_Y),
		VERT(XH,YL,ZL, 1,0, BF_NEG_Y),
		VERT(XH,YL,ZH, 1,1, BF_NEG_Y),
		VERT(XL,YL,ZH, 0,1, BF_NEG_Y),
	};
	FACE
}
void Chunk_Mesher::face_py (std::vector<ChunkMesh::Vertex>* verts) {
	verts->resize(verts->size() + 6);
	ChunkMesh::Vertex* out = &(*verts)[verts->size() - 6];

	ChunkMesh::Vertex vert[4] = {
		VERT(XH,YH,ZL, 0,0, BF_POS_Y),
		VERT(XL,YH,ZL, 1,0, BF_POS_Y),
		VERT(XL,YH,ZH, 1,1, BF_POS_Y),
		VERT(XH,YH,ZH, 0,1, BF_POS_Y),
	};
	FACE
}
void Chunk_Mesher::face_nz (std::vector<ChunkMesh::Vertex>* verts) {
	verts->resize(verts->size() + 6);
	ChunkMesh::Vertex* out = &(*verts)[verts->size() - 6];

	ChunkMesh::Vertex vert[4] = {
		VERT(XL,YH,ZL, 0,0, BF_NEG_Z),
		VERT(XH,YH,ZL, 1,0, BF_NEG_Z),
		VERT(XH,YL,ZL, 1,1, BF_NEG_Z),
		VERT(XL,YL,ZL, 0,1, BF_NEG_Z),
	};
	FACE
}
void Chunk_Mesher::face_pz (std::vector<ChunkMesh::Vertex>* verts) {
	verts->resize(verts->size() + 6);
	ChunkMesh::Vertex* out = &(*verts)[verts->size() - 6];

	ChunkMesh::Vertex vert[4] = {
		VERT(XL,YL,ZH, 0,0, BF_POS_Z),
		VERT(XH,YL,ZH, 1,0, BF_POS_Z),
		VERT(XH,YH,ZH, 1,1, BF_POS_Z),
		VERT(XL,YH,ZH, 0,1, BF_POS_Z),
	};
	FACE
}

void Chunk_Mesher::cube_opaque () {
	if (!bt_is_opaque(chunk.get_block(block_pos_x -1, block_pos_y, block_pos_z).id)) face_nx(opaque_vertices);
	if (!bt_is_opaque(chunk.get_block(block_pos_x +1, block_pos_y, block_pos_z).id)) face_px(opaque_vertices);
	if (!bt_is_opaque(chunk.get_block(block_pos_x, block_pos_y -1, block_pos_z).id)) face_ny(opaque_vertices);
	if (!bt_is_opaque(chunk.get_block(block_pos_x, block_pos_y +1, block_pos_z).id)) face_py(opaque_vertices);
	if (!bt_is_opaque(chunk.get_block(block_pos_x, block_pos_y, block_pos_z -1).id)) face_nz(opaque_vertices);
	if (!bt_is_opaque(chunk.get_block(block_pos_x, block_pos_y, block_pos_z +1).id)) face_pz(opaque_vertices);
};
void Chunk_Mesher::cube_transperant () {
	block_id bt;

	bt = chunk.get_block(block_pos_x -1, block_pos_y, block_pos_z).id;
	if (!bt_is_opaque(bt) && bt != b.id) face_nx(tranparent_vertices);

	bt = chunk.get_block(block_pos_x +1, block_pos_y, block_pos_z).id;
	if (!bt_is_opaque(bt) && bt != b.id) face_px(tranparent_vertices);

	bt = chunk.get_block(block_pos_x, block_pos_y -1, block_pos_z).id;
	if (!bt_is_opaque(bt) && bt != b.id) face_ny(tranparent_vertices);

	bt = chunk.get_block(block_pos_x, block_pos_y +1, block_pos_z).id;
	if (!bt_is_opaque(bt) && bt != b.id) face_py(tranparent_vertices);

	bt = chunk.get_block(block_pos_x, block_pos_y, block_pos_z -1).id;
	if (!bt_is_opaque(bt) && bt != b.id) face_nz(tranparent_vertices);

	bt = chunk.get_block(block_pos_x, block_pos_y, block_pos_z +1).id;
	if (!bt_is_opaque(bt) && bt != b.id) face_pz(tranparent_vertices);
};
