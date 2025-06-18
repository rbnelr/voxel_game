#pragma once
#include "common.hpp"
#include "items.hpp"

struct PlayerInventory {
	bool is_open = false;

	struct Backpack {
		Item slots[10][10];

		Backpack () {
			for (int bid=1; bid<g->assets->block_types.count(); ++bid) {
				int i = bid-1;
				if (i < 10*10) {
					slots[i/10][i%10] = Item::make_block((item_id)bid, 1);
				}
			}
		}
	};

	struct Toolbar {
		Item slots[10];

		int selected = 0;

		Item& get_selected () {
			return slots[selected];
		}

		Toolbar () {
			slots[0] = Item::make_item( I_WOOD_SWORD   );
			slots[1] = Item::make_item( I_WOOD_PICKAXE );
			slots[2] = Item::make_item( I_WOOD_SHOVEL  );
			slots[3] = Item::make_block( (item_id)g->assets->block_types.map_id("earth")    , 1 );
			slots[4] = Item::make_block( (item_id)g->assets->block_types.map_id("grass")    , 1 );
			slots[5] = Item::make_block( (item_id)g->assets->block_types.map_id("stone")    , 1 );
			slots[6] = Item::make_block( (item_id)g->assets->block_types.map_id("tree_log") , 1 );
			slots[7] = Item::make_block( (item_id)g->assets->block_types.map_id("leaves")   , 1 );
			slots[8] = Item::make_block( (item_id)g->assets->block_types.map_id("water")    , 1 );
			slots[9] = Item::make_block( (item_id)g->assets->block_types.map_id("glass")    , 1 );
		}
	};

	Backpack	backpack;
	Toolbar		toolbar;
	Item		hand; // drag&drop picks up items to the 'hand'

	PlayerInventory () {
		hand = {};
	}
};
