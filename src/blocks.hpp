#pragma once
#include "common.hpp"

typedef uint16_t block_id;

static constexpr block_id B_NULL = (uint16_t)0;
static constexpr uint32_t MAX_BLOCK_ID = 1u << 16;

enum collision_mode : uint8_t {
	CM_GAS			, // fall/walk through
	CM_BREAKABLE	, // fall/walk through like gas, but breakable by interaction (breaking torches etc.)
	CM_LIQUID		, // swim in (water, etc.)
	CM_SOLID		, // cannot enter
};
NLOHMANN_JSON_SERIALIZE_ENUM(collision_mode, {{CM_GAS, "gas"}, {CM_BREAKABLE, "breakable"}, {CM_LIQUID, "liquid"}, {CM_SOLID, "solid"}})

enum transparency_mode : uint8_t {
	TM_TRANSPARENT	, // see-through blocks              :  all faces facing non-opaque blocks except faces facing blocks of the same type are rendered (like water where only the surface is visible)
	TM_ALPHA_TEST	, // blocks with holes               :  all faces facing non-opaque blocks of these blocks are rendered (like leaves)
	TM_PARTIAL		, // objects not filling the voxel   :  all faces of these blocks are rendered (like leaves)
	TM_OPAQUE		, // normal blocks which are opaque  :  only opaque to non-opaque faces are rendered
};
NLOHMANN_JSON_SERIALIZE_ENUM(transparency_mode, {{TM_TRANSPARENT, "transparent"}, {TM_ALPHA_TEST, "alpha_test"}, {TM_PARTIAL, "partial"}, {TM_OPAQUE, "opaque"}})

enum class ToolType : uint8_t {
	NONE,
	FISTS,
	SWORD,
	PICKAXE,
	AXE,
	SHOVEL,
};
NLOHMANN_JSON_SERIALIZE_ENUM(ToolType, {{ToolType::NONE, nullptr}, {ToolType::FISTS, "fists"}, {ToolType::SWORD, "sword"}, {ToolType::PICKAXE, "pickaxe"}, {ToolType::AXE, "axe"}, {ToolType::SHOVEL, "shovel"}})

inline constexpr float TOOL_MATCH_BONUS_DAMAGE = 2;
inline constexpr float TOOL_MISMATCH_PENALTY_BREAK = 2;
inline constexpr int MAX_LIGHT_LEVEL = 18;

enum class BlockMeshType : uint8_t {
	BLOCK, // normal 1^3 block
	BOX, // non 1^3 sized box shape (size is the size in 1/16th)
	X, // X shaped sprite plant
};
NLOHMANN_JSON_SERIALIZE_ENUM(BlockMeshType, {{BlockMeshType::BLOCK, nullptr}, {BlockMeshType::BOX, "box"}, {BlockMeshType::X, "X"}})

enum class TexType : uint8_t {
	NORMAL,
	TOP_SIDE,
	TOP_SIDE_BOTTOM,
};
NLOHMANN_JSON_SERIALIZE_ENUM(TexType, {{TexType::NORMAL, nullptr}, {TexType::TOP_SIDE, "top-side"}, {TexType::TOP_SIDE_BOTTOM, "top-side-bottom"}})

struct BlockType {
	std::string			name = "null";

	uint8v3				size = 16;
	BlockMeshType		mesh_type = BlockMeshType::BLOCK;
	TexType				tex_type = TexType::NORMAL;
	uint8_t				variations = 1;

	collision_mode		collision;                  // collision mode for physics
	transparency_mode	transparency;               // transparency mode for meshing
	ToolType			tool = ToolType::NONE;      // tool type to determine which tool should be used for mining
	uint8_t				hardness = 0;               // hardness value to determine damage resistance
	uint8_t				glow = 0;                   // with what light level to glow with
	uint8_t				absorb = MAX_LIGHT_LEVEL;   // how mich light level to absorb (MAX_LIGHT_LEVEL to make block opaque to light)

};

struct BlockTypes {
	
	std::vector<BlockType> blocks;

	block_id air_id;

	block_id map_id (std::string_view name) {
		int id = kiss::indexof(blocks, name, [this] (BlockType const& l, std::string_view r) {
			return l.name == r;
		});
		if (id == -1) return B_NULL;
		return (block_id)id;
	}
};

BlockTypes load_blocks ();

inline BlockTypes g_blocks;

inline bool grass_can_live_below (block_id id) {
	auto& b = g_blocks.blocks[id];
	return b.transparency != TM_OPAQUE && b.collision <= CM_LIQUID;
}

inline bool block_breakable (block_id id) {
	auto& b = g_blocks.blocks[id];
	return b.collision == CM_SOLID || b.collision == CM_BREAKABLE;
}

// Block instance
struct Block {
	block_id	id;
	uint8_t		block_light;
	uint8_t		sky_light;
	uint8_t		hp;

	Block () = default;

	Block (block_id id): id{id} {
		block_light = g_blocks.blocks[id].glow;
		sky_light = 0;
		hp = 255;
	}
};

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
