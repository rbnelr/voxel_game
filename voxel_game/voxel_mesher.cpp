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

	void push_face (std::vector<VoxelVertex>& buf, float3 pos, float size, int face, int inverse) {
		static constexpr int FACE_INDICES[2][6] {
			{ 1,3,0, 0,3,2 }, // facing in positive dir
			{ 0,2,1, 1,2,3 }, // facing in negative dir
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
		static constexpr uint8v2 UVS[4] {
			uint8v2(0x00, 0x00),
			uint8v2(0xff, 0x00),
			uint8v2(0x00, 0xff),
			uint8v2(0xff, 0xff),
		};

		int out = (int)buf.size();
		buf.resize(out + 6);

		VoxelVertex vertices[4];
		for (int i=0; i<4; ++i) {
			vertices[i].pos_model = FACES[face][i] * size + pos;
			vertices[i].uv = UVS[i];
			vertices[i].tex_indx = 5;
		}

		for (int i=0; i<6; ++i) {
			buf[out++] = vertices[ FACE_INDICES[inverse][i] ];
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
			
			int nb_size;
			block_id nb_bid = find_neighbour(nb_pos.x,nb_pos.y,nb_pos.z, size, &nb_size);

			if (nb_size >= size) {
				// generate face facing to neighbour
				if (should_render_face(bid, nb_bid))
					push_face(opaque_mesh, posf, sizef, face, 0);

				// generate face of neighbour facing us
				if (should_render_face(nb_bid, bid))
					push_face(opaque_mesh, posf, sizef, face, 1);
			}
		}
	}

	block_id find_neighbour (int x, int y, int z, int target_size, int* out_size) {
		int size = 1 << svo.root->scale;
		x -= svo.root->pos.x;
		y -= svo.root->pos.y;
		z -= svo.root->pos.z;
		if (x < 0 || y < 0 || z < 0 || x >= size || y >= size || z >= size) {
			*out_size = size;
			return B_NULL;
		}

		// start with root node
		Chunk* chunk = svo.root;
		Node* node = svo.allocator.get_node(svo.root, 0);

		for (;;) {
			size >>= 1;
			assert(size > 0);

			// get child node that contains target node
			int child_idx = get_child_index(x, y, z, size);

			VoxelType child_type;
			Voxel child_vox = node->get_child(child_idx, &child_type);

			if (size == target_size) {
				// return size if block_id of desired size or bigger is found,
				// else return -1 to signal that the target node is made up of further subdivided voxels
				*out_size = child_type == BLOCK_ID ? size : -1;
				return (block_id)child_vox;
			}

			switch (child_type) {
				case NODE_PTR: {
					// recurse into child node
					node = svo.allocator.get_node(chunk, child_vox);
				} break;

				case CHUNK_PTR: {
					assert(child_vox != 0);
					// leafs in root svo (ie. svo of chunks) are chunks, so recurse into chunk nodes
					chunk = &svo.allocator.chunks[child_vox];
					node = svo.allocator.get_node(chunk, 0);
				} break;

				default: {
					assert(child_type == BLOCK_ID);
					return (block_id)child_vox;
				};
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

				VoxelType type;
				Voxel vox = node->get_child(child_idx, &type);

				if (type == NODE_PTR) {
					// Push
					scale--;
					size >>= 1;

					stack[scale] = { svo.allocator.get_node(chunk, vox), child_pos, 0 };

					continue; // don't inc child indx
				} else {
					assert(type == BLOCK_ID);

					chunk_block(child_pos, size, (block_id)vox);
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
