#pragma once
#include "kissmath.hpp"
#include "kissmath_colors.hpp"

#include "stdint.h"

static int texture_res = 16;

static bool unloaded_chunks_traversable = true;

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

enum transparency_mode : uint32_t {
	TM_OPAQUE		=0,
	TM_ALPHA_TEST	, // only surface of a group of transparent blocks of same type is rendered (like water where only the surface is visible)
	TM_TRANSPARENT	, // all faces of these blocks are rendered (like leaves)
};

struct Block_Properties {
	bool				traversable			: 1;
	transparency_mode	transparency		: 2;
	bool				breakable			: 1;
	bool				replaceable			: 1;
	bool				does_autoheal		: 1;
};

static Block_Properties block_props[PSEUDO_BLOCK_TYPES_COUNT] = {
	/* BT_AIR				*/	{ 1, TM_TRANSPARENT,	0, 1, 0 },
	/* BT_WATER				*/	{ 1, TM_TRANSPARENT,	0, 1, 0 },
	/* BT_EARTH				*/	{ 0, TM_OPAQUE,			1, 0, 1 },
	/* BT_GRASS				*/	{ 0, TM_OPAQUE,			1, 0, 1 },
	/* BT_TREE_LOG			*/	{ 0, TM_OPAQUE,			1, 0, 1 },
	/* BT_LEAVES			*/	{ 0, TM_ALPHA_TEST,		1, 0, 1 },
								
	/* BT_OUT_OF_BOUNDS		*/	{ 1, TM_TRANSPARENT,	0, 1, 0 },
	/* BT_NO_CHUNK			*/	{ 1, TM_TRANSPARENT,	0, 1, 0 },
};

static constexpr const char* BLOCK_NAMES[BLOCK_TYPES_COUNT] = {
	/* BT_AIR			*/	nullptr		,
	/* BT_WATER			*/	"water"		,
	/* BT_EARTH			*/	"earth"		,
	/* BT_GRASS			*/	"grass"		,
	/* BT_TREE_LOG		*/	"tree_log"	,
	/* BT_LEAVES		*/	"leaves"	,
};

struct Block {
	block_type	type;
	bool		dark; // any air block that only has air above it (is in sunlight)
	float		hp_ratio;
	lrgba		dbg_tint;
};

static Block B_OUT_OF_BOUNDS = { BT_OUT_OF_BOUNDS, false, 1, 1 };
static Block B_NO_CHUNK = { BT_NO_CHUNK, false, 1, 1 };

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
