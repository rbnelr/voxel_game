
$include "common.glsl"

in		vec3	vs_barycentric;

float wireframe_edge_factor () {
	vec3 d = fwidth(vs_barycentric);
	vec3 a3 = smoothstep(vec3(0.0), d*1.5, vs_barycentric);
	return min(min(a3.x, a3.y), a3.z);
}

out		vec4	frag_col;

bool dbg_out_written = false;
vec4 dbg_out = vec4(1,0,1,1); // complains about maybe used uninitialized

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
	
	#if WIREFRAME
		if (wireframe_edge_factor() >= 0.3) discard;
		
		//col = mix(vec4(1,1,0,1), vec4(0,0,0,1), wireframe_edge_factor());
	#endif
	
	frag_col = col;
}
void FRAG_COL (vec3 col) {
	FRAG_COL(vec4(col, 1));
}
