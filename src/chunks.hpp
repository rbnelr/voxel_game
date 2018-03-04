
static constexpr bpos CHUNK_DIM = bpos(32,32,64);

#if 0
struct Chunk;
static Chunk* query_chunk (chunk_pos_t pos);
static Block* query_block (bpos p, Chunk** out_chunk=nullptr);

struct Chunk {
	chunk_pos_t pos;
	
	bool needs_remesh;
	bool needs_block_brighness_update;
	bool needs_upload;
	
	Block	data[CHUNK_DIM.z][CHUNK_DIM.y][CHUNK_DIM.x];
	
	Vbo		vbo;
	
	void init_gl () {
		vbo.init(&chunk_vbo_vert_layout);
	}
	
	Block* get_block (bpos pos) {
		return &data[pos.z][pos.y][pos.x];
	}
	
	void update_block_brighness () {
		bpos i; // position in chunk
		for (i.y=0; i.y<CHUNK_DIM.y; ++i.y) {
			for (i.x=0; i.x<CHUNK_DIM.x; ++i.x) {
				
				i.z = CHUNK_DIM.z -1;
				
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
	}
	
	bpos chunk_origin_block_pos_world () {
		return bpos(pos * CHUNK_DIM.xy(), 0);
	}
	
	void remesh () {
		bpos chunk_origin = chunk_origin_block_pos_world();
		
		vbo.vertecies.clear();
		
		auto cube = [&] (bpos const& pos_world, bpos const& pos_chunk, Block const* b) {
			
			bpos_t XL = pos_world.x;
			bpos_t YL = pos_world.y;
			bpos_t ZL = pos_world.z;
			bpos_t XH = pos_world.x +1;
			bpos_t YH = pos_world.y +1;
			bpos_t ZH = pos_world.z +1;
			
			f32 w = get_block_texture_index_from_block_type(b->type);
			
			#if 0
			bool dark[3][3][3] = {
				query_block(pos_world +bpos(-1,-1,-1))->dark,
				query_block(pos_world +bpos( 0,-1,-1))->dark,
				query_block(pos_world +bpos(+1,-1,-1))->dark,
				query_block(pos_world +bpos(-1, 0,-1))->dark,
				query_block(pos_world +bpos( 0, 0,-1))->dark,
				query_block(pos_world +bpos(+1, 0,-1))->dark,
				query_block(pos_world +bpos(-1,+1,-1))->dark,
				query_block(pos_world +bpos( 0,+1,-1))->dark,
				query_block(pos_world +bpos(+1,+1,-1))->dark,
				
				query_block(pos_world +bpos(-1,-1, 0))->dark,
				query_block(pos_world +bpos( 0,-1, 0))->dark,
				query_block(pos_world +bpos(+1,-1, 0))->dark,
				query_block(pos_world +bpos(-1, 0, 0))->dark,
				false,
				query_block(pos_world +bpos(+1, 0, 0))->dark,
				query_block(pos_world +bpos(-1,+1, 0))->dark,
				query_block(pos_world +bpos( 0,+1, 0))->dark,
				query_block(pos_world +bpos(+1,+1, 0))->dark,
				
				query_block(pos_world +bpos(-1,-1,+1))->dark,
				query_block(pos_world +bpos( 0,-1,+1))->dark,
				query_block(pos_world +bpos(+1,-1,+1))->dark,
				query_block(pos_world +bpos(-1, 0,+1))->dark,
				query_block(pos_world +bpos( 0, 0,+1))->dark,
				query_block(pos_world +bpos(+1, 0,+1))->dark,
				query_block(pos_world +bpos(-1,+1,+1))->dark,
				query_block(pos_world +bpos( 0,+1,+1))->dark,
				query_block(pos_world +bpos(+1,+1,+1))->dark,
			};
			#endif
			
			auto brightness = [&] (bpos vert_pos_world, bpos axis_a, bpos axis_b, bpos plane) -> flt {
				s32 brightness = 0;
				
				brightness += query_block(vert_pos_world +plane -axis_a      +0)->dark ? 0 : 1;
				brightness += query_block(vert_pos_world +plane      +0      +0)->dark ? 0 : 1;
				brightness += query_block(vert_pos_world +plane -axis_a -axis_b)->dark ? 0 : 1;
				brightness += query_block(vert_pos_world +plane      +0 -axis_b)->dark ? 0 : 1;
				
				return (flt)brightness;
			};
			
			#define VERT(x,y,z, u,v, face, axis_a,axis_b, plane) { v3(x,y,z), v4(u,v, face,w), b->hp_ratio, brightness(v3(x,y,z), axis_a,axis_b,plane), b->dbg_tint }
			
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
			
			if (bt_is_transparent(query_block(pos_world +bpos(+1,0,0))->type)) {
				Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Chunk_Vbo_Vertex)*6);
				
				Chunk_Vbo_Vertex vert[4] = {
					VERT(XH,YL,ZL, 0,0, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
					VERT(XH,YH,ZL, 1,0, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
					VERT(XH,YH,ZH, 1,1, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
					VERT(XH,YL,ZH, 0,1, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
				};
				FACE
			}
			if (bt_is_transparent(query_block(pos_world +bpos(-1,0,0))->type)) {
				Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Chunk_Vbo_Vertex)*6);
				
				Chunk_Vbo_Vertex vert[4] = {
					VERT(XL,YH,ZL, 0,0, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
					VERT(XL,YL,ZL, 1,0, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
					VERT(XL,YL,ZH, 1,1, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
					VERT(XL,YH,ZH, 0,1, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
				};
				FACE
			}
			if (bt_is_transparent(query_block(pos_world +bpos(0,+1,0))->type)) {
				Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Chunk_Vbo_Vertex)*6);
				
				Chunk_Vbo_Vertex vert[4] = {
					VERT(XH,YH,ZL, 0,0, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
					VERT(XL,YH,ZL, 1,0, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
					VERT(XL,YH,ZH, 1,1, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
					VERT(XH,YH,ZH, 0,1, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
				};
				FACE
			}
			if (bt_is_transparent(query_block(pos_world +bpos(0,-1,0))->type)) {
				Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Chunk_Vbo_Vertex)*6);
				
				Chunk_Vbo_Vertex vert[4] = {
					VERT(XL,YL,ZL, 0,0, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
					VERT(XH,YL,ZL, 1,0, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
					VERT(XH,YL,ZH, 1,1, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
					VERT(XL,YL,ZH, 0,1, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
				};
				FACE
			}
			if (bt_is_transparent(query_block(pos_world +bpos(0,0,+1))->type)) {
				Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Chunk_Vbo_Vertex)*6);
				
				Chunk_Vbo_Vertex vert[4] = {
					VERT(XL,YL,ZH, 0,0, UVZW_BLOCK_FACE_TOP, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
					VERT(XH,YL,ZH, 1,0, UVZW_BLOCK_FACE_TOP, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
					VERT(XH,YH,ZH, 1,1, UVZW_BLOCK_FACE_TOP, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
					VERT(XL,YH,ZH, 0,1, UVZW_BLOCK_FACE_TOP, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
				};
				FACE
			}
			if (bt_is_transparent(query_block(pos_world +bpos(0,0,-1))->type)) {
				Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Chunk_Vbo_Vertex)*6);
				
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
		};
		
		{
			bpos i;
			for (i.z=0; i.z<CHUNK_DIM.z; ++i.z) {
				for (i.y=0; i.y<CHUNK_DIM.y; ++i.y) {
					for (i.x=0; i.x<CHUNK_DIM.x; ++i.x) {
						auto* block = get_block(i);
						
						if (block->type != BT_AIR) {
							
							cube(i +chunk_origin, i, block);
							
						}
					}
				}
			}
		}
		
	}
	
	void block_only_hp_changed (bpos block_pos_world) { // only breaking animation of block changed -> do not need to update block brightness -> do not need to remesh surrounding chunks (only this chunk needs remesh)
		needs_remesh = true;
			needs_upload = true;
	}
	void block_changed (bpos block_pos_world) { // block was placed or broken -> need to update our block brightness -> need to remesh surrounding chunks
		needs_remesh = true;
		needs_block_brighness_update = true;
			needs_upload = true;
		
		Chunk* chunk;
		
		bpos pos = block_pos_world -chunk_origin_block_pos_world();
		
		bpos2 chunk_edge = select(pos.xy() == bpos2(0), bpos2(-1), select(pos.xy() == CHUNK_DIM.xy() -1, bpos2(+1), bpos2(0))); // -1 or +1 for each component if it is on negative or positive chunk edge
		
		// update surrounding chunks if the changed block is touching them to update lighting properly
		if (chunk_edge.x != 0						&& (chunk = query_chunk(this->pos +chunk_pos_t(chunk_edge.x,0))))
			chunk->needs_remesh = true;
		if (chunk_edge.y != 0						&& (chunk = query_chunk(this->pos +chunk_pos_t(0,chunk_edge.y))))
			chunk->needs_remesh = true;
		if (chunk_edge.x != 0 && chunk_edge.y != 0	&& (chunk = query_chunk(this->pos +chunk_edge)))
			chunk->needs_remesh = true;
	}
	
	void update_whole_chunk_changed () { // whole chunks could have changed -> update our block brightness -> remesh all surrounding chunks
		needs_remesh = true;
		needs_block_brighness_update = true;
			needs_upload = true;
		
		// update surrounding chunks to update lighting properly
		
		Chunk* chunk;
		
		if ((chunk = query_chunk(this->pos +chunk_pos_t(-1,-1)))) chunk->needs_remesh = true;
		if ((chunk = query_chunk(this->pos +chunk_pos_t( 0,-1)))) chunk->needs_remesh = true;
		if ((chunk = query_chunk(this->pos +chunk_pos_t(+1,-1)))) chunk->needs_remesh = true;
		if ((chunk = query_chunk(this->pos +chunk_pos_t(+1, 0)))) chunk->needs_remesh = true;
		if ((chunk = query_chunk(this->pos +chunk_pos_t(+1,+1)))) chunk->needs_remesh = true;
		if ((chunk = query_chunk(this->pos +chunk_pos_t( 0,+1)))) chunk->needs_remesh = true;
		if ((chunk = query_chunk(this->pos +chunk_pos_t(-1,+1)))) chunk->needs_remesh = true;
		if ((chunk = query_chunk(this->pos +chunk_pos_t(-1, 0)))) chunk->needs_remesh = true;
	}
};
#else // test doing update_block_brighness and remesh in worker threads (disabled rid of cross chunk dependencies)
struct Chunk {
	chunk_pos_t pos;
	
	bool needs_remesh;
	bool needs_block_brighness_update;
	bool needs_upload;
	
	Block	data[CHUNK_DIM.z][CHUNK_DIM.y][CHUNK_DIM.x];
	
	Vbo		vbo;
	
	void init_gl () {
		vbo.init(&chunk_vbo_vert_layout);
	}
	
	Block* get_block (bpos pos) {
		return &data[pos.z][pos.y][pos.x];
	}
	
	void update_block_brighness () {
		bpos i; // position in chunk
		for (i.y=0; i.y<CHUNK_DIM.y; ++i.y) {
			for (i.x=0; i.x<CHUNK_DIM.x; ++i.x) {
				
				i.z = CHUNK_DIM.z -1;
				
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
	
	bpos chunk_origin_block_pos_world () {
		return bpos(pos * CHUNK_DIM.xy(), 0);
	}
	
	void remesh () {
		bpos chunk_origin = chunk_origin_block_pos_world();
		
		vbo.vertecies.clear();
		
		auto cube = [&] (bpos const& pos_world, bpos const& pos_chunk, Block const* b) {
			
			bpos_t XL = pos_world.x;
			bpos_t YL = pos_world.y;
			bpos_t ZL = pos_world.z;
			bpos_t XH = pos_world.x +1;
			bpos_t YH = pos_world.y +1;
			bpos_t ZH = pos_world.z +1;
			
			f32 w = get_block_texture_index_from_block_type(b->type);
			
			auto brightness = [&] (bpos vert_pos_world, bpos axis_a, bpos axis_b, bpos plane) -> flt {
				return 4;
			};
			
			#define VERT(x,y,z, u,v, face, axis_a,axis_b, plane) { v3(x,y,z), v4(u,v, face,w), b->hp_ratio, brightness(v3(x,y,z), axis_a,axis_b,plane), b->dbg_tint }
			
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
			
			if (pos_chunk.x == CHUNK_DIM.x-1	|| bt_is_transparent(get_block(pos_chunk +bpos(+1,0,0))->type)) {
				Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Chunk_Vbo_Vertex)*6);
				
				Chunk_Vbo_Vertex vert[4] = {
					VERT(XH,YL,ZL, 0,0, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
					VERT(XH,YH,ZL, 1,0, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
					VERT(XH,YH,ZH, 1,1, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
					VERT(XH,YL,ZH, 0,1, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(0,0,0)),
				};
				FACE
			}
			if (pos_chunk.x == 0				|| bt_is_transparent(get_block(pos_chunk +bpos(-1,0,0))->type)) {
				Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Chunk_Vbo_Vertex)*6);
				
				Chunk_Vbo_Vertex vert[4] = {
					VERT(XL,YH,ZL, 0,0, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
					VERT(XL,YL,ZL, 1,0, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
					VERT(XL,YL,ZH, 1,1, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
					VERT(XL,YH,ZH, 0,1, UVZW_BLOCK_FACE_SIDE, bpos(0,1,0),bpos(0,0,1), bpos(-1,0,0)),
				};
				FACE
			}
			if (pos_chunk.y == CHUNK_DIM.y-1	|| bt_is_transparent(get_block(pos_chunk +bpos(0,+1,0))->type)) {
				Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Chunk_Vbo_Vertex)*6);
				
				Chunk_Vbo_Vertex vert[4] = {
					VERT(XH,YH,ZL, 0,0, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
					VERT(XL,YH,ZL, 1,0, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
					VERT(XL,YH,ZH, 1,1, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
					VERT(XH,YH,ZH, 0,1, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,0,0)),
				};
				FACE
			}
			if (pos_chunk.y == 0				|| bt_is_transparent(get_block(pos_chunk +bpos(0,-1,0))->type)) {
				Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Chunk_Vbo_Vertex)*6);
				
				Chunk_Vbo_Vertex vert[4] = {
					VERT(XL,YL,ZL, 0,0, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
					VERT(XH,YL,ZL, 1,0, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
					VERT(XH,YL,ZH, 1,1, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
					VERT(XL,YL,ZH, 0,1, UVZW_BLOCK_FACE_SIDE, bpos(1,0,0),bpos(0,0,1), bpos(0,-1,0)),
				};
				FACE
			}
			if (pos_chunk.z == CHUNK_DIM.z-1	|| bt_is_transparent(get_block(pos_chunk +bpos(0,0,+1))->type)) {
				Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Chunk_Vbo_Vertex)*6);
				
				Chunk_Vbo_Vertex vert[4] = {
					VERT(XL,YL,ZH, 0,0, UVZW_BLOCK_FACE_TOP, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
					VERT(XH,YL,ZH, 1,0, UVZW_BLOCK_FACE_TOP, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
					VERT(XH,YH,ZH, 1,1, UVZW_BLOCK_FACE_TOP, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
					VERT(XL,YH,ZH, 0,1, UVZW_BLOCK_FACE_TOP, bpos(1,0,0),bpos(0,1,0), bpos(0,0,0)),
				};
				FACE
			}
			if (pos_chunk.z == 0				|| bt_is_transparent(get_block(pos_chunk +bpos(0,0,-1))->type)) {
				Chunk_Vbo_Vertex* out = (Chunk_Vbo_Vertex*)&*vector_append(&vbo.vertecies, sizeof(Chunk_Vbo_Vertex)*6);
				
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
		};
		
		{
			bpos i;
			for (i.z=0; i.z<CHUNK_DIM.z; ++i.z) {
				for (i.y=0; i.y<CHUNK_DIM.y; ++i.y) {
					for (i.x=0; i.x<CHUNK_DIM.x; ++i.x) {
						auto* block = get_block(i);
						
						if (block->type != BT_AIR) {
							
							cube(i +chunk_origin, i, block);
							
						}
					}
				}
			}
		}
		
		needs_remesh = false;
	}
	
	void block_only_hp_changed (bpos block_pos_world) {
		needs_remesh = true;
			needs_upload = true;
	}
	void block_changed (bpos block_pos_world) {
		needs_remesh = true;
		needs_block_brighness_update = true;
			needs_upload = true;
	}
	
	void update_whole_chunk_changed () {
		needs_remesh = true;
		needs_block_brighness_update = true;
			needs_upload = true;
	}
};
#endif

static chunk_pos_t get_chunk_from_block_pos (bpos2 pos_world) {
	
	chunk_pos_t chunk_pos;
	chunk_pos.x = pos_world.x >> GET_CONST_POT(CHUNK_DIM.x); // arithmetic shift right instead of divide because we want  -10 / 32  to be  -1 instead of 0
	chunk_pos.y = pos_world.y >> GET_CONST_POT(CHUNK_DIM.y);
	
	return chunk_pos;
}
static chunk_pos_t get_chunk_from_block_pos (bpos pos_world, bpos* bpos_in_chunk=nullptr) {
	
	chunk_pos_t chunk_pos = get_chunk_from_block_pos(pos_world.xy());
	
	if (bpos_in_chunk) {
		bpos_in_chunk->x = pos_world.x & ((1 << GET_CONST_POT(CHUNK_DIM.x)) -1);
		bpos_in_chunk->y = pos_world.y & ((1 << GET_CONST_POT(CHUNK_DIM.y)) -1);
		bpos_in_chunk->z = pos_world.z;
	}
	
	return chunk_pos;
}
