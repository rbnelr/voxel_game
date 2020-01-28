#pragma once
#include "kissmath.hpp"
#include "blocks.hpp"

#include "stdint.h"
#include <unordered_map>

#include "gl.hpp"

struct Chunk_Vbo_Vertex {
	float3		pos_world;
	float4		uvzw_atlas; // xy: [0,1] texture uv;  z: 0=side, 1=top, 2=bottom;  w: texture index
	float		hp_ratio; // [0,1]
	float		brightness;
	lrgba		dbg_tint;
};
static constexpr std::array<old::Vertex_Attribute, 5> chunk_vbo_vert_layout = {
	old::Vertex_Attribute{ "pos_world",		old::T_V3,	sizeof(Chunk_Vbo_Vertex), offsetof(Chunk_Vbo_Vertex, pos_world) },
	old::Vertex_Attribute{ "uvzw_atlas",	old::T_V4,	sizeof(Chunk_Vbo_Vertex), offsetof(Chunk_Vbo_Vertex, uvzw_atlas) },
	old::Vertex_Attribute{ "hp_ratio",		old::T_FLT,	sizeof(Chunk_Vbo_Vertex), offsetof(Chunk_Vbo_Vertex, hp_ratio) },
	old::Vertex_Attribute{ "brightness",	old::T_FLT,	sizeof(Chunk_Vbo_Vertex), offsetof(Chunk_Vbo_Vertex, brightness) },
	old::Vertex_Attribute{ "dbg_tint",		old::T_V4,	sizeof(Chunk_Vbo_Vertex), offsetof(Chunk_Vbo_Vertex, dbg_tint) },
};

typedef int64_t	bpos_t;
typedef int64v2	bpos2;
typedef int64v3	bpos;

typedef int64v2	chunk_pos_t;

#define CHUNK_DIM_X			32
#define CHUNK_DIM_Y			32
#define CHUNK_DIM_Z			64
#define CHUNK_DIM_SHIFT_X	5 // for coord >> CHUNK_DIM_SHIFT
#define CHUNK_DIM_SHIFT_Y	5 // for coord >> CHUNK_DIM_SHIFT
#define CHUNK_DIM_SHIFT_Z	6 // for coord >> CHUNK_DIM_SHIFT
#define CHUNK_DIM			bpos(CHUNK_DIM_X, CHUNK_DIM_Y, CHUNK_DIM_Z)
#define CHUNK_DIM_2D		bpos2(CHUNK_DIM_X, CHUNK_DIM_Y)

struct s64v2_hashmap {
	chunk_pos_t v;

	bool operator== (s64v2_hashmap const& r) const { // for hash map
		return v.x == r.v.x && v.y == r.v.y;
	}
};

inline size_t hash (chunk_pos_t v) {
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

struct Chunk;
extern Chunk* _prev_query_chunk; // avoid hash map lookup most of the time, since a lot of query_chunk's are going to end up in the same chunk (in query_block of clustered blocks)

extern std::unordered_map<s64v2_hashmap, Chunk*> chunks;

inline void delete_all_chunks ();

inline Chunk* query_chunk (chunk_pos_t coord);
inline Block* query_block (bpos p, Chunk** out_chunk=nullptr);

static chunk_pos_t get_chunk_from_block_pos (bpos2 pos_world) {
	
	chunk_pos_t chunk_pos;
	chunk_pos.x = pos_world.x >> CHUNK_DIM_SHIFT_X; // arithmetic shift right instead of divide because we want  -10 / 32  to be  -1 instead of 0
	chunk_pos.y = pos_world.y >> CHUNK_DIM_SHIFT_Y;
	
	return chunk_pos;
}
static chunk_pos_t get_chunk_from_block_pos (bpos pos_world, bpos* bpos_in_chunk=nullptr) {
	
	chunk_pos_t chunk_pos = get_chunk_from_block_pos((bpos2)pos_world);
	
	if (bpos_in_chunk) {
		bpos_in_chunk->x = pos_world.x & ((1 << CHUNK_DIM_SHIFT_X) -1);
		bpos_in_chunk->y = pos_world.y & ((1 << CHUNK_DIM_SHIFT_Y) -1);
		bpos_in_chunk->z = pos_world.z;
	}
	
	return chunk_pos;
}

struct Chunk_Mesher {
	
	Vbo_old* vbo_opaque;
	Vbo_old* vbo_transperant;
	
	bpos chunk_origin_block_world;
	
	#define AVOID_QUERY_CHUNK_HASH_LOOKUP 1
	
	#if AVOID_QUERY_CHUNK_HASH_LOOKUP
	Chunk* neighbour_chunks[3][3];
	
	Block const* query_block (bpos_t pos_world_x, bpos_t pos_world_y, bpos_t pos_world_z);
	Block const* query_block (bpos p) {
		return query_block(p.x, p.y, p.z);
	}
	#else
	Block const* query_block (bpos_t pos_world_x, bpos_t pos_world_y, bpos_t pos_world_z) {
		return ::query_block(bpos(pos_world_x,pos_world_y,pos_world_z));
	}
	Block const* query_block (bpos p) {
		return ::query_block(p);
	}
	#endif
	
	float brightness (bpos vert_pos_world, bpos axis_a, bpos axis_b, bpos plane) {
		int brightness = 0;
		
		brightness += query_block(vert_pos_world +plane -axis_a      +0)->dark ? 0 : 1;
		brightness += query_block(vert_pos_world +plane      +0      +0)->dark ? 0 : 1;
		brightness += query_block(vert_pos_world +plane -axis_a -axis_b)->dark ? 0 : 1;
		brightness += query_block(vert_pos_world +plane      +0 -axis_b)->dark ? 0 : 1;
		
		return (float)brightness;
	}
	
	void mesh (Chunk* chunk);

	#define XL (block_pos_world_x)
	#define YL (block_pos_world_y)
	#define ZL (block_pos_world_z)
	#define XH (block_pos_world_x +1)
	#define YH (block_pos_world_y +1)
	#define ZH (block_pos_world_z +1)
	
	#define VERT(x,y,z, u,v, face, axis_a,axis_b, plane) { (float3)bpos(x,y,z), float4(u,v, face,w), b->hp_ratio, brightness(bpos(x,y,z), axis_a,axis_b,plane), b->dbg_tint }
	
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
	
	// per block
	Block const* b;
	bpos_t block_pos_world_x;
	bpos_t block_pos_world_y;
	bpos_t block_pos_world_z;
	float w;

	void face_px (Vbo_old* vbo) {
		Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)vector_append(&vbo->vertecies, sizeof(Chunk_Vbo_Vertex)*6);
		
		Chunk_Vbo_Vertex vert[4] = {
			VERT(XH,YL,ZL, 0,0, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
			VERT(XH,YH,ZL, 1,0, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
			VERT(XH,YH,ZH, 1,1, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
			VERT(XH,YL,ZH, 0,1, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
		};
		FACE
	}
	void face_nx (Vbo_old* vbo) {
		Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)vector_append(&vbo->vertecies, sizeof(Chunk_Vbo_Vertex)*6);
		
		Chunk_Vbo_Vertex vert[4] = {
			VERT(XL,YH,ZL, 0,0, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
			VERT(XL,YL,ZL, 1,0, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
			VERT(XL,YL,ZH, 1,1, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
			VERT(XL,YH,ZH, 0,1, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
		};
		FACE
	}
	void face_py (Vbo_old* vbo) {
		Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)vector_append(&vbo->vertecies, sizeof(Chunk_Vbo_Vertex)*6);
		
		Chunk_Vbo_Vertex vert[4] = {
			VERT(XH,YH,ZL, 0,0, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
			VERT(XL,YH,ZL, 1,0, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
			VERT(XL,YH,ZH, 1,1, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
			VERT(XH,YH,ZH, 0,1, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
		};
		FACE
	}
	void face_ny (Vbo_old* vbo) {
		Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)vector_append(&vbo->vertecies, sizeof(Chunk_Vbo_Vertex)*6);
		
		Chunk_Vbo_Vertex vert[4] = {
			VERT(XL,YL,ZL, 0,0, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
			VERT(XH,YL,ZL, 1,0, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
			VERT(XH,YL,ZH, 1,1, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
			VERT(XL,YL,ZH, 0,1, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
		};
		FACE
	}
	void face_pz (Vbo_old* vbo) {
		Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)vector_append(&vbo->vertecies, sizeof(Chunk_Vbo_Vertex)*6);
		
		Chunk_Vbo_Vertex vert[4] = {
			VERT(XL,YL,ZH, 0,0, UVZW_BLOCK_FACE_TOP, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
			VERT(XH,YL,ZH, 1,0, UVZW_BLOCK_FACE_TOP, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
			VERT(XH,YH,ZH, 1,1, UVZW_BLOCK_FACE_TOP, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
			VERT(XL,YH,ZH, 0,1, UVZW_BLOCK_FACE_TOP, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
		};
		FACE
	}
	void face_nz (Vbo_old* vbo) {
		Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)vector_append(&vbo->vertecies, sizeof(Chunk_Vbo_Vertex)*6);
		
		Chunk_Vbo_Vertex vert[4] = {
			VERT(XL,YH,ZL, 0,0, UVZW_BLOCK_FACE_BOTTOM, bpos(1,0,0),bpos(0,1,0), bpos(0,0,-1)),
			VERT(XH,YH,ZL, 1,0, UVZW_BLOCK_FACE_BOTTOM, bpos(1,0,0),bpos(0,1,0), bpos(0,0,-1)),
			VERT(XH,YL,ZL, 1,1, UVZW_BLOCK_FACE_BOTTOM, bpos(1,0,0),bpos(0,1,0), bpos(0,0,-1)),
			VERT(XL,YL,ZL, 0,1, UVZW_BLOCK_FACE_BOTTOM, bpos(1,0,0),bpos(0,1,0), bpos(0,0,-1)),
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
	
	void cube_opaque () {
		if (bt_is_transparent(query_block(block_pos_world_x +1, block_pos_world_y, block_pos_world_z)->type)) face_px(vbo_opaque);
		if (bt_is_transparent(query_block(block_pos_world_x -1, block_pos_world_y, block_pos_world_z)->type)) face_nx(vbo_opaque);
		if (bt_is_transparent(query_block(block_pos_world_x, block_pos_world_y +1, block_pos_world_z)->type)) face_py(vbo_opaque);
		if (bt_is_transparent(query_block(block_pos_world_x, block_pos_world_y -1, block_pos_world_z)->type)) face_ny(vbo_opaque);
		if (bt_is_transparent(query_block(block_pos_world_x, block_pos_world_y, block_pos_world_z +1)->type)) face_pz(vbo_opaque);
		if (bt_is_transparent(query_block(block_pos_world_x, block_pos_world_y, block_pos_world_z -1)->type)) face_nz(vbo_opaque);
	};
	void cube_transperant () {
		
		block_type bt;
		
		bt = query_block(block_pos_world_x +1, block_pos_world_y, block_pos_world_z)->type;
		if (bt_is_transparent(bt) && bt != b->type) face_px(vbo_transperant);
		
		bt = query_block(block_pos_world_x -1, block_pos_world_y, block_pos_world_z)->type;
		if (bt_is_transparent(bt) && bt != b->type) face_nx(vbo_transperant);
		
		bt = query_block(block_pos_world_x, block_pos_world_y +1, block_pos_world_z)->type;
		if (bt_is_transparent(bt) && bt != b->type) face_py(vbo_transperant);
		
		bt = query_block(block_pos_world_x, block_pos_world_y -1, block_pos_world_z)->type;
		if (bt_is_transparent(bt) && bt != b->type) face_ny(vbo_transperant);
		
		bt = query_block(block_pos_world_x, block_pos_world_y, block_pos_world_z +1)->type;
		if (bt_is_transparent(bt) && bt != b->type) face_pz(vbo_transperant);
		
		bt = query_block(block_pos_world_x, block_pos_world_y, block_pos_world_z -1)->type;
		if (bt_is_transparent(bt) && bt != b->type) face_nz(vbo_transperant);
	};
	
};

struct Chunk {
	chunk_pos_t coord;
	
	bool needs_remesh;
	bool needs_block_brighness_update;
	
	Block	blocks[CHUNK_DIM_Z][CHUNK_DIM_Y][CHUNK_DIM_X];
	
	Vbo_old		vbo				= Vbo_old(chunk_vbo_vert_layout);
	Vbo_old		vbo_transperant = Vbo_old(chunk_vbo_vert_layout);
	
	void init_gl () {
		vbo.init();
		vbo_transperant.init();
	}
	
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
	
	static bpos chunk_origin_block_world (chunk_pos_t coord) {
		return bpos(coord * CHUNK_DIM_2D, 0);
	}
	bpos chunk_origin_block_world () {
		return chunk_origin_block_world(coord);
	}
	
	void remesh () {
		Chunk_Mesher mesher;
		
		mesher.mesh(this);
		
		needs_remesh = false;
	}
	
	void block_only_texture_changed (bpos block_pos_world) { // only breaking animation of block changed -> do not need to update block brightness -> do not need to remesh surrounding chunks (only this chunk needs remesh)
		needs_remesh = true;
	}
	void block_changed (bpos block_pos_world) { // block was placed or broken -> need to update our block brightness -> need to remesh surrounding chunks
		needs_remesh = true;
		needs_block_brighness_update = true;
		
		Chunk* chunk;
		
		bpos pos = block_pos_world -chunk_origin_block_world();
		
		bpos2 chunk_edge = select((bpos2)pos == bpos2(0), bpos2(-1), select((bpos2)pos == CHUNK_DIM_2D -1, bpos2(+1), bpos2(0))); // -1 or +1 for each component if it is on negative or positive chunk edge
		
		// update surrounding chunks if the changed block is touching them to update lighting properly
		if (chunk_edge.x != 0						&& (chunk = query_chunk(coord +chunk_pos_t(chunk_edge.x,0))))
			chunk->needs_remesh = true;
		if (chunk_edge.y != 0						&& (chunk = query_chunk(coord +chunk_pos_t(0,chunk_edge.y))))
			chunk->needs_remesh = true;
		if (chunk_edge.x != 0 && chunk_edge.y != 0	&& (chunk = query_chunk(coord +chunk_edge)))
			chunk->needs_remesh = true;
	}
	
	void update_whole_chunk_changed () { // whole chunks could have changed -> update our block brightness -> remesh all surrounding chunks
		needs_remesh = true;
		needs_block_brighness_update = true;
		
		// update surrounding chunks to update lighting properly
		
		Chunk* chunk;
		
		if ((chunk = query_chunk(coord +chunk_pos_t(-1,-1)))) chunk->needs_remesh = true;
		if ((chunk = query_chunk(coord +chunk_pos_t( 0,-1)))) chunk->needs_remesh = true;
		if ((chunk = query_chunk(coord +chunk_pos_t(+1,-1)))) chunk->needs_remesh = true;
		if ((chunk = query_chunk(coord +chunk_pos_t(+1, 0)))) chunk->needs_remesh = true;
		if ((chunk = query_chunk(coord +chunk_pos_t(+1,+1)))) chunk->needs_remesh = true;
		if ((chunk = query_chunk(coord +chunk_pos_t( 0,+1)))) chunk->needs_remesh = true;
		if ((chunk = query_chunk(coord +chunk_pos_t(-1,+1)))) chunk->needs_remesh = true;
		if ((chunk = query_chunk(coord +chunk_pos_t(-1, 0)))) chunk->needs_remesh = true;
	}
};

static double profile_remesh_total = 0;

#if AVOID_QUERY_CHUNK_HASH_LOOKUP
inline Block const* Chunk_Mesher::query_block (bpos_t pos_world_x, bpos_t pos_world_y, bpos_t pos_world_z) {
	if (pos_world_z < 0 || pos_world_z >= CHUNK_DIM_Z) return &B_OUT_OF_BOUNDS;
	
	bpos pos_in_chunk;
	chunk_pos_t neighbour = get_chunk_from_block_pos(bpos(pos_world_x, pos_world_y, pos_world_z) -chunk_origin_block_world, &pos_in_chunk);
	
	Chunk* chunk = neighbour_chunks[neighbour.y +1][neighbour.x +1];
	if (!chunk) return &B_NO_CHUNK;
	
	return chunk->get_block(pos_in_chunk);
}
#endif

inline void Chunk_Mesher::mesh (Chunk* chunk) {
	//PROFILE_BEGIN(profile_remesh_total);
	
	vbo_opaque =		&chunk->vbo;
	vbo_transperant =	&chunk->vbo_transperant;
	
	vbo_opaque->		vertecies.clear();
	vbo_transperant->	vertecies.clear();
	
	chunk_origin_block_world = Chunk::chunk_origin_block_world(chunk->coord);
	
	#if AVOID_QUERY_CHUNK_HASH_LOOKUP
	neighbour_chunks[0][0] = ::query_chunk(chunk->coord +chunk_pos_t(-1,-1));
	neighbour_chunks[0][1] = ::query_chunk(chunk->coord +chunk_pos_t( 0,-1));
	neighbour_chunks[0][2] = ::query_chunk(chunk->coord +chunk_pos_t(+1,-1));
	neighbour_chunks[1][0] = ::query_chunk(chunk->coord +chunk_pos_t(-1, 0));
	neighbour_chunks[1][1] = chunk                                          ;
	neighbour_chunks[1][2] = ::query_chunk(chunk->coord +chunk_pos_t(+1, 0));
	neighbour_chunks[2][0] = ::query_chunk(chunk->coord +chunk_pos_t(-1,+1));
	neighbour_chunks[2][1] = ::query_chunk(chunk->coord +chunk_pos_t( 0,+1));
	neighbour_chunks[2][2] = ::query_chunk(chunk->coord +chunk_pos_t(+1,+1));
	#endif
	
	{
		bpos i;
		for (i.z=0; i.z<CHUNK_DIM_Z; ++i.z) {
			for (i.y=0; i.y<CHUNK_DIM_Y; ++i.y) {
				for (i.x=0; i.x<CHUNK_DIM_X; ++i.x) {
					auto* block = chunk->get_block(i);
					
					if (block->type != BT_AIR) {
						
						b = block;
						block_pos_world_x = i.x +chunk_origin_block_world.x;
						block_pos_world_y = i.y +chunk_origin_block_world.y;
						block_pos_world_z = i.z +chunk_origin_block_world.z;
						w = (float)get_block_texture_index_from_block_type(b->type);
						
						if (block_props[block->type].transparency == TM_TRANSP_MASS)
							cube_transperant();
						else
							cube_opaque();
						
					}
				}
			}
		}
	}
	
	//PROFILE_END_ACCUM(profile_remesh_total);
	//PROFILE_PRINT(profile_remesh_total, "");
}


inline void delete_all_chunks () {
	for (auto& c : chunks) delete c.second;

	_prev_query_chunk = nullptr;

	chunks.clear();
}

inline Chunk* query_chunk (chunk_pos_t coord) {
	if (_prev_query_chunk && equal(_prev_query_chunk->coord, coord)) {
		return _prev_query_chunk;
	} else {

		auto k = chunks.find({coord});
		if (k == chunks.end()) return nullptr;

		Chunk* chunk = k->second;

		_prev_query_chunk = chunk;

		return chunk;
	}
}
inline Block* query_block (bpos p, Chunk** out_chunk) {
	if (out_chunk) *out_chunk = nullptr;

	if (p.z < 0 || p.z >= CHUNK_DIM_Z) return (Block*)&B_OUT_OF_BOUNDS;

	bpos block_pos_chunk;
	chunk_pos_t chunk_pos = get_chunk_from_block_pos(p, &block_pos_chunk);

	Chunk* chunk = query_chunk(chunk_pos);
	if (!chunk) return (Block*)&B_NO_CHUNK;

	if (out_chunk) *out_chunk = chunk;
	return chunk->get_block(block_pos_chunk);
}

