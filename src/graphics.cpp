#include "common.hpp"
#include "graphics.hpp"
// 
#include "assimp/cimport.h"        // Plain-C interface
#include "assimp/scene.h"          // Output data structure
#include "assimp/postprocess.h"    // Post processing flags
#include "assimp/importer.hpp"

float4 roundv (float4 v) {
	v *= 256.0f;
	v = kissmath::round(v);
	v *= 1.0f / 256.0f;
	return v;
}

BlockMeshes Assets::generate_block_meshes (json const& blocks_json) {
	BlockMeshes bm;

	auto* scene = aiImportFile("meshes/block_meshes.fbx", aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);

	auto load_mesh = [&] (char const* name) {
		int mesh_idx = -1;

		for (unsigned i=0; i<scene->mNumMeshes; ++i) {
			auto* mesh = scene->mMeshes[i];

			if (strcmp(mesh->mName.C_Str(), name) == 0) {

				BlockMeshes::Mesh m;
				m.offset = (int)bm.slices.size();
				m.length = mesh->mNumFaces / 2; // 2 tris -> one face
				bm.slices.resize(m.offset + m.length);

				for (unsigned j=0; j<mesh->mNumFaces; ++j) {
					auto& f = mesh->mFaces[j];
					assert(f.mNumIndices == 3);

					for (unsigned k=0; k<3; ++k) {
						unsigned index = f.mIndices[k];

						auto pos = mesh->mVertices[index];
						auto norm = mesh->mNormals[index];
						auto uv = mesh->mTextureCoords[0][index];

						bm.slices[m.offset + j/2].vertices[j%2 * 3 + k] = {
							roundv( float4(pos.x, pos.y, pos.z, 1.0f) ),
							roundv( float4(norm.x, norm.y, norm.z, 1.0f) ),
							roundv( float4(uv.x, 1.0f - uv.y, 0.0f, 1.0f) ),
						};
					}
				}

				mesh_idx = (int)block_mesh_info.size();
				block_mesh_info.push_back(m);
				break;
			}
		}

		return mesh_idx;
	};

	for (auto name : { "block.negx", "block.posx", "block.negy", "block.posy", "block.negz", "block.posz" })
		load_mesh(name);

	{ // B_NULL
		block_meshes.push_back(-1);
	}

	for (auto& kv : blocks_json["blocks"].items()) {
		auto& name = kv.key();
		auto& val = kv.value();

		auto mesh_name = name;

		if (val.contains("mesh")) mesh_name = val.at("mesh").get_to(mesh_name);

		block_meshes.push_back( load_mesh(mesh_name.c_str()) );
	}

	aiReleaseImport(scene);
	return bm;
}

void Assets::load_block_textures (json const& blocks_json) {
	{ // B_NULL
		block_tiles.push_back({});
	}
	block_id id = 1;

	for (auto& kv : blocks_json["blocks"].items()) {
		auto& name = kv.key();
		auto& val = kv.value();

		//
		BlockTile bt;

		if (val.contains("variants")) val.at("variants").get_to(bt.variants);

		if (val.contains("tex")) {
			auto& tex = val.at("tex");

			auto get_uv = [] (json const& arr) {
				int2 tile = 0;
				if (arr.is_array() && arr.size() >= 2)
					tile = int2(arr[0].get<int>(), arr[1].get<int>());
				return tile.y * TILEMAP_SIZE.x + tile.x;
			};

			if (tex.is_array()) {
				auto idxs = get_uv(tex);
				for (auto& s : bt.sides) s = idxs;

			} else if (tex.is_object()) {
				for (auto& tex : tex.items()) {
					auto idxs = get_uv(tex.value());

					if      (tex.key() == "top")		bt.sides[BF_TOP] = idxs;
					else if (tex.key() == "bottom")		bt.sides[BF_BOTTOM] = idxs;
					else if (tex.key() == "side")		for (int s=0; s<4; ++s) bt.sides[s] = idxs;
					else if (tex.key() == "top-bottom")	bt.sides[BF_BOTTOM] = bt.sides[BF_TOP] = idxs;
				}
			}
		}

		block_tiles.push_back(bt);
	}
}