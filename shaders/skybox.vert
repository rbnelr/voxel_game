#version 150 core // version 3.2

out		vec3	vs_pos_world_dir;

uniform	mat4	world_to_clip;
uniform	vec2	mcursor_pos;
uniform	vec2	screen_dim;

uniform	mat4	skybox_to_clip;

#define LLL	vec3(-1,-1,-1)
#define HLL	vec3(+1,-1,-1)
#define LHL	vec3(-1,+1,-1)
#define HHL	vec3(+1,+1,-1)
#define LLH	vec3(-1,-1,+1)
#define HLH	vec3(+1,-1,+1)
#define LHH	vec3(-1,+1,+1)
#define HHH	vec3(+1,+1,+1)

//#define QUAD(a,b,c,d) b,c,a, a,c,d // facing outward
#define QUAD(a,b,c,d) a,d,b, b,d,c // facing inward

const vec3 arr[6*6] = vec3[] (
	QUAD(	LHL,
			LLL,
			LLH,
			LHH ),
	
	QUAD(	HLL,
			HHL,
			HHH,
			HLH ),
	
	QUAD(	LLL,
			HLL,
			HLH,
			LLH ),
	
	QUAD(	HHL,
			LHL,
			LHH,
			HHH ),
	
	QUAD(	HLL,
			LLL,
			LHL,
			HHL ),
	
	QUAD(	LLH,
			HLH,
			HHH,
			LHH )
);

void main () {
	float r = mix(1, 0.2, mcursor_pos.x / screen_dim.x);
	
	gl_Position =		skybox_to_clip * vec4(arr[gl_VertexID], 1);
	vs_pos_world_dir =	arr[gl_VertexID];
}
