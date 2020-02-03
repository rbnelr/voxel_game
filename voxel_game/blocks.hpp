#pragma once
#include "kissmath.hpp"
#include "kissmath_colors.hpp"

#include "stdint.h"

enum collision_mode : uint8_t {
	CM_GAS			=0, // fall through
	CM_SOLID		,   // cannot enter
	CM_LIQUID		,   // swim in (water, etc.)
};
enum transparency_mode : uint8_t {
	TM_OPAQUE		=0, // only opaque to non-opaque faces are rendered
	TM_TRANSPARENT	,   // all faces facing non-opaque blocks except faces facing blocks of the same type are rendered (like water where only the surface is visible)
	TM_ALPHA_TEST	,   // all faces facing non-opaque blocks of these blocks are rendered (like leaves)
};

struct Block_Properties {
	collision_mode		collision;
	transparency_mode	transparency;
};

enum block_type : uint8_t {
	BT_AIR				=0,
	BT_WATER			,
	BT_EARTH			,
	BT_GRASS			,
	BT_TREE_LOG			,
	BT_LEAVES			,

	BLOCK_TYPES_COUNT	,

	BT_OUT_OF_BOUNDS	=BLOCK_TYPES_COUNT,
	BT_NO_CHUNK			,

	PSEUDO_BLOCK_TYPES_COUNT,
};

static constexpr const char* BLOCK_NAMES[BLOCK_TYPES_COUNT] = {
	/* BT_AIR			*/	nullptr		,
	/* BT_WATER			*/	"water"		,
	/* BT_EARTH			*/	"earth"		,
	/* BT_GRASS			*/	"grass"		,
	/* BT_TREE_LOG		*/	"tree_log"	,
	/* BT_LEAVES		*/	"leaves"	,
};

static Block_Properties block_props[PSEUDO_BLOCK_TYPES_COUNT] = {
	/* BT_AIR				*/	{ CM_GAS	, TM_TRANSPARENT	 },
	/* BT_WATER				*/	{ CM_LIQUID	, TM_TRANSPARENT	 },
	/* BT_EARTH				*/	{ CM_SOLID	, TM_OPAQUE			 },
	/* BT_GRASS				*/	{ CM_SOLID	, TM_OPAQUE			 },
	/* BT_TREE_LOG			*/	{ CM_SOLID	, TM_OPAQUE			 },
	/* BT_LEAVES			*/	{ CM_SOLID	, TM_ALPHA_TEST		 },
								
	/* BT_OUT_OF_BOUNDS		*/	{ CM_GAS	, TM_TRANSPARENT	 },
	/* BT_NO_CHUNK			*/	{ CM_SOLID	, TM_TRANSPARENT	 },
};

struct Block {
	block_type	type;
	bool		dark; // any air block that only has air above it (is in sunlight)
	float		hp_ratio;
	lrgba		dbg_tint;
};

// global block instances for pseudo blocks to allow returning Block* to these for out of chunk queries
static constexpr inline Block B_OUT_OF_BOUNDS = { BT_OUT_OF_BOUNDS, false, 1, 1 };
static constexpr inline Block B_NO_CHUNK      = { BT_NO_CHUNK     , false, 1, 1 };

// garbage from windows.h
#undef BF_BOTTOM
#undef BF_TOP

enum BlockFace {
	BF_NEG_X	=0,
	BF_POS_X	,
	BF_NEG_Y	,
	BF_POS_Y	,
	BF_NEG_Z	,
	BF_POS_Z	,

	BF_BOTTOM	=BF_NEG_Z,
	BF_TOP		=BF_POS_Z,
};
