#include "common.hpp"
#include "blocks.hpp"

void BlockTypes::from_json (json const& blocks_json) {
	ZoneScoped;
	
	{ // B_NULL
		Block b;

		b.name = "null";
		b.hardness = 1;
		b.collision = CM_SOLID;
		//b.transparency = TM_OPAQUE; // don't render edges of world
		b.transparency = TM_TRANSPARENT; // render edges of world

		//name_map.emplace(b.name, (block_id)blocks.size());
		blocks.push_back(std::move(b));
	}

	for (auto& kv : blocks_json["blocks"].items()) {
		Block b;

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
		blocks.push_back(std::move(b));
	}

	air_id = map_id("air");
}
