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
		b.hardness = MAX_HARDNESS;
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
		std::string cls = "solid";

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

// why was I rounding here?
// maybe I was just paranoid about blender having floats be slightly off?
// anyway, rounding to 1/256 should not hurt I think (even normals?)
float4 roundv (float4 v) {
	v *= 256.0f;
	v = kissmath::round(v);
	v *= 1.0f / 256.0f;
	return v;
}

void BlockMeshes::load (json const& blocks_json) {
	ZoneScoped;

	auto* scene = aiImportFile("meshes/block_meshes.fbx", aiProcess_Triangulate | aiProcess_CalcTangentSpace); // aiProcess_JoinIdenticalVertices
	if (!scene)
		throw std::runtime_error("meshes/block_meshes.fbx not found!");

	auto load_mesh = [&] (char const* name, float offs_strength=1.0f) {
		static constexpr float scale = 1.0f / 16;

		int mesh_idx = -1;

		for (unsigned i=0; i<scene->mNumMeshes; ++i) {
			auto* mesh = scene->mMeshes[i];

			if (strcmp(mesh->mName.C_Str(), name) == 0) {

				BlockMeshes::Mesh m;
				m.index = (int)slices.size();
				m.length = mesh->mNumFaces / 2; // 2 tris -> one face
				m.offs_strength = offs_strength;
				slices.resize(m.index + m.length);

				for (unsigned j=0; j<mesh->mNumFaces; ++j) {
					auto& f = mesh->mFaces[j];
					assert(f.mNumIndices == 3);

					for (unsigned k=0; k<3; ++k) {
						unsigned index = f.mIndices[k];

						auto pos = mesh->mVertices[index];
						auto norm = mesh->mNormals[index];
						auto tang = mesh->mTangents[index];
						auto uv = mesh->mTextureCoords[0][index];

						slices[m.index + j/2].vertices[j%2 * 3 + k] = {
							roundv( float4(pos.x * scale, pos.y * scale, pos.z * scale, 1.0f) ),
							roundv( float4(norm.x, norm.y, norm.z, 1.0f) ),
							roundv( float4(tang.x, tang.y, tang.z, 1.0f) ),
							roundv( float4(uv.x, uv.y, 0.0f, 1.0f) ),
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

		int mesh = -1;
		if (val.contains("mesh")) {
			std::string mesh_name;
			val.at("mesh").get_to(mesh_name);
			
			float offs_strength = 1.0f;
			if (val.contains("offs-strength")) val.at("offs-strength").get_to(offs_strength);

			mesh = load_mesh(mesh_name.c_str(), offs_strength);
		}

		block_meshes.push_back(mesh);
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


#if 0
// Can use mTransformation in node tree to avoid having to apply transformations in blender,
// but for some reason the object names do not appear in the fbx? only the mesh names do
void print_fbx (aiNode const* node, int depth, float scale, float4x4 transform) {
	auto& t = node->mTransformation;
	float4x4 m = float4x4(t.a1,t.a2,t.a3,t.a4,  t.b1,t.b2,t.b3,t.b4, t.c1,t.c2,t.c3,t.c4,  t.d1,t.d2,t.d3,t.d4);
	m = transform * m;

	float3 pos = (float3)(m * float4(0,0,0,1)) * scale;

	for (int i=0; i<depth; ++i)
		printf("  ");
	printf("%s %f,%f,%f\n", node->mName.C_Str(), pos.x, pos.y, pos.z);

	for (unsigned int i=0; i<node->mNumChildren; ++i)
		print_fbx(node->mChildren[i], depth+1, scale, m);
}
void print_fbx (aiScene const* scene) {
	float scale = 1.0f / 100; // fbx seems to use centimeter units (with UnitScaleFactor 1) 

	for (unsigned int i=0; i<scene->mMetaData->mNumProperties; ++i) {
		if (strcmp(scene->mMetaData->mKeys[i].C_Str(), "UnitScaleFactor") == 0) {
			if (scene->mMetaData->mValues[i].mType == AI_DOUBLE) scale *= (float)*(double*)scene->mMetaData->mValues[i].mData;
			if (scene->mMetaData->mValues[i].mType == AI_FLOAT)  scale *=        *(float*) scene->mMetaData->mValues[i].mData;
		}
	}

	print_fbx(scene->mRootNode, 0, scale, float4x4::identity());
}
#endif

GenericSubmesh load_subobject (GenericVertexData* data, aiScene const* scene, std::string_view subobj) {
	GenericSubmesh m = {};

	if (scene) {
		for (unsigned i=0; i<scene->mNumMeshes; ++i) {
			auto* mesh = scene->mMeshes[i];

			if (subobj.compare(mesh->mName.C_Str()) == 0) {

				m.base_vertex = (uint32_t)data->vertices.size();
				m.index_offs  = (uint32_t)data->indices.size();

				for (unsigned j=0; j<mesh->mNumVertices; ++j) {
					data->vertices.emplace_back();
					GenericVertex& v = data->vertices.back();

					auto& pos = mesh->mVertices[j];
					auto& norm = mesh->mNormals[j];
					auto* uv = &mesh->mTextureCoords[0][j];
					auto* col = &mesh->mColors[0][j];

					v.pos = float3(pos.x, pos.y, pos.z);
					v.norm = float3(norm.x, norm.y, norm.z);
					v.uv = mesh->mTextureCoords[0] ? float2(uv->x, uv->y) : 0.0f;
					v.col = mesh->mColors[0] ? float4(col->r, col->g, col->b, col->a) : lrgba(1,1,1,1);
				}

				for (unsigned j=0; j<mesh->mNumFaces; ++j) {
					auto& f = mesh->mFaces[j];
					assert(f.mNumIndices == 3);

					for (unsigned k=0; k<3; ++k) {
						data->indices.push_back( (uint32_t)f.mIndices[k] );
					}
				}

				m.index_count  = (uint32_t)(mesh->mNumFaces * 3);
			
				return m;
			}
		}
	}

	clog(ERROR, "[load_subobject] Suboject %s not found in fbx!", subobj);
	return m;
}

BlockHighlightSubmeshes load_block_highlight_mesh (GenericVertexData* data) {
	auto* block_highlight_fbx = aiImportFile("meshes/block_highlight.fbx", aiProcess_Triangulate|aiProcess_JoinIdenticalVertices);
	//print_fbx(block_highlight_fbx);
	
	BlockHighlightSubmeshes bh;
	bh.block_highlight = load_subobject(data, block_highlight_fbx, "block_highlight");
	bh.face_highlight  = load_subobject(data, block_highlight_fbx, "face_highlight");

	aiReleaseImport(block_highlight_fbx);
	return bh;
}

GenericVertexData load_fbx (char const* filename, char const* submesh_name) {
	auto* fbx = aiImportFile(filename, aiProcess_Triangulate|aiProcess_JoinIdenticalVertices);
	
	GenericVertexData data;
	auto m = load_subobject(&data, fbx, submesh_name);

	aiReleaseImport(fbx);
	return data;
}
