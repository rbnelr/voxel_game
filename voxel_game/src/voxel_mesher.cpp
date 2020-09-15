#include "stdafx.hpp"
#include "voxel_mesher.hpp"
#include "svo.hpp"
#include "blocks.hpp"

using namespace svo;

struct ChunkRemesher {
	Chunk* chunk;
	svo::SVO& svo;
	Graphics const& g;
	uint64_t world_seed;
	std::vector<VoxelInstance>& opaque_mesh;
	std::vector<VoxelInstance>& transparent_mesh;

	int chunk_lod;

	int calc_tex_index (block_id bid, int face) {
		auto& bti = g.tile_textures.block_tile_info[bid];

		int tex_indx = bti.base_index;
		switch (face) {
			case 4:	tex_indx += bti.bottom;		break;
			case 5:	tex_indx += bti.top;		break;
			default: break;
		}

		return tex_indx;
	}

	VoxelInstance* push_vertex (std::vector<VoxelInstance>& buf) {
		size_t out = buf.size();
		buf.emplace_back();
		return &buf[out];
	}

	__forceinline void push_face (std::vector<VoxelInstance>& buf, int scale, int face, block_id bid, int posx, int posy, int posz) {
		auto* v = push_vertex(buf);
		v->posx = (int8_t)posx;
		v->posy = (int8_t)posy;
		v->posz = (int8_t)posz;
		v->scale_face = (scale << 4) | face;
		v->tex_indx = calc_tex_index(bid, face);
	}

	// should we render the face of block a facing to block b
	bool should_render_face (block_id a, block_id b, transparency_mode ta, transparency_mode tb) {
		// special case for now, might consider doing more logic here
		// for ex. the surface of water seen from inside the water is technically the surface of the air, so wont get rendered
		if (a == B_AIR || a == B_NULL)
			return false;

		return tb != TM_OPAQUE && (a != b || tb == TM_PARTIAL);
	}

	void block (int posx, int posy, int posz, int scale, block_id bid) {
		static constexpr int3 FACES[6] {
			int3(-1,0,0), int3(+1,0,0), int3(0,-1,0), int3(0,+1,0), int3(0,0,-1), int3(0,0,+1)
		};
		static constexpr float3 FACESf[6] {
			float3(-1,0,0), float3(+1,0,0), float3(0,-1,0), float3(0,+1,0), float3(0,0,-1), float3(0,0,+1)
		};

		int size = 1 << scale;

		auto tm = blocks.transparency[bid];

		for (int face=0; face<6; ++face) {
			int nb_posx = posx + FACES[face].x * size;
			int nb_posy = posy + FACES[face].y * size;
			int nb_posz = posz + FACES[face].z * size;
			
			int abs_nb_posx = nb_posx + chunk->pos.x;
			int abs_nb_posy = nb_posy + chunk->pos.y;
			int abs_nb_posz = nb_posz + chunk->pos.z;

			auto nb = svo.octree_read(abs_nb_posx, abs_nb_posy, abs_nb_posz, scale); // account for scales being relative to chunk
			auto nb_bid = (block_id)nb.vox.value;

			if (nb.size >= size) {
				assert(nb.vox.type == BLOCK_ID);

				auto nb_tm = blocks.transparency[nb_bid];

				// generate face facing to neighbour
				// only if neighbour is a voxel of same size or large (ie. not subdivided futher) because we would have to recurse further to generate the correct faces
				if (should_render_face(bid, nb_bid, tm, nb_tm)) {
					push_face(	tm == TM_TRANSPARENT ? transparent_mesh : opaque_mesh,
						scale -chunk_lod, face, bid, posx >> chunk_lod, posy >> chunk_lod, posz >> chunk_lod);
				}

				// generate face of neighbour for it
				// if it is larger than us (ie. it did not want to recurse further to find us as it's neightbour) like described above
				if (nb.size > size && should_render_face(nb_bid, bid, nb_tm, tm)) {
					// ^1 flips the -X face to +X and the inverse, since we need to generate the face the other way around
					push_face(	nb_tm == TM_TRANSPARENT ? transparent_mesh : opaque_mesh,
						scale -chunk_lod, face ^ 1, nb_bid,  nb_posx >> chunk_lod, nb_posy >> chunk_lod, nb_posz >> chunk_lod); 
				}
			}
		}
	}

	void block_mesh (int x, int y, int z, int scale, block_id bid) {
		//auto& bmi = g.tile_textures.block_meshes_info[bid];
		//auto& bti = g.tile_textures.block_tile_info[bid];
		//
		//// get a 'random' but deterministic value based on block position
		//uint64_t h = hash(int3(x,y,z) + chunk->pos) ^ wg.seed;
		//
		//// get a random determinisitc 2d offset
		//float rand_x = (float)( h        & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // [0, 1)
		//float rand_y = (float)((h >> 32) & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // [0, 1)
		//
		//// get a random deterministic variant
		//int tex_indx = bti.base_index;
		//tex_indx += bti.variants > 1 ? (int)(rand_x * (float)bti.variants) : 0; // [0, tile.variants)
		//
		//
		//int out = (int)opaque_mesh.size();
		//opaque_mesh.resize(out + bmi.size);
		//
		//for (int i=0; i<bmi.size; ++i) {
		//	auto& v = g.tile_textures.block_meshes[bmi.offset + i];
		//
		//	opaque_mesh[out].pos_model.x = v.pos_model.x + (float)x + 0.5f + (rand_x * 2 - 1) * 0.25f;
		//	opaque_mesh[out].pos_model.y = v.pos_model.y + (float)y + 0.5f + (rand_y * 2 - 1) * 0.25f;
		//	opaque_mesh[out].pos_model.z = v.pos_model.z + (float)z + 0.5f;
		//
		//	opaque_mesh[out].uv.x = v.uv.x * bti.uv_size.x + bti.uv_pos.x;
		//	opaque_mesh[out].uv.y = v.uv.y * bti.uv_size.y + bti.uv_pos.y;
		//
		//	opaque_mesh[out].tex_indx = tex_indx;
		//
		//	out++;
		//}
	
	}

	void remesh_chunk () {
		ZoneScoped;

		chunk_lod = chunk->scale - CHUNK_SCALE;

		int scale = chunk->scale-1;

		StackNode stack[MAX_DEPTH];
		stack[scale] = { &chunk->nodes[0], 0, 0 };

		while (scale < chunk->scale) {
			assert(scale >= 0);

			int3& pos			= stack[scale].pos;
			int& child_idx		= stack[scale].child_idx;
			Node* node			= stack[scale].node;

			if (child_idx >= 8) {
				// Pop
				scale++;

			} else {

				int child_x = pos.x + (children_pos[child_idx].x << scale);
				int child_y = pos.y + (children_pos[child_idx].y << scale);
				int child_z = pos.z + (children_pos[child_idx].z << scale);

				auto vox = node->get_child(child_idx);

				if (vox.type == NODE_PTR) {
					// Push
					scale--;

					stack[scale] = { &chunk->nodes[vox.value], int3(child_x, child_y, child_z), 0 };

					continue; // don't inc child indx
				} else {
					assert(vox.type == BLOCK_ID);

					block_id bid = (block_id)vox.value;
					auto tb = blocks.transparency[bid];

					//if (tb != TM_BLOCK_MESH) {
						block(child_x, child_y, child_z, scale, bid);
					//} else {
					//	block_mesh(child_x, child_y, child_z, scale, bid);
					//}
				}
			}

			stack[scale].child_idx++;
		}

		ZoneValue((int)opaque_mesh.size());
		ZoneValue((int)transparent_mesh.size());
	}
};

void remesh_chunk (Chunk* chunk, svo::SVO& svo, Graphics const& g, uint64_t world_seed,
		std::vector<VoxelInstance>& opaque_mesh,
		std::vector<VoxelInstance>& transparent_mesh) {
	
	ChunkRemesher{ chunk, svo, g, world_seed, opaque_mesh, transparent_mesh }.remesh_chunk();
}
