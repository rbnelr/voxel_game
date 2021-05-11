
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
	if (      bid == B_MAGMA   ) return 7.0;
	else if ( bid == B_CRYSTAL ) return 6.0;
	else if ( bid == B_URANIUM ) return 3.2;
	else if ( bid >= B_CRYSTAL2 && bid <= B_CRYSTAL6 ) return 6.0;
	else if ( bid == B_GLOWSHROOM ) return 12.0;
	return 0.0;
}


#define WORLD_SIZE_CHUNKS	8 // number of chunks for fixed subchunk texture (for now)
#define WORLD_SIZE			(WORLD_SIZE_CHUNKS * CHUNK_SIZE)

#define VOXTEX_SIZE			2048 // max width, height, depth of sparse voxel texture (subchunk voxels)
#define VOXTEX_SIZE_SHIFT	11

#define VOXTEX_TEX_SHIFT	(VOXTEX_SIZE_SHIFT - SUBCHUNK_SHIFT)

#define SUBC_SPARSE_BIT		0x80000000u

#define OCTREE_MIPS			10

uniform usampler3D	subchunks_tex;
uniform usampler3D	voxels_tex;
uniform usampler3D	octree;
		
uniform sampler3D	vct_basetex;
uniform sampler3D	vct_texNX;
uniform sampler3D	vct_texPX;
uniform sampler3D	vct_texNY;
uniform sampler3D	vct_texPY;
uniform sampler3D	vct_texNZ;
uniform sampler3D	vct_texPZ;

#define VCT_COL_MAX 5.0

vec4 vct_pack (vec4 col) {
	col.rgb = col.rgb / VCT_COL_MAX;
	return col;
}
vec4 vct_unpack (vec4 col) {
	col.rgb = col.rgb * VCT_COL_MAX;
	return col;
}

uint read_bid (ivec3 coord) {
	if (!all(lessThan(uvec3(coord), uvec3(WORLD_SIZE))))
		return B_AIR;
	
	uvec3 texcoord = bitfieldExtract(uvec3(coord), SUBCHUNK_SHIFT, 32 - SUBCHUNK_SHIFT); // (coord & ~SUBCHUNK_MASK) >> SUBCHUNK_SHIFT;
	uint subchunk = texelFetch(subchunks_tex, ivec3(texcoord), 0).r;
	
	if ((subchunk & SUBC_SPARSE_BIT) != 0) {
		return subchunk & ~SUBC_SPARSE_BIT;
	} else {
		
		// subchunk id to 3d tex offset (including subchunk_size multiplication)
		// ie. split subchunk id into 3 sets of VOXTEX_TEX_SHIFT bits
		uvec3 subc_offs;
		subc_offs.x = bitfieldExtract(subchunk, VOXTEX_TEX_SHIFT*0, VOXTEX_TEX_SHIFT);
		subc_offs.y = bitfieldExtract(subchunk, VOXTEX_TEX_SHIFT*1, VOXTEX_TEX_SHIFT);
		subc_offs.z = bitfieldExtract(subchunk, VOXTEX_TEX_SHIFT*2, VOXTEX_TEX_SHIFT);
		
		texcoord = bitfieldInsert(uvec3(coord), subc_offs, SUBCHUNK_SHIFT, 32 - SUBCHUNK_SHIFT); // (coord & SUBCHUNK_MASK) | (subc_offs << SUBCHUNK_SHIFT)
		
		return texelFetch(voxels_tex, ivec3(texcoord), 0).r;
	}
}
