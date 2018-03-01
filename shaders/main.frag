#version 150 core // version 3.2

#define WIREFRAME 0

$include "common.frag"

in		vec3	vs_pos_cam;
in		vec4	vs_uvzw_atlas;
in		float	vs_hp_ratio;
in		vec4	vs_dbg_tint;

uniform	sampler2D	atlas;
uniform	sampler2D	breaking;

uniform int texture_res;
uniform int atlas_textures_count;
uniform int breaking_frames_count;

vec2 map (vec2 val, vec2 in_a, vec2 in_b) { // val[in_a,in_b] -> [0,1]
	return mix((val -in_a) / (in_b -in_a), vec2(0), equal(in_a, in_b));
}
vec2 map (vec2 val, vec2 in_a, vec2 in_b, vec2 out_a, vec2 out_b) { // val[in_a,in_b] -> [out_a,out_b]
	return mix(out_a, out_b, map(val, in_a, in_b));
}

vec2 map_clamp (vec2 val, vec2 in_a, vec2 in_b) { // val[in_a,in_b] -> [0,1]
	//return clamp( mix((val -in_a) / (in_b -in_a), vec2(0), equal(in_a, in_b)), 0,1); this returns 0 if in_a == in_b 
	return clamp((val -in_a) / (in_b -in_a), 0,1);
}
vec2 map_clamp (vec2 val, vec2 in_a, vec2 in_b, vec2 out_a, vec2 out_b) { // val[in_a,in_b] -> [out_a,out_b]
	return mix(out_a, out_b, map_clamp(val, in_a, in_b));
}

vec2 nearest_filtering_edge_smoothing (vec2 uv) {
	
	float smooth_distance = mix(0.0, 0.5, mouse().x);
	smooth_distance = 0;
	
	if (smooth_distance == 0) return vec2(0);
	
	vec2 l = smoothstep(vec2(0),vec2(1), map_clamp(fract(uv), vec2(0),vec2(smooth_distance), vec2(0.5),vec2(1))) -1;
	vec2 h = smoothstep(vec2(0),vec2(1), map_clamp(fract(uv), 1 -vec2(smooth_distance),vec2(1), vec2(0),vec2(0.5)));
	
	//DBG_COL(( l +h +0.5 ).x);
	
	return l +h;
}

vec2 manual_nearest_filtering (vec2 uv) {
	uv *= vec2(texture_res);
	uv = floor(uv) +0.5 +nearest_filtering_edge_smoothing(uv);
	uv /= vec2(texture_res);
	
	vec2 pixel_size = 1.0 / vec2(texture_res);
	
	// clamp uvs to be so that we never sample outside of our part of the texture atlas even with smoothing
	//  This could also be achieved with a one pixel border around the textures
	uv = clamp(uv, vec2(0) +pixel_size/2, vec2(1) -pixel_size/2);
	
	return uv;
}

void main () {
	//FRAG_COL( vec3(1,1,0.8) );
	
	vec2 uv = vs_uvzw_atlas.xy;
	
	uv = manual_nearest_filtering(uv);
	
	uv.x /= 3;
	uv.x += vs_uvzw_atlas.z / 3;
	
	uv.y /= atlas_textures_count;
	uv.y += vs_uvzw_atlas.w / atlas_textures_count;
	
	vec4 col = texture(atlas, uv).rgba;
	
	if (false && vs_hp_ratio > 0.0f) {
		vec2 breaking_uv = vs_uvzw_atlas.xy;
		
		breaking_uv = manual_nearest_filtering(breaking_uv);
		
		int breaking_frame = clamp(int(floor(vs_hp_ratio * float(breaking_frames_count))), 0, breaking_frames_count-1);
		
		breaking_uv.y /= float(breaking_frames_count);
		breaking_uv.y += float(breaking_frame -1 -breaking_frames_count) / float(breaking_frames_count);
		
		col *= texture(breaking, breaking_uv).rgba * vec4(2,2,2,1);
	}
	
	FRAG_COL( mix(col, vec4(1), 0.1) * mix(vec4(1), vs_dbg_tint, 1) );
}
