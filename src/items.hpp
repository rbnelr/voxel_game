#pragma once
#include "blocks.hpp"
#include "stdint.h"
#include "assert.h"

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
	tool_type	tool : 4;
	uint8		damage;
	uint8		hardness;
};

static constexpr ItemProperties FISTS_PROPS		 = { FISTS, 32, 4 };

static constexpr ItemProperties ITEM_PROPS[ITEM_IDS_COUNT - MAX_BLOCK_ID] = {
	/* I_WOOD_SWORD		*/	{ SWORD		, 32, 4 },
	/* I_WOOD_PICKAXE	*/	{ PICKAXE	, 32, 4 },
	/* I_WOOD_SHOVEL	*/	{ SHOVEL	, 32, 4 },
};

static inline constexpr const char* get_item_name (item_id id) {
	if (id < MAX_BLOCK_ID)	return blocks.name[(block_id)id];
	else					return ITEM_NAMES[id - MAX_BLOCK_ID];
}
static inline constexpr ItemProperties get_item_props (item_id id) {
	if (id < MAX_BLOCK_ID)	return FISTS_PROPS;
	else					return ITEM_PROPS[id - MAX_BLOCK_ID];
}

struct Tool {
	uint8 hp = 255;
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

	Item () {
		memset(this, 0, sizeof(Item));
	}
	Item (item_id id) {
		// zero potential unused memory
		memset(this, 0, sizeof(Item));
		// set id
		this->id = id;
		// init correct part of union
		if (id >= MAX_BLOCK_ID) {
			tool = Tool();
		}
	}

	Item (Item const& other) {
		memcpy(this, &other, sizeof(Item));
	}
	Item (Item&& other) {
		memcpy(this, &other, sizeof(Item));
	}
	Item& operator= (Item const& other) {
		memcpy(this, &other, sizeof(Item));
		return *this;
	}
	Item& operator= (Item&& other) {
		memcpy(this, &other, sizeof(Item));
		return *this;
	}
};
