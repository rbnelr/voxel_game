#include "time_of_day.hpp"
#include "voxel_light.hpp"

SkyColors TimeOfDay::calc_sky_colors (uint8* sky_light_reduce) {
	float t = time <= 12 ? time / 12 : (24-time) / 12; // 0: night, 1:day

	*sky_light_reduce = (uint8)roundi((1 - t) * (MAX_LIGHT_LEVEL-1));
	return {
		lerp(night_colors.sky_col    , day_colors.sky_col    , t),
		lerp(night_colors.horiz_col  , day_colors.horiz_col  , t),
		lerp(night_colors.ambient_col, day_colors.ambient_col, t)
	};
}
