#pragma once
#include "common.hpp"
#include "blocks.hpp"

enum item_id : uint32_t {
	I_NULL			=0,

	I_WOOD_PICKAXE	=MAX_BLOCK_ID,
	I_WOOD_SHOVEL	,
	I_WOOD_SWORD	,
};
inline constexpr int ITEM_COUNT = I_WOOD_SWORD+1 - I_WOOD_PICKAXE;

inline constexpr const char* ITEM_NAMES[] = {
	/* I_WOOD_PICKAXE	*/	"wood_pickaxe",
	/* I_WOOD_SHOVEL	*/	"wood_shovel",
	/* I_WOOD_SWORD		*/	"wood_sword",
};

struct ItemProperties {
	ToolType	tool;
	uint8_t		damage;
	uint8_t		hardness;
};

inline constexpr ItemProperties FISTS_PROPS		 = { ToolType::FISTS, 32, 4 };

inline constexpr ItemProperties ITEM_PROPS[] = {
	/* I_WOOD_PICKAXE	*/	{ ToolType::PICKAXE	, 32, 4 },
	/* I_WOOD_SWORD		*/	{ ToolType::SWORD	, 32, 4 },
	/* I_WOOD_SHOVEL	*/	{ ToolType::SHOVEL	, 32, 4 },
};
inline constexpr int ITEM_TILES[] = {
	/* I_WOOD_PICKAXE	*/	{ 0 + 14 * 16 },
	/* I_WOOD_SWORD		*/	{ 0 + 13 * 16 },
	/* I_WOOD_SHOVEL	*/	{ 0 + 12 * 16 },
};

struct ToolState {
	uint8_t hp = 255;
};
struct Item {
	item_id	id;
	union {
		struct {
			uint8_t		count;
		} block;
		struct {
			ToolState	state;
		} item;
	};

	bool is_block () {
		return id != I_NULL && id < MAX_BLOCK_ID;
	}

	ItemProperties const& get_props () {
		if (id == I_NULL)			return FISTS_PROPS;
		else if (id < MAX_BLOCK_ID)	return FISTS_PROPS;
		else						return ITEM_PROPS[id - MAX_BLOCK_ID];
	}
	const char* get_name () {
		if (id == I_NULL)			return "null";
		else if (id < MAX_BLOCK_ID)	return g_assets.block_types[(block_id)id].name.c_str();
		else						return ITEM_NAMES[id - MAX_BLOCK_ID];
	}

	Item () {
		memset(this, 0, sizeof(Item));
	}

	static Item make_item (item_id id, ToolState state={}) {
		assert(id >= MAX_BLOCK_ID);
		Item i;
		i.id = id;
		i.item.state = state;
		return i;
	}
	static Item make_block (item_id id, uint8_t count) {
		assert(id < MAX_BLOCK_ID);
		Item i;
		i.id = id;
		i.block.count = count;
		return i;
	}
};


