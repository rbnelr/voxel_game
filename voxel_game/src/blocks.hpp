#pragma once
#include "stdint.h"

enum collision_mode : uint8_t {
	CM_GAS			=0, // fall/walk through
	CM_SOLID		,   // cannot enter
	CM_LIQUID		,   // swim in (water, etc.)
	CM_BREAKABLE	,	// fall/walk through like gas, but breakable by interaction (breaking torches etc.)
};
enum transparency_mode : uint8_t {
	TM_OPAQUE		=0, // normal blocks which are opaque						:  only opaque to non-opaque faces are rendered
	TM_TRANSPARENT	,   // see-through blocks									:  all faces facing non-opaque blocks except faces facing blocks of the same type are rendered (like water where only the surface is visible)
	TM_PARTIAL		,   // blocks that are see though because they have holes   :  all faces facing non-opaque blocks of these blocks are rendered (like leaves)
	TM_BLOCK_MESH	,	// blocks with meshes									:  these blocks are rendered as meshes (torches, etc.)
};

enum tool_type : uint8_t {
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

	B_ICE1				,
	B_DUST1				,
	B_SHRUBS1			,
	B_GREEN1			,
	B_HOT_ROCK			,

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
	uint8_t				hardness		[PSEUDO_BLOCK_IDS_COUNT]; // hardness value to determine damage resistance
	uint8_t				glow			[PSEUDO_BLOCK_IDS_COUNT]; // with what light level to glow with
	uint8_t				absorb			[PSEUDO_BLOCK_IDS_COUNT]; // how mich light level to absorb (MAX_LIGHT_LEVEL to make block opaque to light)

	inline bool breakable (block_id id) {
		auto& c = collision[id];
		return id != B_NULL && (c == CM_SOLID || c == CM_BREAKABLE);
	}
	inline bool grass_can_live_below (block_id id) {
		return id == B_AIR || transparency[id] == TM_PARTIAL;
	}
};

static BlockTypes load_block_types () {
	BlockTypes bt;
	int cur = 0;

	auto block = [&] (const char* name, collision_mode cm, transparency_mode tm, tool_type tool, uint8_t hard, uint8_t glow, uint8_t absorb) {
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
	auto liquid = [&] (const char* name, transparency_mode transparency=TM_TRANSPARENT, uint8_t glow_level=0) {
		block(name, CM_LIQUID, transparency, NONE, 0, glow_level, transparency == TM_TRANSPARENT ? 3 : MAX_LIGHT_LEVEL);
	};
	auto solid = [&] (const char* name, uint8_t hardness, tool_type tool=NONE, uint8_t glow_level=0) {
		block(name, CM_SOLID, TM_OPAQUE, tool, hardness, glow_level, MAX_LIGHT_LEVEL);
	};
	auto solid_alpha_test = [&] (const char* name, uint8_t hardness, uint8_t absorb_light_level=1, tool_type tool=NONE, uint8_t glow_level=0) {
		block(name, CM_SOLID, TM_PARTIAL, tool, hardness, glow_level, absorb_light_level);
	};
	auto torch = [&] (const char* name, uint8_t glow_level) {
		block(name, CM_BREAKABLE, TM_BLOCK_MESH, NONE, 0, glow_level, 0);
	};
	auto plant = [&] (const char* name, uint8_t absorb_light_level=1) {
		block(name, CM_BREAKABLE, TM_BLOCK_MESH, NONE, 0, 0, 1);
	};

	/* B_NULL				*/ block(			"null", CM_SOLID, TM_TRANSPARENT, NONE, 255, 0, 0);
	/* B_AIR				*/ gas(				"air");
	/* B_WATER				*/ liquid(			"water");
	/* B_STONE				*/ solid(			"stone"	,   20, PICKAXE);
	/* B_EARTH				*/ solid(			"earth"	,    3, SHOVEL );
	/* B_GRASS				*/ solid(			"grass"	,    3, SHOVEL );
	/* B_SAND				*/ solid(			"sand"	,    2, SHOVEL );
	/* B_PEBBLES			*/ solid(			"pebbles",   4, SHOVEL );
	/* B_TREE_LOG			*/ solid(			"tree_log",  7, AXE	 );
	/* B_LEAVES				*/ solid_alpha_test("leaves",    1, 2);
	/* B_TORCH				*/ torch(			"torch", MAX_LIGHT_LEVEL - 1);
	/* B_TALLGRASS			*/ plant(			"tallgrass");

	/* B_ICE1				*/ solid(			"ice1"		,   20, PICKAXE);
	/* B_DUST1				*/ solid(			"dust1"		,   20, PICKAXE);
	/* B_SHRUBS1			*/ solid(			"shrubs1"	,   20, PICKAXE);
	/* B_GREEN1				*/ solid(			"green1"	,   20, PICKAXE);
	/* B_HOT_ROCK			*/ solid(			"hot_rock"	,   20, PICKAXE);

	return bt;
}

static inline BlockTypes blocks = load_block_types();

enum BlockFace : uint8_t {
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