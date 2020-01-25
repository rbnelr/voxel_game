#version 150 core // version 3.2

$include "common.frag"

in		vec2	vs_uv;
in		vec4	vs_col;

uniform	bool	view_col;
uniform	bool	view_alpha;

uniform sampler2D	tex;

void main () {
	vec4 col = texture(tex, vs_uv) * vs_col;
	
	// checkerboard alpha pattern
	vec3 bg_col =	( ((int(gl_FragCoord.x) / 8) % 2) ^
					  ((int(gl_FragCoord.y) / 8) % 2) ) == 0 ?
			vec3(pow(153.0/255, 2.2)) :
			vec3(pow(102.0/255, 2.2));
	
	if (		 view_col &&  view_alpha )		col.rgb = mix(bg_col, col.rgb, col.a);
	else if (	 view_col && !view_alpha )		col.rgb = col.rgb;
	else if (	!view_col &&  view_alpha )		col.rgb = col.aaa;
	else /*(	!view_col && !view_alpha )*/	col.rgb = bg_col;
	
	col.a = 1;
	
	FRAG_COL( col );
}
