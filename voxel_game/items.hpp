#pragma once
#include "blocks.hpp"
#include "stdint.h"
#include "assert.h"

enum item_id : uint16_t {
	I_NULL			=0, // No item

	// All block ids

	I_WOOD_PICKAXE	=MAX_BLOCK_ID, // Allocate item ids after entire possible block id space, so adding blocks does not shift all item ids
	I_WOOD_SWORD	,
	I_WOOD_SHOVEL	,

	ITEM_IDS_COUNT	,
};

// Only store names for the actual items even though the item id range include the block ids
static constexpr const char* ITEM_NAMES[ITEM_IDS_COUNT - MAX_BLOCK_ID] = {
	/* I_WOOD_PICKAXE	*/	"wood_pickaxe",
	/* I_WOOD_SWORD		*/	"wood_sword",
	/* I_WOOD_SHOVEL	*/	"wood_shovel",
};

enum item_type : uint8_t {
	FISTS, // has no item id, is just what the player holds when the quickbar slot is empty
	BLOCK,
	TOOL,

};

struct ItemProperties {
	item_type	type;
	uint8_t		damage;
	uint8_t		hardness;
};

static constexpr ItemProperties FISTS_PROPS		 = { FISTS, 64, 0 };
static constexpr ItemProperties BLOCK_ITEM_PROPS = { BLOCK, 64, 0 };

static constexpr ItemProperties ITEM_PROPS[ITEM_IDS_COUNT - MAX_BLOCK_ID] = {
	/* I_WOOD_PICKAXE	*/	{ TOOL, 64, 5 },
	/* I_WOOD_SWORD		*/	{ TOOL, 64, 5 },
	/* I_WOOD_SHOVEL	*/	{ TOOL, 64, 5 },
};

static inline constexpr const char* get_item_name (item_id id) {
	if (id < MAX_BLOCK_ID)	return BLOCK_NAMES[(block_id)id];
	else					return ITEM_NAMES[id - MAX_BLOCK_ID];
}
static inline constexpr ItemProperties get_item_props (item_id id) {
	if (id < MAX_BLOCK_ID)	return BLOCK_ITEM_PROPS;
	else					return ITEM_PROPS[id - MAX_BLOCK_ID];
}

struct Tool {
	uint8_t hp = 255;
};

struct Item {
	item_id	id;

	// Item state
	union {
		Tool tool;
	};

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
			switch (get_item_props(id).type) {
				case TOOL:
					tool = Tool();
					break;
				default:
					break;
			}
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
