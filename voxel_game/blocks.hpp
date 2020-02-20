#pragma once
#include "kissmath.hpp"

enum collision_mode : uint8 {
	CM_GAS			=0, // fall/walk through
	CM_SOLID		,   // cannot enter
	CM_LIQUID		,   // swim in (water, etc.)
	CM_BREAKABLE	,	// fall/walk through, but breakable (torches etc.)
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

static inline float TOOL_MATCH_BONUS_DAMAGE = 2;
static inline float TOOL_MISMATCH_PENALTY_BREAK = 2;

struct BlockProperties {
	const char*			name;
	collision_mode		collision;
	transparency_mode	transparency;
	tool_type			tool = NONE;
	uint8				hardness = 255;
	uint8				glow_level = 0;
	uint8				absorb_light_level = 15;
};

enum block_id : uint8 {
#define MAX_BLOCK_ID (1u << (sizeof(block_id)*8))

	B_NULL				=0,
	B_AIR				,
	B_WATER				,
	B_EARTH				,
	B_GRASS				,
	B_STONE				,
	B_TREE_LOG			,
	B_LEAVES			,
	B_TORCH				,

	BLOCK_IDS_COUNT		,

	B_OUT_OF_BOUNDS		=BLOCK_IDS_COUNT,
	B_NO_CHUNK			,

	PSEUDO_BLOCK_IDS_COUNT,
};
static BlockProperties BLOCK_PROPS[PSEUDO_BLOCK_IDS_COUNT] = {
	/* B_NULL				*/	{ "null"		, CM_SOLID		, TM_OPAQUE			, NONE		, 1, 0 },
	/* B_AIR				*/	{ "null"		, CM_GAS		, TM_TRANSPARENT	, NONE		, 0, 0, 0 },
	/* B_WATER				*/	{ "water"		, CM_LIQUID		, TM_TRANSPARENT	, NONE		, 0, 0, 1 },
	/* B_EARTH				*/	{ "earth"		, CM_SOLID		, TM_OPAQUE			, SHOVEL	, 3 },
	/* B_GRASS				*/	{ "grass"		, CM_SOLID		, TM_OPAQUE			, SHOVEL	, 3 },
	/* B_STONE				*/	{ "stone"		, CM_SOLID		, TM_OPAQUE			, PICKAXE	, 20 },
	/* B_TREE_LOG			*/	{ "tree_log"	, CM_SOLID		, TM_OPAQUE			, AXE		, 7 },
	/* B_LEAVES				*/	{ "leaves"		, CM_SOLID		, TM_ALPHA_TEST		, NONE		, 2, 0, 1 },
	/* B_TORCH				*/	{ "null"		, CM_BREAKABLE	, TM_PARTIAL		, NONE		, 0, 15, 0 },
	
	/* B_OUT_OF_BOUNDS		*/	{ "null"		, CM_GAS		, TM_TRANSPARENT	, NONE		, 0, 0, 0 },
	/* B_NO_CHUNK			*/	{ "null"		, CM_SOLID		, TM_TRANSPARENT	, NONE		, 0, 0, 0 },
};

static inline bool grass_can_live_below (block_id id) {
	return id == B_AIR || id == B_OUT_OF_BOUNDS || BLOCK_PROPS[id].transparency == TM_PARTIAL;
}
static inline bool breakable (block_id id) {
	auto& props = BLOCK_PROPS[id];
	return props.collision == CM_SOLID || props.collision == CM_BREAKABLE;
}

// Block instance
struct Block {
	block_id	id;
	uint8		light_level = 0; // [0,15]
	uint8		hp = 255;

	Block () {}

	Block (block_id id): id{id} {
		light_level = BLOCK_PROPS[id].glow_level;
	}
};

// global block instances for pseudo blocks to allow returning Block* to these for out of chunk queries
static inline Block _OUT_OF_BOUNDS = B_OUT_OF_BOUNDS;
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
