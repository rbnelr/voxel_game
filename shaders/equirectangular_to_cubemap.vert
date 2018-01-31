#version 150 core // version 3.2

out		vec3	vs_dir_cubemap;
out		vec2	vs_pos_clip;

#define LLL	vec3(-1,-1,-1)
#define HLL	vec3(+1,-1,-1)
#define LHL	vec3(-1,+1,-1)
#define HHL	vec3(+1,+1,-1)
#define LLH	vec3(-1,-1,+1)
#define HLH	vec3(+1,-1,+1)
#define LHH	vec3(-1,+1,+1)
#define HHH	vec3(+1,+1,+1)

#define QUAD(a,b,c,d) b,c,a, a,c,d // facing outward
#define QUADI(a,b,c,d) a,d,b, b,d,c // facing inward

const vec3 dir_cubemap[6*6] = vec3[] (
	QUADI(	HLL,
			HHL,
			HHH,
			HLH ),
			
	QUADI(	LHL,
			LLL,
			LLH,
			LHH ),
			
	QUADI(	HLL,
			LLL,
			LHL,
			HHL ),
			
	QUADI(	HHH,
			LHH,
			LLH,
			HLH ),

	QUADI(	HHL,
			LHL,
			LHH,
			HHH ),
			
	QUADI(	LLL,
			HLL,
			HLH,
			LLH )
);
const vec2 pos_clip[6] = vec2[] (
	QUAD(	vec2(-1,-1),
			vec2(+1,-1),
			vec2(+1,+1),
			vec2(-1,+1) )
);

void main () {
	gl_Position =		vec4(pos_clip[gl_VertexID % 6], 0, 1);
	vs_dir_cubemap =	dir_cubemap[gl_VertexID];
	vs_pos_clip =		pos_clip[gl_VertexID % 6];
}
