
//// Gpu voxel data

#if 1
#define CHUNK_SIZE			64 // size of chunk in blocks per axis
#define CHUNK_SIZE_SHIFT	6 // for pos >> CHUNK_SIZE_SHIFT
#define CHUNK_SIZE_MASK		63
#else
#define CHUNK_SIZE			32 // size of chunk in blocks per axis
#define CHUNK_SIZE_SHIFT	5 // for pos >> CHUNK_SIZE_SHIFT
#define CHUNK_SIZE_MASK		31
#endif

#if 0
#define SUBCHUNK_SIZE		4 // size of subchunk in blocks per axis
#define SUBCHUNK_SHIFT		2
#define SUBCHUNK_MASK		3
#else
#define SUBCHUNK_SIZE		8 // size of subchunk in blocks per axis
#define SUBCHUNK_SHIFT		3
#define SUBCHUNK_MASK		7
#endif

#define SUBCHUNK_COUNT		(CHUNK_SIZE / SUBCHUNK_SIZE) // size of chunk in subchunks per axis


#define B_AIR			1
#define B_WATER			3
#define B_EARTH			4
#define B_GRASS			5
#define B_STONE			6
#define B_HARDSTONE		7
#define B_SAND			8
#define B_GRAVEL		9
#define B_MAGMA			12
#define B_CRYSTAL		15
#define B_URANIUM		16
#define B_TREE_LOG		17
#define B_LEAVES		18
#define B_TORCH			19
#define B_TALLGRASS		20
#define B_GLASS			21
#define B_CRYSTAL2		22
#define B_CRYSTAL3		23
#define B_CRYSTAL4		24
#define B_CRYSTAL5		25
#define B_CRYSTAL6		26
#define B_GLOWSHROOM	27

float get_emmisive (uint bid) {
	if (      bid == B_MAGMA      ) return 4.0*  0.4; // 7
	else if ( bid == B_CRYSTAL || (bid >= B_CRYSTAL2 && bid <= B_CRYSTAL6) ) return 4.0*  1.0;
	else if ( bid == B_URANIUM    ) return 4.0*  0.32;
	else if ( bid == B_GLOWSHROOM ) return 4.0*  0.2; // 12
	return 0.0;
}
float get_fake_alpha (uint bid) {
	if      ( bid == B_CRYSTAL || (bid >= B_CRYSTAL2 && bid <= B_CRYSTAL6) ) return 0.1;
	else if ( bid == B_LEAVES )  return 0.9;
	//else if ( bid == B_GLOWSHROOM ) return 2.0; // 12
	return 1.0;
}


// set by shader macro from engine code: WORLD_SIZE_CHUNKS  // number of chunks for fixed subchunk texture (for now)

#define WORLD_SIZE			(WORLD_SIZE_CHUNKS * CHUNK_SIZE)

#define VOXTEX_SIZE			2048 // max width, height, depth of sparse voxel texture (subchunk voxels)
#define VOXTEX_SIZE_SHIFT	11

#define VOXTEX_TEX_SHIFT	(VOXTEX_SIZE_SHIFT - SUBCHUNK_SHIFT)

#define SUBC_SPARSE_BIT		0x80000000u

#define OCTREE_MIPS			10


#define VCT_COL_MAX 4.0
#define VCT_UNPACK vec4(VCT_COL_MAX,VCT_COL_MAX,VCT_COL_MAX, 1.0)


uniform usampler3D voxel_tex;
uniform isampler3D df_tex;

uniform sampler3D vct_tex_mip0;
uniform sampler3D vct_texNX;
uniform sampler3D vct_texPX;
uniform sampler3D vct_texNY;
uniform sampler3D vct_texPY;
uniform sampler3D vct_texNZ;
uniform sampler3D vct_texPZ;
