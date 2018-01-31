
out		vec4	frag_col;

uniform	vec2	mcursor_pos;
uniform	vec2	screen_dim;

bool dbg_out_written = false;
vec4 dbg_out = vec4(1,0,1,1); // complains about maybe used uninitialized

vec2 mouse () {		return mcursor_pos / screen_dim; }
vec2 screen () {	return gl_FragCoord.xy / screen_dim; }

bool split_horizon = false;
bool split_vertial = false;

bool _split_left () {
	split_vertial = true;
	return gl_FragCoord.x < mcursor_pos.x;
}
bool _split_top () {
	split_horizon = true;
	return gl_FragCoord.y > mcursor_pos.y;
}

#define SPLIT_LEFT		_split_left()
#define SPLIT_TOP		_split_top()
#define SPLIT_RIGHT		!_split_left()
#define SPLIT_BOTTOM	!_split_top()

void DBG_COL (vec4 col) {
	dbg_out_written = true;
	dbg_out = col;
}
void DBG_COL (vec3 col) {
	DBG_COL(vec4(col, 1));
}
void DBG_COL (vec2 col) {
	DBG_COL(vec4(col, 0, 1));
}
void DBG_COL (float col) {
	DBG_COL(vec4(col,col,col, 1));
}
void FRAG_COL (vec4 col) {
	if (dbg_out_written) col = dbg_out;
	
	if (split_vertial && distance(gl_FragCoord.x, mcursor_pos.x) < 1) {
		col.rgb = mix(col.rgb, vec3(0.9,0.9,0.1), 0.7);
	}
	if (split_horizon && distance(gl_FragCoord.y, mcursor_pos.y) < 1) {
		col.rgb = mix(col.rgb, vec3(0.9,0.9,0.1), 0.7);
	}
	
	frag_col = col;
}
void FRAG_COL (vec3 col) {
	FRAG_COL(vec4(col, 1));
}

float map (float x, float a, float b) { return (x -a) / (b -a); }

#define PI 3.1415926535897932384626433832795

const mat3 Z_UP_CONVENTION_TO_OPENGL_CUBEMAP_CONVENTION = mat3(
	vec3(+1, 0, 0),
	vec3( 0, 0,+1),
	vec3( 0,-1, 0) );	// my z-up convention:			+x = right,		+y = forward,	+z = up
						// opengl cubemap convention:	+x = left,		+y = down,		+z = forward
