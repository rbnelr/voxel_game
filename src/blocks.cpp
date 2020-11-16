#include "common.hpp"
#include "blocks.hpp"

void from_json (json const& j, BlockTypes& t) {
	
	for (auto kv : j["blocks"].items()) {
		BlockType b;

		b.name = kv.key();
		auto val = kv.value();

		// get block 'class' which eases the block config by setting block parameter defaults depending on class
		std::string cls = "solid";

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

		if (val.contains("size")) val.at("size").get_to(b.size);
		if (val.contains("mesh")) val.at("mesh").get_to(b.mesh_type);
		if (val.contains("tex-type")) val.at("tex-type").get_to(b.tex_type);
		if (val.contains("variations")) val.at("variations").get_to(b.variations);

		if (val.contains("alpha-test") && val.at("alpha-test").get<bool>()) {
			b.transparency = TM_ALPHA_TEST;
		}

		if (val.contains("collision")) val.at("collision").get_to(b.collision);
		if (val.contains("transparency")) val.at("transparency").get_to(b.transparency);
		if (val.contains("tool")) val.at("tool").get_to(b.tool);
		if (val.contains("hardness")) val.at("hardness").get_to(b.hardness);
		if (val.contains("glow")) val.at("glow").get_to(b.glow);
		if (val.contains("absorb")) val.at("absorb").get_to(b.absorb);

		t.blocks.push_back(std::move(b));
	}
}

BlockTypes load_blocks () {
	return load<BlockTypes>("blocks.json");
}
