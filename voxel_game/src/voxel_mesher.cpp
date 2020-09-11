#include "stdafx.hpp"
#include "voxel_mesher.hpp"
#include "svo.hpp"
#include "blocks.hpp"

using namespace svo;

struct ChunkRemesher {
	Chunk* chunk;
	svo::SVO& svo;
	Graphics const& g;
	WorldGenerator const& wg;
	std::vector<VoxelVertex>& opaque_mesh;
	std::vector<VoxelVertex>& transparent_mesh;

	void push_face (std::vector<VoxelVertex>& buf, float size, int face, block_id bid, float posx, float posy, float posz) {
		static constexpr int FACE_INDICES[6] {
			1,3,0, 0,3,2
		};
		static constexpr float3 FACES[6][4] {
			{ // X- face
				float3(0,1,0),
				float3(0,0,0),
				float3(0,1,1),
				float3(0,0,1),
			},
			{ // X+ face
				float3(1,0,0),
				float3(1,1,0),
				float3(1,0,1),
				float3(1,1,1),
			},
			{ // Y- face
				float3(0,0,0),
				float3(1,0,0),
				float3(0,0,1),
				float3(1,0,1),
			},
			{ // Y+ face
				float3(1,1,0),
				float3(0,1,0),
				float3(1,1,1),
				float3(0,1,1),
			},
			{ // Z- face
				float3(0,1,0),
				float3(1,1,0),
				float3(0,0,0),
				float3(1,0,0),
			},
			{ // Z+ face
				float3(0,0,1),
				float3(1,0,1),
				float3(0,1,1),
				float3(1,1,1),
			},
		};
		static constexpr float2 UVS[4] {
			float2(0, 0),
			float2(1, 0),
			float2(0, 1),
			float2(1, 1),
		};

		int out = (int)buf.size();
		buf.resize(out + 6);

		auto& bti = g.tile_textures.block_tile_info[bid];
		
		int tex_indx = bti.base_index;
		switch (face) {
			case 4:	tex_indx += bti.bottom;		break;
			case 5:	tex_indx += bti.top;		break;
			default: break;
		}

		VoxelVertex vertices[4];
		for (int i=0; i<4; ++i) {
			vertices[i].pos_model.x = FACES[face][i].x * size + posx;
			vertices[i].pos_model.y = FACES[face][i].y * size + posy;
			vertices[i].pos_model.z = FACES[face][i].z * size + posz;
			vertices[i].uv = UVS[i] * size;
			vertices[i].tex_indx = tex_indx;
		}

		for (int i=0; i<6; ++i) {
			buf[out++] = vertices[ FACE_INDICES[i] ];
		}
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

		float posfx = (float)posx;
		float posfy = (float)posy;
		float posfz = (float)posz;
		float sizef = (float)size;

		int abs_posx = posx + chunk->pos.x;
		int abs_posy = posy + chunk->pos.y;
		int abs_posz = posz + chunk->pos.z;

		auto tm = blocks.transparency[bid];

		for (int face=0; face<6; ++face) {
			int nb_posx = abs_posx + FACES[face].x * size;
			int nb_posy = abs_posy + FACES[face].y * size;
			int nb_posz = abs_posz + FACES[face].z * size;
			
			auto nb = svo.octree_read(nb_posx, nb_posy, nb_posz, scale);
			auto nb_bid = (block_id)nb.vox.value;

			if (nb.size >= size) {
				assert(nb.vox.type == BLOCK_ID);

				auto nb_tm = blocks.transparency[nb_bid];

				// generate face facing to neighbour
				// only if neighbour is a voxel of same size or large (ie. not subdivided futher) because we would have to recurse further to generate the correct faces
				if (should_render_face(bid, nb_bid, tm, nb_tm)) {
					push_face(	tm == TM_TRANSPARENT ? transparent_mesh : opaque_mesh,
						sizef, face, bid, posfx, posfy, posfz);
				}

				// generate face of neighbour for it
				// if it is larger than us (ie. it did not want to recurse further to find us as it's neightbour) like described above
				if (nb.size > size && should_render_face(nb_bid, bid, nb_tm, tm)) {
					float nb_posfx = posfx + FACESf[face].x * sizef;
					float nb_posfy = posfy + FACESf[face].y * sizef;
					float nb_posfz = posfz + FACESf[face].z * sizef;

					// ^1 flips the -X face to +X and the inverse, since we need to generate the face the other way around
					push_face(	nb_tm == TM_TRANSPARENT ? transparent_mesh : opaque_mesh,
						sizef, face ^ 1, nb_bid,  nb_posfx, nb_posfy, nb_posfz); 
				}
			}
		}
	}

	void block_mesh (int x, int y, int z, int scale, block_id bid) {
		auto& bmi = g.tile_textures.block_meshes_info[bid];
		auto& bti = g.tile_textures.block_tile_info[bid];

		// get a 'random' but deterministic value based on block position
		uint64_t h = hash(int3(x,y,z) + chunk->pos) ^ wg.seed;

		// get a random determinisitc 2d offset
		float rand_x = (float)( h        & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // [0, 1)
		float rand_y = (float)((h >> 32) & 0xffffffffull) * (1.0f / (float)(1ull << 32)); // [0, 1)

		// get a random deterministic variant
		int tex_indx = bti.base_index;
		tex_indx += bti.variants > 1 ? (int)(rand_x * (float)bti.variants) : 0; // [0, tile.variants)


		int out = (int)opaque_mesh.size();
		opaque_mesh.resize(out + bmi.size);

		for (int i=0; i<bmi.size; ++i) {
			auto& v = g.tile_textures.block_meshes[bmi.offset + i];

			opaque_mesh[out].pos_model.x = v.pos_model.x + (float)x + 0.5f + (rand_x * 2 - 1) * 0.25f;
			opaque_mesh[out].pos_model.y = v.pos_model.y + (float)y + 0.5f + (rand_y * 2 - 1) * 0.25f;
			opaque_mesh[out].pos_model.z = v.pos_model.z + (float)z + 0.5f;

			opaque_mesh[out].uv.x = v.uv.x * bti.uv_size.x + bti.uv_pos.x;
			opaque_mesh[out].uv.y = v.uv.y * bti.uv_size.y + bti.uv_pos.y;

			opaque_mesh[out].tex_indx = tex_indx;

			out++;
		}

	}

	void remesh_chunk () {
		ZoneScoped;

		StackNode stack[MAX_DEPTH];
		stack[chunk->scale-1] = { &chunk->nodes[0], 0, 0 };

		int scale = chunk->scale-1;

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

					if (tb != TM_BLOCK_MESH) {
						block(child_x, child_y, child_z, scale, bid);
					} else {
						block_mesh(child_x, child_y, child_z, scale, bid);
					}
				}
			}

			stack[scale].child_idx++;
		}
	}
};

void remesh_chunk (Chunk* chunk, svo::SVO& svo, Graphics const& g, WorldGenerator const& wg,
		std::vector<VoxelVertex>& opaque_mesh, std::vector<VoxelVertex>& transparent_mesh) {
	
	ChunkRemesher{ chunk, svo, g, wg, opaque_mesh, transparent_mesh }.remesh_chunk();
}
