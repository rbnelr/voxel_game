#version 150 core // version 3.2

$include "common.vert"

out		vec3	vs_world_dir;

uniform	mat4	world_to_clip;

uniform	mat4	world_to_cam;
uniform	mat4	cam_to_clip;

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
	
	vec3 world_dir = arr[gl_VertexID];

	gl_Position =	cam_to_clip * vec4(mat3(world_to_cam) * world_dir, 1);
	vs_world_dir =	world_dir;
	
	WIREFRAME_MACRO;
}
