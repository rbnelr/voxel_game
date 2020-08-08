#pragma once
#include "stdafx.hpp"

enum collision_mode : uint8 {
	CM_GAS			=0, // fall/walk through
	CM_SOLID		,   // cannot enter
	CM_LIQUID		,   // swim in (water, etc.)
	CM_BREAKABLE	,	// fall/walk through like gas, but breakable by interaction (breaking torches etc.)
};
enum transparency_mode : uint8 {
	TM_OPAQUE		=0, // normal blocks which are opaque  :  only opaque to non-opaque faces are rendered
	TM_TRANSPARENT	,   // see-through blocks              :  all faces facing non-opaque blocks except faces facing blocks of the same type are rendered (like water where only the surface is visible)
	TM_ALPHA_TEST	,   // blocks with holes               :  all faces facing non-opaque blocks of these blocks are rendered (like leaves)
	TM_PARTIAL		,	// objects not filling the voxel   :  all faces of these blocks are rendered (like leaves)
};

enum tool_type : uint8 {
	NONE,
	FISTS,
	SWORD,
	PICKAXE,
	AXE,
	SHOVEL,
};

enum block_id : uint16_t {
#define MAX_BLOCK_ID (1u << (sizeof(block_id)*8))

	B_NULL				=0,
	B_AIR				,
	B_WATER				,
	B_STONE				,
	B_EARTH				,
	B_GRASS				,
	B_SAND				,
	B_PEBBLES			,
	B_TREE_LOG			,
	B_LEAVES			,
	B_TORCH				,
	B_TALLGRASS			,

	BLOCK_IDS_COUNT		,

	B_NO_CHUNK			=BLOCK_IDS_COUNT,

	PSEUDO_BLOCK_IDS_COUNT,
};

static constexpr float TOOL_MATCH_BONUS_DAMAGE = 2;
static constexpr float TOOL_MISMATCH_PENALTY_BREAK = 2;
#define MAX_LIGHT_LEVEL 18

struct BlockTypes {
	const char*			name			[PSEUDO_BLOCK_IDS_COUNT]; // name for texture and ui
	collision_mode		collision		[PSEUDO_BLOCK_IDS_COUNT]; // collision mode to determine 
	transparency_mode	transparency	[PSEUDO_BLOCK_IDS_COUNT]; // transparency mode for meshing
	tool_type			tool			[PSEUDO_BLOCK_IDS_COUNT]; // tool type to determine which tool should be used for mining
	uint8				hardness		[PSEUDO_BLOCK_IDS_COUNT]; // hardness value to determine damage resistance
	uint8				glow			[PSEUDO_BLOCK_IDS_COUNT]; // with what light level to glow with
	uint8				absorb			[PSEUDO_BLOCK_IDS_COUNT]; // how mich light level to absorb (MAX_LIGHT_LEVEL to make block opaque to light)

	inline bool breakable (block_id id) {
		auto& c = collision[id];
		return c == CM_SOLID || c == CM_BREAKABLE;
	}
	inline bool grass_can_live_below (block_id id) {
		return id == B_AIR || transparency[id] == TM_PARTIAL;
	}
};

static BlockTypes load_block_types () {
	BlockTypes bt;
	int cur = 0;

	auto block = [&] (const char* name, collision_mode cm, transparency_mode tm, tool_type tool, uint8 hard, uint8 glow, uint8 absorb) {
		bt.name[cur] = name;
		bt.collision[cur] = cm;
		bt.transparency[cur] = tm;
		bt.tool[cur] = tool;
		bt.hardness[cur] = hard;
		bt.glow[cur] = glow;
		bt.absorb[cur] = absorb;
		cur++;
	};
	auto gas = [&] (const char* name="null") {
		block(name, CM_GAS, TM_TRANSPARENT, NONE, 0, 0, 0);
	};
	auto liquid = [&] (const char* name, transparency_mode transparency=TM_TRANSPARENT, uint8 glow_level=0) {
		block(name, CM_LIQUID, transparency, NONE, 0, glow_level, transparency == TM_TRANSPARENT ? 3 : MAX_LIGHT_LEVEL);
	};
	auto solid = [&] (const char* name, uint8 hardness, tool_type tool=NONE, uint8 glow_level=0) {
		block(name, CM_SOLID, TM_OPAQUE, tool, hardness, glow_level, MAX_LIGHT_LEVEL);
	};
	auto solid_alpha_test = [&] (const char* name, uint8 hardness, uint8 absorb_light_level=1, tool_type tool=NONE, uint8 glow_level=0) {
		block(name, CM_SOLID, TM_ALPHA_TEST, tool, hardness, glow_level, absorb_light_level);
	};
	auto torch = [&] (const char* name, uint8 glow_level) {
		block(name, CM_BREAKABLE, TM_PARTIAL, NONE, 0, glow_level, 0);
	};
	auto plant = [&] (const char* name, uint8 absorb_light_level=1) {
		block(name, CM_BREAKABLE, TM_PARTIAL, NONE, 0, 0, 1);
	};

	/* B_NULL				*/ solid("null", 1);
	/* B_AIR				*/ gas();
	/* B_WATER				*/ liquid("water");
	/* B_STONE				*/ solid(			"stone"	,   20, PICKAXE);
	/* B_EARTH				*/ solid(			"earth"	,    3, SHOVEL );
	/* B_GRASS				*/ solid(			"grass"	,    3, SHOVEL );
	/* B_SAND				*/ solid(			"sand"	,    2, SHOVEL );
	/* B_PEBBLES			*/ solid(			"pebbles",   4, SHOVEL );
	/* B_TREE_LOG			*/ solid(			"tree_log",  7, AXE	 );
	/* B_LEAVES				*/ solid_alpha_test("leaves",    1, 2);
	/* B_TORCH				*/ torch("torch", MAX_LIGHT_LEVEL - 1);
	/* B_TALLGRASS			*/ plant("tallgrass");

	/* B_NO_CHUNK			*/ block("null", CM_SOLID, TM_TRANSPARENT, NONE, 0, 0, 0);

	return bt;
}

static inline BlockTypes blocks = load_block_types();

// Block instance
struct Block {
	block_id	id;
	uint8_t		block_light;
	uint8_t		sky_light;
	uint8_t		hp;

	Block () = default;

	Block (block_id id): id{id} {
		block_light = blocks.glow[id];
		sky_light = 0;
		hp = 255;
	}
	Block (block_id	id, uint8_t block_light, uint8_t sky_light, uint8_t hp):
			id{id}, block_light{block_light}, sky_light{sky_light}, hp{hp} {

	}
};

// global block instances for pseudo blocks to allow returning Block* to these for out of chunk queries
static inline Block _NO_CHUNK      = B_NO_CHUNK     ;

enum BlockFace {
	BF_NULL		=1,

	BF_NEG_X	=0,
	BF_POS_X	,
	BF_NEG_Y	,
	BF_POS_Y	,
	BF_NEG_Z	,
	BF_POS_Z	,

	BF_BOTTOM	=BF_NEG_Z,
	BF_TOP		=BF_POS_Z,
};
