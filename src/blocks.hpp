
static s32 texture_res = 16;

static constexpr s32 ATLAS_BLOCK_FACES_COUNT =	3;

static constexpr s32 UVZW_BLOCK_FACE_SIDE =		0;
static constexpr s32 UVZW_BLOCK_FACE_TOP =		1;
static constexpr s32 UVZW_BLOCK_FACE_BOTTOM =	2;

static bool unloaded_chunks_traversable = true;

enum block_type : u8 {
	BT_AIR			=0,
	BT_EARTH		,
	BT_GRASS		,
	BT_WATER		,
	BT_TREE_LOG		,
	BT_TREE_LEAVES	,
	
	BLOCK_TYPES_COUNT,
	
	BT_OUT_OF_BOUNDS	=0xfe,
	BT_NO_CHUNK			=0xff,
};

static bool bt_is_traversable (block_type t) {	return (t == BT_AIR || t == BT_WATER || t == BT_OUT_OF_BOUNDS || (t == BT_NO_CHUNK && unloaded_chunks_traversable)); }
static bool bt_is_breakable (block_type t) {	return !(t == BT_AIR || t == BT_WATER || t == BT_OUT_OF_BOUNDS || t == BT_NO_CHUNK); }
static bool bt_is_transparent (block_type t) {	return (t == BT_AIR || t == BT_WATER || t == BT_OUT_OF_BOUNDS || t == BT_NO_CHUNK); }
static bool bt_is_replaceable (block_type t) {	return (t == BT_AIR || t == BT_WATER); }
static bool bt_does_autoheal (block_type t) {	return (t == BT_EARTH || t == BT_GRASS); }

static cstr block_texture_name[BLOCK_TYPES_COUNT] = {
	/* BT_AIR			*/	"missing.png",
	/* BT_EARTH			*/	"earth.png",
	/* BT_GRASS			*/	"grass.png",
	/* BT_WATER			*/	"water.png",
	/* BT_TREE_LOG		*/	"missing.png",
	/* BT_TREE_LEAVES	*/	"missing.png",
};
static s32 BLOCK_TEXTURE_INDEX_MISSING = 0;

static s32 atlas_textures_count = ARRLEN(block_texture_name);

static s32 get_block_texture_index_from_block_type (block_type bt) {
	return bt;
}

struct Block {
	block_type	type;
	bool		dark; // any air block that only has air above it (is in sunlight)
	f32			hp_ratio;
	lrgba8		dbg_tint;
};

static Block B_OUT_OF_BOUNDS = { BT_OUT_OF_BOUNDS, false, 1, 255 };
static Block B_NO_CHUNK = { BT_NO_CHUNK, false, 1, 255 };

#undef BF_BOTTOM
#undef BF_TOP

enum block_face_e : s32 {
	BF_NEG_X =		0,
	BF_POS_X =		1,
	BF_NEG_Y =		2,
	BF_POS_Y =		3,
	BF_BOTTOM =		4,
	BF_TOP =		5,
};
DEFINE_ENUM_ITER_OPS(block_face_e, s32)
static constexpr block_face_e BF_NEG_Z = (block_face_e)4;
static constexpr block_face_e BF_POS_Z = (block_face_e)5;
