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

	void push_face (std::vector<VoxelVertex>& buf, float3 pos, float size, int face, int tex_indx) {
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

		VoxelVertex vertices[4];
		for (int i=0; i<4; ++i) {
			vertices[i].pos_model = FACES[face][i] * size + pos;
			vertices[i].uv = UVS[i];
			vertices[i].tex_indx = tex_indx;
		}

		for (int i=0; i<6; ++i) {
			buf[out++] = vertices[ FACE_INDICES[i] ];
		}
	}

	// should we render the face of block a facing to block b
	bool should_render_face (block_id a, block_id b) {
		// special case for now, might consider doing more logic here
		// for ex. the surface of water seen from inside the water is technically the surface of the air, so wont get rendered
		if (a == B_AIR || a == B_NULL)
			return false;

		auto tb = blocks.transparency[b];

		//return tb != TM_OPAQUE;
		return b == B_AIR || b == B_NULL;
	}

	void chunk_block (int3 pos, int size, block_id bid) {
		static constexpr int3 FACES[6] {
			int3(-1,0,0), int3(+1,0,0), int3(0,-1,0), int3(0,+1,0), int3(0,0,-1), int3(0,0,+1)
		};

		float3 posf = (float3)pos;
		float sizef = (float)size;

		int3 abs_pos = pos + chunk->pos;

		for (int face=0; face<6; ++face) {
			int3 nb_pos = abs_pos + FACES[face] * size;
			
			auto nb = svo.octree_read(nb_pos.x, nb_pos.y, nb_pos.z, size);

			if (nb.size >= size) {
				assert(nb.vox.type == BLOCK_ID);

				// generate face facing to neighbour
				// only if neighbour is a voxel of same size or large (ie. not subdivided futher) because we would have to recurse further to generate the correct faces
				if (should_render_face(bid, (block_id)nb.vox.value)) {
					push_face(opaque_mesh, posf, sizef, face, 5);
				}

				// generate face of neighbour for it
				// if it is larger than us (ie. it did not want to recurse further to find us as it's neightbour) like described above
				if (nb.size > size && should_render_face((block_id)nb.vox.value, bid)) {
					float3 nb_mesh_pos = (float3)(pos + FACES[face] * size);
					push_face(opaque_mesh, nb_mesh_pos, sizef, face ^ 1, 5); // ^1 flips the -X face to +X and the inverse, since we need to generate the face the other way around
				}
			}
		}
	}

	void remesh_chunk () {
		ZoneScoped;

		StackNode stack[MAX_DEPTH];
		stack[chunk->scale-1] = { svo.allocator.get_node(chunk, 0), 0, 0 };

		int scale = chunk->scale-1;
		int size = 1 << scale;

		while (scale < chunk->scale) {
			assert(scale >= 0);

			int3& pos			= stack[scale].pos;
			int& child_idx		= stack[scale].child_idx;
			Node* node			= stack[scale].node;

			if (child_idx >= 8) {
				// Pop
				scale++;
				size <<= 1;

			} else {

				int3 child_pos = pos + (children_pos[child_idx] << scale);

				auto vox = node->get_child(child_idx);

				if (vox.type == NODE_PTR) {
					// Push
					scale--;
					size >>= 1;

					stack[scale] = { svo.allocator.get_node(chunk, vox.value), child_pos, 0 };

					continue; // don't inc child indx
				} else {
					assert(vox.type == BLOCK_ID);

					chunk_block(child_pos, size, (block_id)vox.value);
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
