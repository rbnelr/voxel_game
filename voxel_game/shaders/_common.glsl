
uniform	vec2	mcursor_pos;
uniform	vec2	screen_dim;

vec2 mouse () {		return mcursor_pos / screen_dim; }

float map (float x, float a, float b) { return (x -a) / (b -a); }

#define PI 3.1415926535897932384626433832795

const mat3 Z_UP_CONVENTION_TO_OPENGL_CUBEMAP_CONVENTION = mat3(
	vec3(+1, 0, 0),
	vec3( 0, 0,+1),
	vec3( 0,-1, 0) );	// my z-up convention:			+x = right,		+y = forward,	+z = up
						// opengl cubemap convention:	+x = left,		+y = down,		+z = forward
