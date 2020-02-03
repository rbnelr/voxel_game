#pragma once
#include "kissmath.hpp"
#include "blocks.hpp"
#include "graphics/graphics.hpp"
#include "util/move_only_class.hpp"

#include "stdint.h"
#include <unordered_map>

//#include "graphics/graphics.hpp"

typedef int64_t	bpos_t;
typedef int64v2	bpos2;
typedef int64v3	bpos;

typedef int64_t	chunk_pos_t;
typedef int64v2	chunk_pos;

#define CHUNK_DIM_X			32
#define CHUNK_DIM_Y			32
#define CHUNK_DIM_Z			64
#define CHUNK_DIM_SHIFT_X	5 // for coord >> CHUNK_DIM_SHIFT
#define CHUNK_DIM_SHIFT_Y	5 // for coord >> CHUNK_DIM_SHIFT
#define CHUNK_DIM_SHIFT_Z	6 // for coord >> CHUNK_DIM_SHIFT
#define CHUNK_DIM			bpos(CHUNK_DIM_X, CHUNK_DIM_Y, CHUNK_DIM_Z)
#define CHUNK_DIM_2D		bpos2(CHUNK_DIM_X, CHUNK_DIM_Y)

inline chunk_pos get_chunk_from_block_pos (bpos2 pos_world) {

	chunk_pos chunk_pos;
	chunk_pos.x = pos_world.x >> CHUNK_DIM_SHIFT_X; // arithmetic shift right instead of divide because we want  -10 / 32  to be  -1 instead of 0
	chunk_pos.y = pos_world.y >> CHUNK_DIM_SHIFT_Y;

	return chunk_pos;
}
inline chunk_pos get_chunk_from_block_pos (bpos pos_world, bpos* bpos_in_chunk=nullptr) {

	chunk_pos chunk_pos = get_chunk_from_block_pos((bpos2)pos_world);

	if (bpos_in_chunk) {
		bpos_in_chunk->x = pos_world.x & ((1 << CHUNK_DIM_SHIFT_X) -1);
		bpos_in_chunk->y = pos_world.y & ((1 << CHUNK_DIM_SHIFT_Y) -1);
		bpos_in_chunk->z = pos_world.z;
	}

	return chunk_pos;
}

struct s64v2_hashmap {
	chunk_pos v;

	bool operator== (s64v2_hashmap const& r) const { // for hash map
		return v.x == r.v.x && v.y == r.v.y;
	}
};

inline size_t hash (chunk_pos v) {
	return 53 * (std::hash<int64_t>()(v.x) + 53) + std::hash<int64_t>()(v.y);
};

static_assert(sizeof(size_t) == 8, "");

namespace std {
	template<> struct hash<s64v2_hashmap> { // for hash map
		size_t operator() (s64v2_hashmap const& v) const {
			return ::hash(v.v);
		}
	};
}

class Chunk;

struct Chunks {

	// avoid hash map lookup most of the time, since a lot of query_chunk's are going to end up in the same chunk (in query_block of clustered blocks)
	// I did profile this at some point and it was measurably faster than just doing to hashmap lookup all the time
	Chunk* _prev_query_chunk = nullptr;

	// Chunk hashmap
	std::unordered_map<s64v2_hashmap, Chunk> chunks;

	Chunk* _lookup_chunk (chunk_pos coord);

	Chunk* query_chunk (chunk_pos coord);
	Block* query_block (bpos p, Chunk** out_chunk=nullptr);

	Chunk* create (chunk_pos chunk_pos);
	void remove (chunk_pos chunk_pos);

	void remesh_all ();
	void delete_all ();
};

#define AVOID_QUERY_CHUNK_HASH_LOOKUP 1

struct Chunk_Mesher {

	bool alpha_test;

	std::vector<ChunkMesh::Vertex>* opaque_vertices;
	std::vector<ChunkMesh::Vertex>* tranparent_vertices;

#if AVOID_QUERY_CHUNK_HASH_LOOKUP
	Chunk* neighbour_chunks[3][3];
	
	Block const* query_block (bpos_t pos_x, bpos_t pos_y, bpos_t pos_z);
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

	lrgba color;

	float brightness (bpos vert_pos, bpos axis_a, bpos axis_b, bpos plane) {
		int brightness = 0;
		
		brightness += query_block(vert_pos +plane -axis_a      +0)->dark ? 0 : 1;
		brightness += query_block(vert_pos +plane      +0      +0)->dark ? 0 : 1;
		brightness += query_block(vert_pos +plane -axis_a -axis_b)->dark ? 0 : 1;
		brightness += query_block(vert_pos +plane      +0 -axis_b)->dark ? 0 : 1;
		
		static constexpr float LUT[] = { 0.02f, 0.08f, 0.3f, 0.6f, 1.0f };

		return LUT[brightness];
	}
	
	void mesh (Chunks& chunks, ChunkGraphics& graphics, Chunk* chunk);

	#define XL (block_pos_x)
	#define YL (block_pos_y)
	#define ZL (block_pos_z)
	#define XH (block_pos_x +1)
	#define YH (block_pos_y +1)
	#define ZH (block_pos_z +1)

	// float3	pos_model;
	// float	brightness;
	// float4	uvz_hp; // xy: [0,1] face uv; z: texture index, w: hp_ratio [0,1]
	// lrgba	color;

	#define VERT(x,y,z, u,v, face, axis_a,axis_b, plane) \
		{ (float3)bpos(x,y,z), brightness(bpos(x,y,z), axis_a,axis_b,plane), float4(u,v, calc_texture_index(face), b->hp_ratio), color }
	
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
	
	float calc_texture_index (BlockFace face) {
		int index = tile.base_index;
		if (face == BF_POS_Z)
			index += tile.top;
		else if (face == BF_NEG_Z)
			index += tile.bottom;
		return (float)index;
	}

	void face_nx (std::vector<ChunkMesh::Vertex>* verts) {
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
	void face_px (std::vector<ChunkMesh::Vertex>* verts) {
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
	void face_ny (std::vector<ChunkMesh::Vertex>* verts) {
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
	void face_py (std::vector<ChunkMesh::Vertex>* verts) {
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
	void face_nz (std::vector<ChunkMesh::Vertex>* verts) {
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
	void face_pz (std::vector<ChunkMesh::Vertex>* verts) {
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
	
	#undef VERT
	#undef QUAD
	#undef QUAD_ALTERNATE
	#undef FACE
	
	#undef XL
	#undef YL
	#undef ZL
	#undef XH
	#undef YH
	#undef ZH

	bool bt_is_transparent (block_type t) {
		if (alpha_test)
			return block_props[t].transparency != TM_OPAQUE;
		else
			return block_props[t].transparency == TM_TRANSPARENT;
	}

	void cube_opaque () {
		if (bt_is_transparent(query_block(block_pos_x -1, block_pos_y, block_pos_z)->type)) face_nx(opaque_vertices);
		if (bt_is_transparent(query_block(block_pos_x +1, block_pos_y, block_pos_z)->type)) face_px(opaque_vertices);
		if (bt_is_transparent(query_block(block_pos_x, block_pos_y -1, block_pos_z)->type)) face_ny(opaque_vertices);
		if (bt_is_transparent(query_block(block_pos_x, block_pos_y +1, block_pos_z)->type)) face_py(opaque_vertices);
		if (bt_is_transparent(query_block(block_pos_x, block_pos_y, block_pos_z -1)->type)) face_nz(opaque_vertices);
		if (bt_is_transparent(query_block(block_pos_x, block_pos_y, block_pos_z +1)->type)) face_pz(opaque_vertices);
	};
	void cube_transperant () {
		block_type bt;
		
		bt = query_block(block_pos_x -1, block_pos_y, block_pos_z)->type;
		if (bt_is_transparent(bt) && bt != b->type) face_nx(tranparent_vertices);
		
		bt = query_block(block_pos_x +1, block_pos_y, block_pos_z)->type;
		if (bt_is_transparent(bt) && bt != b->type) face_px(tranparent_vertices);
		
		bt = query_block(block_pos_x, block_pos_y -1, block_pos_z)->type;
		if (bt_is_transparent(bt) && bt != b->type) face_ny(tranparent_vertices);
		
		bt = query_block(block_pos_x, block_pos_y +1, block_pos_z)->type;
		if (bt_is_transparent(bt) && bt != b->type) face_py(tranparent_vertices);
		
		bt = query_block(block_pos_x, block_pos_y, block_pos_z -1)->type;
		if (bt_is_transparent(bt) && bt != b->type) face_nz(tranparent_vertices);
		
		bt = query_block(block_pos_x, block_pos_y, block_pos_z +1)->type;
		if (bt_is_transparent(bt) && bt != b->type) face_pz(tranparent_vertices);
	};
	
};

class Chunk {
	NO_MOVE_COPY_CLASS(Chunk)
public:
	const chunk_pos coord;

	Chunk (chunk_pos coord): coord{coord} {
		
	}
	
	bool needs_remesh = true;
	bool needs_block_brighness_update = true;
	
	Block	blocks[CHUNK_DIM_Z][CHUNK_DIM_Y][CHUNK_DIM_X];
	
	ChunkMesh mesh;

	Block* get_block (bpos pos) {
		return &blocks[pos.z][pos.y][pos.x];
	}
	
	void update_block_brighness () {
		bpos i; // position in chunk
		for (i.y=0; i.y<CHUNK_DIM_Y; ++i.y) {
			for (i.x=0; i.x<CHUNK_DIM_X; ++i.x) {
				
				i.z = CHUNK_DIM_Z -1;
				
				for (; i.z > -1; --i.z) {
					auto* b = get_block(i);
					
					if (b->type != BT_AIR) break;
					
					b->dark = false;
				}
				
				for (; i.z > -1; --i.z) {
					auto* b = get_block(i);
					
					b->dark = true;
				}
				
			}
		}
		
		needs_block_brighness_update = false;
	}
	
	static bpos chunk_pos_world (chunk_pos coord) {
		return bpos(coord * CHUNK_DIM_2D, 0);
	}
	bpos chunk_pos_world () {
		return chunk_pos_world(coord);
	}
	
	void remesh (Chunks& chunks, ChunkGraphics& graphics) {
		Chunk_Mesher mesher;
		
		mesher.mesh(chunks, graphics, this);
		
		needs_remesh = false;
	}
	
	void block_only_texture_changed (bpos block_pos_world) { // only breaking animation of block changed -> do not need to update block brightness -> do not need to remesh surrounding chunks (only this chunk needs remesh)
		needs_remesh = true;
	}
	void block_changed (Chunks& chunks, bpos block_pos_world) { // block was placed or broken -> need to update our block brightness -> need to remesh surrounding chunks
		needs_remesh = true;
		needs_block_brighness_update = true;
		
		Chunk* chunk;
		
		bpos pos = block_pos_world -chunk_pos_world();
		
		bpos2 chunk_edge = select((bpos2)pos == bpos2(0), bpos2(-1), select((bpos2)pos == CHUNK_DIM_2D -1, bpos2(+1), bpos2(0))); // -1 or +1 for each component if it is on negative or positive chunk edge
		
		// update surrounding chunks if the changed block is touching them to update lighting properly
		if (chunk_edge.x != 0						&& (chunk = chunks.query_chunk(coord +chunk_pos(chunk_edge.x,0))))
			chunk->needs_remesh = true;
		if (chunk_edge.y != 0						&& (chunk = chunks.query_chunk(coord +chunk_pos(0,chunk_edge.y))))
			chunk->needs_remesh = true;
		if (chunk_edge.x != 0 && chunk_edge.y != 0	&& (chunk = chunks.query_chunk(coord +chunk_edge)))
			chunk->needs_remesh = true;
	}
	
	void update_whole_chunk_changed (Chunks& chunks) { // whole chunks could have changed -> update our block brightness -> remesh all surrounding chunks
		needs_remesh = true;
		needs_block_brighness_update = true;
		
		// update surrounding chunks to update lighting properly
		
		Chunk* chunk;
		
		if ((chunk = chunks.query_chunk(coord +chunk_pos(-1,-1)))) chunk->needs_remesh = true;
		if ((chunk = chunks.query_chunk(coord +chunk_pos( 0,-1)))) chunk->needs_remesh = true;
		if ((chunk = chunks.query_chunk(coord +chunk_pos(+1,-1)))) chunk->needs_remesh = true;
		if ((chunk = chunks.query_chunk(coord +chunk_pos(+1, 0)))) chunk->needs_remesh = true;
		if ((chunk = chunks.query_chunk(coord +chunk_pos(+1,+1)))) chunk->needs_remesh = true;
		if ((chunk = chunks.query_chunk(coord +chunk_pos( 0,+1)))) chunk->needs_remesh = true;
		if ((chunk = chunks.query_chunk(coord +chunk_pos(-1,+1)))) chunk->needs_remesh = true;
		if ((chunk = chunks.query_chunk(coord +chunk_pos(-1, 0)))) chunk->needs_remesh = true;
	}
};

static double profile_remesh_total = 0;

#if AVOID_QUERY_CHUNK_HASH_LOOKUP
inline Block const* Chunk_Mesher::query_block (bpos_t pos_x, bpos_t pos_y, bpos_t pos_z) {
	if (pos_z < 0 || pos_z >= CHUNK_DIM_Z) return &B_OUT_OF_BOUNDS;
	
	bpos pos_in_chunk;
	chunk_pos neighbour = get_chunk_from_block_pos(bpos(pos_x, pos_y, pos_z), &pos_in_chunk);
	
	Chunk* chunk = neighbour_chunks[neighbour.y +1][neighbour.x +1];
	if (!chunk) return &B_NO_CHUNK;
	
	return chunk->get_block(pos_in_chunk);
}
#endif
