#pragma once
#include "common.hpp"
#include "chunks.hpp"
#include "assets.hpp"

struct WorldGenerator;
struct Assets;

struct ChunkSliceData {
	BlockMeshInstance verts[CHUNK_SLICE_LENGTH];
};

struct ChunkMeshData {
	BlockMeshInstance* next_ptr = nullptr;
	BlockMeshInstance* alloc_end = nullptr;

	std::vector<ChunkSliceData*> slices;

	ChunkMeshData () {
		slices.reserve(32);
	}

	uint32_t vertex_count () {
		return (uint32_t)slices.size() * CHUNK_SLICE_LENGTH - (uint32_t)(alloc_end - next_ptr);
	}

	void alloc_slice () {
		ZoneScopedC(tracy::Color::Crimson);

		auto* s = (ChunkSliceData*)malloc(sizeof(ChunkSliceData));

		next_ptr  = s->verts;
		alloc_end = s->verts + CHUNK_SLICE_LENGTH;

		slices.push_back(s);
	}
	static void free_slice (ChunkSliceData* s) {
		if (s) {
			ZoneScopedC(tracy::Color::Crimson);
			::free(s);
		}
	}

	// forceinline because this is doing nothing but an if and a increment 99% of the time, compiler should keep alloc_slice not inlined instead
	__forceinline BlockMeshInstance* push () {
		if (next_ptr != alloc_end) {
			// likely case
		} else {
			// unlikely case
			alloc_slice();
		}
		return next_ptr++;
	}
};

struct RemeshChunkJob { // Chunk remesh
	//// input data
	// LUTs
	BlockTypes::Block const*	block_types;
	int const*					block_meshes;
	BlockMeshes::Mesh const*	block_meshes_meshes;
	BlockTile const*			block_tiles;

	ChunkVoxels*				chunk_voxels;
	SubchunkVoxels*				subchunks;

	chunk_id					chunk;

	// chunk neighbours (neg dir)
	chunk_id					chunk_nx;
	chunk_id					chunk_ny;
	chunk_id					chunk_nz;

	bool						mesh_world_border;
	uint64_t					chunk_seed;

	//// output data
	ChunkMeshData				opaque_vertices;
	ChunkMeshData				transp_vertices;

	RemeshChunkJob (Chunks& chunks, chunk_id cid, WorldGenerator const& wg, bool mesh_world_border);

	void execute ();
};

inline auto parallelism_threadpool = Threadpool<RemeshChunkJob>(parallelism_threads, TPRIO_PARALLELISM, ">> parallelism threadpool" ); // parallelism_threads - 1 to let main thread contribute work too

//#include "assimp/cimport.h"
//#include "assimp/cexport.h"
//#include "assimp/Exporter.hpp"
//#include "assimp/scene.h"          // Output data structure
//#include "assimp/postprocess.h"    // Post processing flags
//#include "assimp/importer.hpp"

#define EXPORT_CHUNK_MESHES 0

struct ChunkMeshExporter {
#if !EXPORT_CHUNK_MESHES
	void export_ (Chunk& chunk, ChunkMeshData& mesh, bool transparent) {}
	void export_file () {}
#else
	struct Vertex {
		float3 pos;
		float3 norm;
		float2 uv;
	};

	std::unordered_map<int3, std::vector<Vertex>> chunk_meshes;
	std::unordered_map<int3, std::vector<Vertex>> chunk_meshes_trasp;

	float2 uv_from_uvi (float2 mesh_uv, int texid) {
		int2 atlas_tile;
		atlas_tile.x =                    texid % TILEMAP_SIZE.x;
		atlas_tile.y = TILEMAP_SIZE.y-1 - texid / TILEMAP_SIZE.x;

		float2 uv = ((float2)mesh_uv + (float2)atlas_tile) / (float2)TILEMAP_SIZE;
		return uv;
	}

	void export_ (Chunk& chunk, ChunkMeshData& mesh, bool transparent) {
		std::vector<Vertex> chunk_vertices;

		uint32_t remain_vertices = mesh.vertex_count();

		if (remain_vertices == 0)
			return;

		int i = 0;
		while (remain_vertices > 0) {
			uint32_t slice_vertex_count = std::min(remain_vertices, (uint32_t)CHUNK_SLICE_LENGTH);
			auto& slice = *mesh.slices[i++];
			
			for (uint32_t i=0; i<slice_vertex_count; i++) {
				auto& instance = slice.verts[i];

				auto& mesh_slice = g_assets.block_meshes.slices[instance.meshid];
				float3 vox_pos = (float3)(int3(instance.posx, instance.posy, instance.posz)) * 1.0f / BlockMeshInstance_FIXEDPOINT_FAC;
				vox_pos += (float3)(chunk.pos * CHUNK_SIZE);

				for (auto& mesh_vertex : mesh_slice.vertices) {
					Vertex v;

					v.pos = (float3)mesh_vertex.pos + vox_pos;
					v.norm = (float3)mesh_vertex.normal;
					v.uv = uv_from_uvi((float2)mesh_vertex.uv, instance.texid);

					chunk_vertices.push_back(v);
				}
			}

			remain_vertices -= slice_vertex_count;
		}

		// chunks get meshes again when neighbours load, need to replace mesh instead of pushing into vector!
		(transparent ? chunk_meshes_trasp : chunk_meshes)[chunk.pos] = std::move(chunk_vertices);
	}

	//void export_assimp () {
	//	
	//	// https://github.com/assimp/assimp/issues/203
	//	
	//	aiMaterial *material = new aiMaterial();            // deleted: Version.cpp:155
	//	
	//	aiNode *root = new aiNode();                        // deleted: Version.cpp:143
	//	root->mNumMeshes = 0;
	//
	//	aiScene *out = new aiScene();                       // deleted: by us after use
	//	out->mNumMeshes = chunk_meshes.size();
	//	out->mMeshes = new aiMesh * [out->mNumMeshes];            // deleted: Version.cpp:151
	//	out->mNumMaterials = 1;
	//	out->mMaterials = new aiMaterial * [1] { material }; // deleted: Version.cpp:158
	//	out->mRootNode = root; 
	//	out->mMetaData = new aiMetadata(); // workaround, issue #3781
	//
	//	std::vector<aiNode*> children;
	//
	//	for (int i=0; i<chunk_meshes.size(); i++) {
	//		auto& data = chunk_meshes[i];
	//
	//		aiMesh *mesh = new aiMesh();                        // deleted: Version.cpp:150
	//		mesh->mNumVertices = (unsigned)data.size();
	//		mesh->mVertices = new aiVector3D[mesh->mNumVertices];
	//		mesh->mNumFaces = mesh->mNumVertices/3;
	//		mesh->mFaces = new aiFace[mesh->mNumFaces];
	//		mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE; // workaround, issue #3778
	//	
	//		unsigned int idx = 0;
	//		for (unsigned i=0; i<mesh->mNumFaces; i++) {
	//			mesh->mFaces[i].mNumIndices = 3;
	//			mesh->mFaces[i].mIndices = new unsigned int[3]; // <--- Wtf!? Individual heap alloc triangles is clinically insane
	//		
	//			for (int j=0; j<3; j++) {
	//				mesh->mVertices[idx].x = data[idx].pos.x;
	//				mesh->mVertices[idx].y = data[idx].pos.y;
	//				mesh->mVertices[idx].z = data[idx].pos.z;
	//	
	//				mesh->mFaces[i].mIndices[j] = idx++;
	//			}
	//		}
	//
	//		out->mMeshes[i] = mesh;
	//
	//		aiNode *root = new aiNode();                        // deleted: Version.cpp:143
	//		root->mNumMeshes = 1;
	//		root->mMeshes = new unsigned[1] { (unsigned)i };              // deleted: scene.cpp:77
	//
	//		children.push_back(root);
	//	}
	//
	//	root->addChildren(children.size(), children.data());
	//	
	//	Assimp::Exporter exporter;
	//	if (exporter.Export(out, "fbx", "chunk_mesh_export_test.fbx") != AI_SUCCESS)
	//		printf(exporter.GetErrorString());
	//
	//	// deleting the scene will also take care of the vertices, faces, meshes, materials, nodes, etc.
	//
	//	// Doesn't fucking work, let's just leak memory
	//	//delete out;
	//}

	void export_file () {

		FILE* file = fopen("chunk_mesh_export.obj", "w");
		
		fprintf(file, "mtllib chunk_mesh_export.mtl\n");

		{ // Export material
			FILE* file2 = fopen("chunk_mesh_export.mtl", "w");

			fprintf(file2,
				"newmtl VoxelTextures\n"
				"Ns 250.000000\n"
				"Ka 1.000000 1.000000 1.000000\n"
				"Ks 0.500000 0.500000 0.500000\n"
				"Ke 0.000000 0.000000 0.000000\n"
				"Ni 1.500000\n"
				"d 1.000000\n"
				"illum 2\n"
				"map_Kd atlas.png\n"
			);

			fprintf(file2,
				"newmtl VoxelTextures_transparent\n"
				"Ns 250.000000\n"
				"Ka 1.000000 1.000000 1.000000\n"
				"Ks 0.500000 0.500000 0.500000\n"
				"Ke 0.000000 0.000000 0.000000\n"
				"Ni 1.500000\n"
				"d 0.600000\n"
				"illum 6\n"
				"map_Kd atlas.png\n"
			);

			fclose(file2);
		}
		
		int counter = 0;
		int vertex_idx = 0;

		int total = 0;
		
		for (auto& kv : chunk_meshes) {
			if (any(abs(kv.first) <= int3(3))) total++;
		}
		for (auto& kv : chunk_meshes_trasp) {
			if (any(abs(kv.first) <= int3(3))) total++;
		}

		auto push_chunk_mesh = [&] (int3 chunk_pos, std::vector<Vertex>& data, bool transp) {
			if (any(abs(chunk_pos) > int3(3)))
				return;

			printf("Writing .obj chunk data %d/%d...\n", counter+1, total);
			counter++;

			fprintf(file, "o Chunk(%d/%d/%d)%s\n", chunk_pos.x, chunk_pos.y, chunk_pos.z, transp ? "(T)" : "");
		
			for (auto& v : data) {
				fprintf(file, "v %g %g %g\n", v.pos.x, v.pos.y, v.pos.z);
			}
			for (auto& v : data) {
				fprintf(file, "vn %g %g %g\n", v.norm.x, v.norm.y, v.norm.z);
			}
			for (auto& v : data) {
				fprintf(file, "vt %g %g\n", v.uv.x, v.uv.y);
			}
			fprintf(file, "s 0\n");

			fprintf(file, "usemtl %s\n", transp ? "VoxelTextures_transparent" : "VoxelTextures");

			// Vertex indices in .obj are global with multiple meshes!!
			for (int j=0; j<(int)data.size()/3; j++) {
				int idx = vertex_idx+1; // 1 indexed for some reason
				vertex_idx += 3;
				fprintf(file, "f %d/%d/%d %d/%d/%d %d/%d/%d\n", idx,idx,idx, idx+1,idx+1,idx+1, idx+2,idx+2,idx+2);
			}
		};

		for (auto& kv : chunk_meshes) {
			push_chunk_mesh(kv.first, kv.second, false);
		}

		for (auto& kv : chunk_meshes_trasp) {
			push_chunk_mesh(kv.first, kv.second, true);
		}

		fclose(file);
		
	}
#endif
};
static inline ChunkMeshExporter g_ChunkMeshExporter;
