// modified from here https://stackoverflow.com/questions/16569660/2d-perlin-noise-in-c

#include	<stdlib.h>
#include	<math.h>

namespace perlin_noise_n {
	/*
	f32 noise (s32 x, s32 y) {
		s32		n;
		
		n = x + y * 57;
		n = pow((n << 13), n);
		return (1.0 - ( (n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0);
	}*/
	f32 noise (s32 x, s32 y) {
		s32 n;
		
		n = x + y * 57;
		n = (n << 13) ^ n;
		return (1.0 - ( (n * ((n * n * 15731) + 789221) +  1376312589) & 0x7fffffff) / 1073741824.0);
	}

	f32 interpolate (f32 a, f32 b, f32 x) {
		f32		pi_mod;
		f32		f_unk;
		
		pi_mod = x * 3.1415927;
		f_unk = (1 - cos(pi_mod)) * 0.5;
		return (a * (1 - f_unk) + b * x);
	}

	f32 smooth_noise (s32 x, s32 y) {
		f32		corners;
		f32		center;
		f32		sides;
		
		corners = (noise(x - 1, y - 1) + noise(x + 1, y - 1) + noise(x - 1, x + 1) + noise(x + 1, y + 1)) / 16;
		sides = (noise(x - 1, y) + noise(x + 1, y) + noise(x, y - 1) + noise(x, y + 1)) / 8;
		center = noise(x, y) / 4;
		return (corners + sides + center);
	}

	f32 perlin_base (fv2 v) {
		s32		int_val[2];
		f32		frac_val[2];
		f32		value[4];
		f32		res[2];

		int_val[0] = (s32)v.x;
		int_val[1] = (s32)v.y;
		
		frac_val[0] = v.x - int_val[0];
		frac_val[1] = v.y - int_val[1];
		
		value[0] = smooth_noise(int_val[0], int_val[1]);
		value[1] = smooth_noise(int_val[0] + 1, int_val[1]);
		value[2] = smooth_noise(int_val[0], int_val[1] + 1);
		value[3] = smooth_noise(int_val[0] + 1, int_val[1] + 1);
		
		res[0] = interpolate(value[0], value[1], frac_val[0]);
		res[1] = interpolate(value[2], value[3], frac_val[0]);
		
		return (interpolate(res[0], res[1], frac_val[1]));
	}
	
	f32 perlin_octave (fv2 v, f32 freq) {
		return perlin_base(v * freq);
	}
	
	#if 1
	f32 perlin_two (fv2 val, s32 octave=1) {
		f32		total;
		f32		per;
		f32		amp;
		s32		hz;
		s32		i;

		total = 0.0;
		per = 0.5;
		i = 0;
		
		while (i < octave) {
			hz = pow(2, i);
			amp = pow(per, (f32)i);
			total += perlin_base(val * (fv2)hz) * amp;
			i += 1;
		}
		
		return (total);
	}
	#else
	f32 perlin_two(f32 x, f32 y, f32 gain, s32 octaves, s32 hgrid) {
		s32 i;
		f32 total = 0.0f;
		f32 frequency = 1.0f/(f32)hgrid;
		f32 amplitude = gain;
		f32 lacunarity = 2.0;

		for (i = 0; i < octaves; ++i)
		{
			total += noise_hander((f32)x * frequency, (f32)y * frequency) * amplitude;         
			frequency *= lacunarity;
			amplitude *= gain;
		} 

		return (total);
	}
	#endif
}
