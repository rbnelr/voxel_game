#include "chunk_mesher.hpp"

static constexpr int offs (int3 offset) {
	return offset.z * CHUNK_LAYER_OFFS + offset.y * CHUNK_ROW_OFFS + offset.x;
}

struct Chunk_Mesher {
	bool alpha_test;

	ChunkMesh::Vertex* opaque_vertices;
	ChunkMesh::Vertex* tranparent_vertices;

	Block* cur;

	// per block
	uint8v3 block_pos;

	BlockTileInfo tile;

	bool bt_is_opaque (block_id id) {
		auto t = blocks.transparency[id];
		if (t == TM_OPAQUE)
			return true;

		if (!alpha_test)
			return t == TM_ALPHA_TEST;

		return false;
	}

	static constexpr int offsets[8] = {
		offs(int3( 0, 0, 0)),
		offs(int3(-1, 0, 0)),
		offs(int3( 0,-1, 0)),
		offs(int3(-1,-1, 0)),
		offs(int3( 0, 0,-1)),
		offs(int3(-1, 0,-1)),
		offs(int3( 0,-1,-1)),
		offs(int3(-1,-1,-1)),
	};

	inline uint8 calc_vertex_AO (BlockFace face, int3 vert_pos) {
		auto* ptr = cur + offs(vert_pos);
		
		int total = 0;

		for (int i=0; i<8; ++i) {
			total += blocks.collision[ (ptr + offsets[i])->id ] != CM_SOLID ? 1 : 0;
		}
		
		return (total * 255) / 8;
	}
	uint8 calc_block_light (BlockFace face) {
		static constexpr int offsets[] = {
			-1, +1,
			-CHUNK_ROW_OFFS, +CHUNK_ROW_OFFS,
			-CHUNK_LAYER_OFFS, +CHUNK_LAYER_OFFS
		};

		return (uint8)(cur + offsets[face])->block_light;
	}
	uint8 calc_sky_light (BlockFace face) {
		static constexpr int offsets[] = {
			-1, +1,
			-CHUNK_ROW_OFFS, +CHUNK_ROW_OFFS,
			-CHUNK_LAYER_OFFS, +CHUNK_LAYER_OFFS
		};

		return (uint8)(cur + offsets[face])->sky_light;
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

					if (blocks.transparency[cur->id] == TM_TRANSPARENT)
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

// uint8v3	pos_model;
// uint8v2	uv;
// uint8	tex_indx;
// uint8	block_light;
// uint8	sky_light;
// uint8	hp;

#define VERT(x,y,z, u,v, face) \
		{ block_pos + uint8v3(x,y,z), uint8v2(u,v), (uint8)tile.calc_texture_index(face), \
		  calc_block_light(face), calc_sky_light(face), calc_vertex_AO(face, int3(x,y,z)), cur->hp }

#define QUAD(a,b,c,d)	do { \
			*out++ = a; *out++ = b; *out++ = d; \
			*out++ = d; *out++ = b; *out++ = c; \
		} while (0)
#define QUAD_ALTERNATE(a,b,c,d)	do { \
			*out++ = b; *out++ = c; *out++ = a; \
			*out++ = a; *out++ = c; *out++ = d; \
		} while (0)

#define FACE \
		if (vert[0].vertex_AO +vert[2].vertex_AO < vert[1].vertex_AO +vert[3].vertex_AO) \
			QUAD(vert[0], vert[1], vert[2], vert[3]); \
		else \
			QUAD_ALTERNATE(vert[0], vert[1], vert[2], vert[3]);
//#define FACE QUAD(vert[0], vert[1], vert[2], vert[3]);

ChunkMesh::Vertex* Chunk_Mesher::face_nx (ChunkMesh::Vertex* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(0,1,0, 0,0, BF_NEG_X),
		VERT(0,0,0, 1,0, BF_NEG_X),
		VERT(0,0,1, 1,1, BF_NEG_X),
		VERT(0,1,1, 0,1, BF_NEG_X),
	};
 	FACE
	return out;
}
ChunkMesh::Vertex* Chunk_Mesher::face_px (ChunkMesh::Vertex* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(1,0,0, 0,0, BF_POS_X),
		VERT(1,1,0, 1,0, BF_POS_X),
		VERT(1,1,1, 1,1, BF_POS_X),
		VERT(1,0,1, 0,1, BF_POS_X),
	};
	FACE
	return out;
}
ChunkMesh::Vertex* Chunk_Mesher::face_ny (ChunkMesh::Vertex* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(0,0,0, 0,0, BF_NEG_Y),
		VERT(1,0,0, 1,0, BF_NEG_Y),
		VERT(1,0,1, 1,1, BF_NEG_Y),
		VERT(0,0,1, 0,1, BF_NEG_Y),
	};
	FACE
	return out;
}
ChunkMesh::Vertex* Chunk_Mesher::face_py (ChunkMesh::Vertex* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(1,1,0, 0,0, BF_POS_Y),
		VERT(0,1,0, 1,0, BF_POS_Y),
		VERT(0,1,1, 1,1, BF_POS_Y),
		VERT(1,1,1, 0,1, BF_POS_Y),
	};
	FACE
	return out;
}
ChunkMesh::Vertex* Chunk_Mesher::face_nz (ChunkMesh::Vertex* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(0,1,0, 0,0, BF_NEG_Z),
		VERT(1,1,0, 1,0, BF_NEG_Z),
		VERT(1,0,0, 1,1, BF_NEG_Z),
		VERT(0,0,0, 0,1, BF_NEG_Z),
	};
	FACE
	return out;
}
ChunkMesh::Vertex* Chunk_Mesher::face_pz (ChunkMesh::Vertex* out) {
	ChunkMesh::Vertex vert[4] = {
		VERT(0,0,1, 0,0, BF_POS_Z),
		VERT(1,0,1, 1,0, BF_POS_Z),
		VERT(1,1,1, 1,1, BF_POS_Z),
		VERT(0,1,1, 0,1, BF_POS_Z),
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
