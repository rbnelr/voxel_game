#pragma once
#include "common.hpp"
#include "blocks.hpp"

enum item_id : uint32_t {
	I_NULL			=0, // No item

	// All block ids

	I_WOOD_SWORD	=MAX_BLOCK_ID, // Allocate item ids after entire possible block id space, so adding blocks does not shift all item ids
	I_WOOD_PICKAXE	,
	I_WOOD_SHOVEL	,

	ITEM_IDS_COUNT	,
};

// Only store names for the actual items even though the item id range include the block ids
static constexpr const char* ITEM_NAMES[ITEM_IDS_COUNT - MAX_BLOCK_ID] = {
	/* I_WOOD_SWORD		*/	"wood_sword",
	/* I_WOOD_PICKAXE	*/	"wood_pickaxe",
	/* I_WOOD_SHOVEL	*/	"wood_shovel",
};

struct ItemProperties {
	ToolType	tool : 4;
	uint8_t		damage;
	uint8_t		hardness;
};

static constexpr ItemProperties FISTS_PROPS		 = { ToolType::FISTS, 32, 4 };

static constexpr ItemProperties ITEM_PROPS[ITEM_IDS_COUNT - MAX_BLOCK_ID] = {
	/* I_WOOD_SWORD		*/	{ ToolType::SWORD		, 32, 4 },
	/* I_WOOD_PICKAXE	*/	{ ToolType::PICKAXE	, 32, 4 },
	/* I_WOOD_SHOVEL	*/	{ ToolType::SHOVEL	, 32, 4 },
};

static inline constexpr const char* get_item_name (item_id id) {
	if (id < MAX_BLOCK_ID)	return g_blocks.blocks[(block_id)id].name.c_str();
	else					return ITEM_NAMES[id - MAX_BLOCK_ID];
}
static inline constexpr ItemProperties get_item_props (item_id id) {
	if (id < MAX_BLOCK_ID)	return FISTS_PROPS;
	else					return ITEM_PROPS[id - MAX_BLOCK_ID];
}

struct Tool {
	uint8_t hp = 255;
};

struct Item {
	item_id	id = I_NULL;

	// Item state
	union {
		Tool tool;
	};

	ItemProperties get_props () const {
		return get_item_props(id);
	}

	Item () {}
	Item (item_id id): id{id} {
		if (id >= MAX_BLOCK_ID) {
			tool = Tool();
		}
	}
};
