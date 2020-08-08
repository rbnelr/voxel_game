#include "stdafx.hpp"
#include "time_of_day.hpp"
#include "voxel_light.hpp"

void TimeOfDay::calc_sky_colors (uint8* sky_light_reduce) {
	float t = time <= 12 ? time / 12 : (24-time) / 12; // 0: night, 1:day

	sun_dir = rotate3_Z(sun_azim) * rotate3_X((time / 24) * deg(360)) * float3(0,0,-1);

	*sky_light_reduce = (uint8)roundi((1 - t) * (MAX_LIGHT_LEVEL-1));
	cols = {
		lerp(night_colors.sky_col    , day_colors.sky_col    , t),
		lerp(night_colors.horiz_col  , day_colors.horiz_col  , t),
		lerp(night_colors.ambient_col, day_colors.ambient_col, t),
		lerp(night_colors.sun_col	 , day_colors.sun_col,     t),
	};
}
