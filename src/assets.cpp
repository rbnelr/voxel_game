#include "common.hpp"
#include "assets.hpp"
// 
#include "assimp/cimport.h"        // Plain-C interface
#include "assimp/scene.h"          // Output data structure
#include "assimp/postprocess.h"    // Post processing flags
#include "assimp/importer.hpp"

void Assets::load_block_types (json const& blocks_json) {
	ZoneScoped;

	{ // B_NULL
		BlockTypes::Block b;

		b.name = "null";
		b.hardness = 1;
		b.collision = CM_SOLID;
		//b.transparency = TM_OPAQUE; // don't render edges of world
		b.transparency = TM_TRANSPARENT; // render edges of world

										 //name_map.emplace(b.name, (block_id)blocks.size());
		block_types.blocks.push_back(std::move(b));
	}

	for (auto& kv : blocks_json["blocks"].items()) {
		BlockTypes::Block b;

		b.name = kv.key();
		auto val = kv.value();

		// get block 'class' which eases the block config by setting block parameter defaults depending on class
		std_string cls = "solid";

	#define GET(val, member) if ((val).contains(member)) (val).at(member)

		if (val.contains("class")) val.at("class").get_to(cls);

		if (       cls == "solid") {
			b.collision = CM_SOLID;
			b.transparency = TM_OPAQUE;
		} else if (cls == "liquid") {
			b.collision = CM_LIQUID;
			b.transparency = TM_TRANSPARENT;
			b.absorb = 2;
		} else if (cls == "gas") {
			b.collision = CM_GAS;
			b.transparency = TM_TRANSPARENT;
			b.absorb = 1;
		} else if (cls == "deco") {
			b.collision = CM_BREAKABLE;
			b.transparency = TM_PARTIAL;
			b.absorb = 1;
		}

		GET(val, "collision")	.get_to(b.collision);
		GET(val, "transparency").get_to(b.transparency);
		GET(val, "tool")		.get_to(b.tool);
		GET(val, "hardness")	.get_to(b.hardness);
		GET(val, "glow")		.get_to(b.glow);
		GET(val, "absorb")		.get_to(b.absorb);

		//name_map.emplace(b.name, (block_id)blocks.size());
		block_types.blocks.push_back(std::move(b));
	}

	block_types.air_id = block_types.map_id("air");
}

float4 roundv (float4 v) {
	v *= 256.0f;
	v = kissmath::round(v);
	v *= 1.0f / 256.0f;
	return v;
}

void BlockMeshes::load (json const& blocks_json) {
	ZoneScoped;

	auto* scene = aiImportFile("meshes/block_meshes.fbx", aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);

	auto load_mesh = [&] (char const* name) {
		int mesh_idx = -1;

		for (unsigned i=0; i<scene->mNumMeshes; ++i) {
			auto* mesh = scene->mMeshes[i];

			if (strcmp(mesh->mName.C_Str(), name) == 0) {

				BlockMeshes::Mesh m;
				m.offset = (int)slices.size();
				m.length = mesh->mNumFaces / 2; // 2 tris -> one face
				slices.resize(m.offset + m.length);

				for (unsigned j=0; j<mesh->mNumFaces; ++j) {
					auto& f = mesh->mFaces[j];
					assert(f.mNumIndices == 3);

					for (unsigned k=0; k<3; ++k) {
						unsigned index = f.mIndices[k];

						auto pos = mesh->mVertices[index];
						auto norm = mesh->mNormals[index];
						auto uv = mesh->mTextureCoords[0][index];

						slices[m.offset + j/2].vertices[j%2 * 3 + k] = {
							roundv( float4(pos.x, pos.y, pos.z, 1.0f) ),
							roundv( float4(norm.x, norm.y, norm.z, 1.0f) ),
							roundv( float4(uv.x, 1.0f - uv.y, 0.0f, 1.0f) ),
						};
					}
				}

				mesh_idx = (int)meshes.size();
				meshes.push_back(m);
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

	//{
	//	printf("const BlockMeshVertex vertices[%d][%d] = {\n", (int)bm.slices.size(), bm.MERGE_INSTANCE_FACTOR);
	//	for (int i=0; i<(int)bm.slices.size(); ++i) {
	//		printf("\t{\n");
	//		for (int j=0; j<bm.MERGE_INSTANCE_FACTOR; ++j) {
	//			auto& v = bm.slices[i].vertices[j];
	//			printf("\t\tBlockMeshVertex( vec3(%f,%f,%f), vec2(%f,%f) ),\n",
	//				v.pos.x, v.pos.y, v.pos.z,  v.uv.x, v.uv.y);
	//		}
	//		printf("\t},\n");
	//	}
	//	printf("};\n");
	//}

	aiReleaseImport(scene);
}

void Assets::load_block_tiles (json const& blocks_json) {
	ZoneScoped;

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
