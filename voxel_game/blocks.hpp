#pragma once
#include "kissmath.hpp"

enum block_id : uint8_t {
	B_NULL				=0,
	B_AIR				,
	B_WATER				,
	B_EARTH				,
	B_GRASS				,
	B_TREE_LOG			,
	B_LEAVES			,

	BLOCK_IDS_COUNT		,

	B_OUT_OF_BOUNDS		=BLOCK_IDS_COUNT,
	B_NO_CHUNK			,

	PSEUDO_BLOCK_IDS_COUNT,
};

static constexpr unsigned int MAX_BLOCK_ID = 1u << (sizeof(block_id)*8);

static constexpr const char* BLOCK_NAMES[BLOCK_IDS_COUNT] = {
	/* B_NULL			*/	"null"		,
	/* B_AIR			*/	nullptr		,
	/* B_WATER			*/	"water"		,
	/* B_EARTH			*/	"earth"		,
	/* B_GRASS			*/	"grass"		,
	/* B_TREE_LOG		*/	"tree_log"	,
	/* B_LEAVES			*/	"leaves"	,
};

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

struct BlockProperties {
	collision_mode		collision;
	transparency_mode	transparency;
};

static BlockProperties BLOCK_PROPS[PSEUDO_BLOCK_IDS_COUNT] = {
	/* B_NULL				*/	{ CM_SOLID	, TM_OPAQUE			 },
	/* B_AIR				*/	{ CM_GAS	, TM_TRANSPARENT	 },
	/* B_WATER				*/	{ CM_LIQUID	, TM_TRANSPARENT	 },
	/* B_EARTH				*/	{ CM_SOLID	, TM_OPAQUE			 },
	/* B_GRASS				*/	{ CM_SOLID	, TM_OPAQUE			 },
	/* B_TREE_LOG			*/	{ CM_SOLID	, TM_OPAQUE			 },
	/* B_LEAVES				*/	{ CM_SOLID	, TM_ALPHA_TEST		 },
							
	/* B_OUT_OF_BOUNDS		*/	{ CM_GAS	, TM_TRANSPARENT	 },
	/* B_NO_CHUNK			*/	{ CM_SOLID	, TM_TRANSPARENT	 },
};

struct Block {
	block_id	id;
	bool		dark; // any air block that only has air above it (is in sunlight)
	uint8		hp;
};

// global block instances for pseudo blocks to allow returning Block* to these for out of chunk queries
static constexpr inline Block _OUT_OF_BOUNDS = { B_OUT_OF_BOUNDS, false, 255 };
static constexpr inline Block _NO_CHUNK      = { B_NO_CHUNK     , false, 255 };

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
