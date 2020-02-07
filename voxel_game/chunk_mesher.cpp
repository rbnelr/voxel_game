#include "chunk_mesher.hpp"

#define AVOID_QUERY_CHUNK_HASH_LOOKUP 1

struct Chunk_Mesher {

	bool alpha_test;

	std::vector<ChunkMesh::Vertex>* opaque_vertices;
	std::vector<ChunkMesh::Vertex>* tranparent_vertices;

#if AVOID_QUERY_CHUNK_HASH_LOOKUP
	Chunk* neighbour_chunks[3][3];

	Block const* query_block (bpos_t pos_x, bpos_t pos_y, bpos_t pos_z) {
		if (pos_z < 0 || pos_z >= CHUNK_DIM_Z)
			return &B_OUT_OF_BOUNDS;

		bpos pos_in_chunk;
		chunk_coord neighbour = get_chunk_from_block_pos(bpos(pos_x, pos_y, pos_z), &pos_in_chunk); // pass in local block pos as world block pos, get -1,-1 to +1,+1 coords for our neighbour chunks instead of world chunk coords

		Chunk* chunk = neighbour_chunks[neighbour.y +1][neighbour.x +1];
		if (!chunk) return &B_NO_CHUNK;

		return chunk->get_block(pos_in_chunk);
	}
	Block const* query_block (bpos p) {
		return query_block(p.x, p.y, p.z);
	}
#else
	Block const* query_block (bpos_t pos_x, bpos_t pos_y, bpos_t pos_z) {
		return ::query_block(bpos(pos_x,pos_y,pos_z));
	}
	Block const* query_block (bpos p) {
		return ::query_block(p);
	}
#endif

	// per block
	Block const* b;

	bpos_t block_pos_x;
	bpos_t block_pos_y;
	bpos_t block_pos_z;

	BlockTileInfo tile;

	float hp;

	float calc_brightness (bpos vert_pos, bpos axis_a, bpos axis_b, bpos plane);

	bool bt_is_non_opaque (block_type t) {
		if (alpha_test)
			return block_props[t].transparency != TM_OPAQUE;
		else
			return block_props[t].transparency == TM_TRANSPARENT;
	}

	float calc_texture_index (BlockFace face) {
		int index = tile.base_index;
		if (face == BF_POS_Z)
			index += tile.top;
		else if (face == BF_NEG_Z)
			index += tile.bottom;
		return (float)index;
	}

	void face_nx (std::vector<ChunkMesh::Vertex>* verts);
	void face_px (std::vector<ChunkMesh::Vertex>* verts);
	void face_ny (std::vector<ChunkMesh::Vertex>* verts);
	void face_py (std::vector<ChunkMesh::Vertex>* verts);
	void face_nz (std::vector<ChunkMesh::Vertex>* verts);
	void face_pz (std::vector<ChunkMesh::Vertex>* verts);

	void cube_opaque ();
	void cube_transperant ();

	void mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, Chunk* chunk);
};

void mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, Chunk* chunk) {
	Chunk_Mesher cm;
	cm.mesh_chunk(chunks, graphics, chunk);
}

void Chunk_Mesher::mesh_chunk (Chunks& chunks, ChunkGraphics const& graphics, Chunk* chunk) {
	alpha_test = graphics.alpha_test;

	opaque_vertices = &chunk->mesh.opaque_faces;
	tranparent_vertices = &chunk->mesh.transparent_faces;

	opaque_vertices->clear();
	tranparent_vertices->clear();

#if AVOID_QUERY_CHUNK_HASH_LOOKUP
	neighbour_chunks[0][0] = chunks.query_chunk(chunk->coord +chunk_coord(-1,-1));
	neighbour_chunks[0][1] = chunks.query_chunk(chunk->coord +chunk_coord( 0,-1));
	neighbour_chunks[0][2] = chunks.query_chunk(chunk->coord +chunk_coord(+1,-1));
	neighbour_chunks[1][0] = chunks.query_chunk(chunk->coord +chunk_coord(-1, 0));
	neighbour_chunks[1][1] = chunk                                          ;
	neighbour_chunks[1][2] = chunks.query_chunk(chunk->coord +chunk_coord(+1, 0));
	neighbour_chunks[2][0] = chunks.query_chunk(chunk->coord +chunk_coord(-1,+1));
	neighbour_chunks[2][1] = chunks.query_chunk(chunk->coord +chunk_coord( 0,+1));
	neighbour_chunks[2][2] = chunks.query_chunk(chunk->coord +chunk_coord(+1,+1));
#endif

	{
		bpos i;
		for (i.z=0; i.z<CHUNK_DIM_Z; ++i.z) {
			for (i.y=0; i.y<CHUNK_DIM_Y; ++i.y) {
				for (i.x=0; i.x<CHUNK_DIM_X; ++i.x) {
					auto* block = chunk->get_block(i);

					if (block->type != BT_AIR) {

						b = block;
						block_pos_x = i.x;
						block_pos_y = i.y;
						block_pos_z = i.z;
						tile = graphics.tile_textures.block_tile_info[b->type];
						hp = (float)b->hp / 255.0f;

						if (block_props[block->type].transparency == TM_TRANSPARENT)
							cube_transperant();
						else
							cube_opaque();

					}
				}
			}
		}
	}

	chunk->mesh.opaque_mesh.upload(chunk->mesh.opaque_faces);
	chunk->mesh.transparent_mesh.upload(chunk->mesh.transparent_faces);
}

float Chunk_Mesher::calc_brightness (bpos vert_pos, bpos axis_a, bpos axis_b, bpos plane) {
	int brightness = 0;

	brightness += query_block(vert_pos +plane -axis_a      +0)->dark ? 0 : 1;
	brightness += query_block(vert_pos +plane      +0      +0)->dark ? 0 : 1;
	brightness += query_block(vert_pos +plane -axis_a -axis_b)->dark ? 0 : 1;
	brightness += query_block(vert_pos +plane      +0 -axis_b)->dark ? 0 : 1;

	static constexpr float LUT[] = { 0.02f, 0.08f, 0.3f, 0.6f, 1.0f };

	return LUT[brightness];
}

#define XL (block_pos_x)
#define YL (block_pos_y)
#define ZL (block_pos_z)
#define XH (block_pos_x +1)
#define YH (block_pos_y +1)
#define ZH (block_pos_z +1)

// float3	pos_model;
// float	brightness;
// float4	uvz_hp; // xy: [0,1] face uv; z: texture index, w: hp_ratio [0,1]

#define VERT(x,y,z, u,v, face, axis_a,axis_b, plane) \
		{ (float3)bpos(x,y,z), calc_brightness(bpos(x,y,z), axis_a,axis_b,plane), float4(u,v, calc_texture_index(face), hp) }

#define QUAD(a,b,c,d)	do { \
			*out++ = a; *out++ = b; *out++ = d; \
			*out++ = d; *out++ = b; *out++ = c; \
		} while (0)
#define QUAD_ALTERNATE(a,b,c,d)	do { \
			*out++ = b; *out++ = c; *out++ = a; \
			*out++ = a; *out++ = c; *out++ = d; \
		} while (0)

#define FACE \
		if (vert[0].brightness +vert[2].brightness < vert[1].brightness +vert[3].brightness) \
			QUAD(vert[0], vert[1], vert[2], vert[3]); \
		else \
			QUAD_ALTERNATE(vert[0], vert[1], vert[2], vert[3]);

void Chunk_Mesher::face_nx (std::vector<ChunkMesh::Vertex>* verts) {
	verts->resize(verts->size() + 6);
	ChunkMesh::Vertex* out = &(*verts)[verts->size() - 6];

	ChunkMesh::Vertex vert[4] = {
		VERT(XL,YH,ZL, 0,0, BF_NEG_X, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
		VERT(XL,YL,ZL, 1,0, BF_NEG_X, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
		VERT(XL,YL,ZH, 1,1, BF_NEG_X, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
		VERT(XL,YH,ZH, 0,1, BF_NEG_X, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
	};
	FACE
}
void Chunk_Mesher::face_px (std::vector<ChunkMesh::Vertex>* verts) {
	verts->resize(verts->size() + 6);
	ChunkMesh::Vertex* out = &(*verts)[verts->size() - 6];

	ChunkMesh::Vertex vert[4] = {
		VERT(XH,YL,ZL, 0,0, BF_POS_X, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
		VERT(XH,YH,ZL, 1,0, BF_POS_X, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
		VERT(XH,YH,ZH, 1,1, BF_POS_X, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
		VERT(XH,YL,ZH, 0,1, BF_POS_X, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
	};
	FACE
}
void Chunk_Mesher::face_ny (std::vector<ChunkMesh::Vertex>* verts) {
	verts->resize(verts->size() + 6);
	ChunkMesh::Vertex* out = &(*verts)[verts->size() - 6];

	ChunkMesh::Vertex vert[4] = {
		VERT(XL,YL,ZL, 0,0, BF_NEG_Y, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
		VERT(XH,YL,ZL, 1,0, BF_NEG_Y, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
		VERT(XH,YL,ZH, 1,1, BF_NEG_Y, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
		VERT(XL,YL,ZH, 0,1, BF_NEG_Y, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
	};
	FACE
}
void Chunk_Mesher::face_py (std::vector<ChunkMesh::Vertex>* verts) {
	verts->resize(verts->size() + 6);
	ChunkMesh::Vertex* out = &(*verts)[verts->size() - 6];

	ChunkMesh::Vertex vert[4] = {
		VERT(XH,YH,ZL, 0,0, BF_POS_Y, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
		VERT(XL,YH,ZL, 1,0, BF_POS_Y, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
		VERT(XL,YH,ZH, 1,1, BF_POS_Y, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
		VERT(XH,YH,ZH, 0,1, BF_POS_Y, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
	};
	FACE
}
void Chunk_Mesher::face_nz (std::vector<ChunkMesh::Vertex>* verts) {
	verts->resize(verts->size() + 6);
	ChunkMesh::Vertex* out = &(*verts)[verts->size() - 6];

	ChunkMesh::Vertex vert[4] = {
		VERT(XL,YH,ZL, 0,0, BF_NEG_Z, bpos(1,0,0),bpos(0,1,0), bpos(0,0,-1)),
		VERT(XH,YH,ZL, 1,0, BF_NEG_Z, bpos(1,0,0),bpos(0,1,0), bpos(0,0,-1)),
		VERT(XH,YL,ZL, 1,1, BF_NEG_Z, bpos(1,0,0),bpos(0,1,0), bpos(0,0,-1)),
		VERT(XL,YL,ZL, 0,1, BF_NEG_Z, bpos(1,0,0),bpos(0,1,0), bpos(0,0,-1)),
	};
	FACE
}
void Chunk_Mesher::face_pz (std::vector<ChunkMesh::Vertex>* verts) {
	verts->resize(verts->size() + 6);
	ChunkMesh::Vertex* out = &(*verts)[verts->size() - 6];

	ChunkMesh::Vertex vert[4] = {
		VERT(XL,YL,ZH, 0,0, BF_POS_Z, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
		VERT(XH,YL,ZH, 1,0, BF_POS_Z, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
		VERT(XH,YH,ZH, 1,1, BF_POS_Z, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
		VERT(XL,YH,ZH, 0,1, BF_POS_Z, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
	};
	FACE
}

void Chunk_Mesher::cube_opaque () {
	if (bt_is_non_opaque(query_block(block_pos_x -1, block_pos_y, block_pos_z)->type)) face_nx(opaque_vertices);
	if (bt_is_non_opaque(query_block(block_pos_x +1, block_pos_y, block_pos_z)->type)) face_px(opaque_vertices);
	if (bt_is_non_opaque(query_block(block_pos_x, block_pos_y -1, block_pos_z)->type)) face_ny(opaque_vertices);
	if (bt_is_non_opaque(query_block(block_pos_x, block_pos_y +1, block_pos_z)->type)) face_py(opaque_vertices);
	if (bt_is_non_opaque(query_block(block_pos_x, block_pos_y, block_pos_z -1)->type)) face_nz(opaque_vertices);
	if (bt_is_non_opaque(query_block(block_pos_x, block_pos_y, block_pos_z +1)->type)) face_pz(opaque_vertices);
};
void Chunk_Mesher::cube_transperant () {
	block_type bt;

	bt = query_block(block_pos_x -1, block_pos_y, block_pos_z)->type;
	if (bt_is_non_opaque(bt) && bt != b->type) face_nx(tranparent_vertices);

	bt = query_block(block_pos_x +1, block_pos_y, block_pos_z)->type;
	if (bt_is_non_opaque(bt) && bt != b->type) face_px(tranparent_vertices);

	bt = query_block(block_pos_x, block_pos_y -1, block_pos_z)->type;
	if (bt_is_non_opaque(bt) && bt != b->type) face_ny(tranparent_vertices);

	bt = query_block(block_pos_x, block_pos_y +1, block_pos_z)->type;
	if (bt_is_non_opaque(bt) && bt != b->type) face_py(tranparent_vertices);

	bt = query_block(block_pos_x, block_pos_y, block_pos_z -1)->type;
	if (bt_is_non_opaque(bt) && bt != b->type) face_nz(tranparent_vertices);

	bt = query_block(block_pos_x, block_pos_y, block_pos_z +1)->type;
	if (bt_is_non_opaque(bt) && bt != b->type) face_pz(tranparent_vertices);
};
