
layout(std140) uniform Fog {
	vec3 sky_col;
	vec3 horiz_col;
	vec3 down_col;

	float coeff;
} fog;

vec3 fog_color (vec3 dir_world) {
		
	vec3 col;
	if (dir_world.z > 0)
		col = mix(fog.horiz_col, fog.sky_col, dir_world.z);
	else
		col = mix(fog.horiz_col, fog.down_col, -dir_world.z);

	return col;
}

vec3 apply_fog (vec3 old_col, float dist_sqr, vec3 dir_world) {
	float blend = 1.0 - dist_sqr*dist_sqr * fog.coeff*fog.coeff*fog.coeff*fog.coeff;
	vec3 fog_col = fog_color(dir_world);
	
	return mix(fog_col, old_col, clamp(blend, 0.0,1.0));
}
