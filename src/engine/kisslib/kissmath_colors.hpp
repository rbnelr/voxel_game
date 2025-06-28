#pragma once
#include "stdint.h"
#include "kissmath/output/float3.hpp"
#include "kissmath/output/float4.hpp"
#include "kissmath/output/int3.hpp"
#include "kissmath/output/int4.hpp"
#include "kissmath/output/uint8v3.hpp"
#include "kissmath/output/uint8v4.hpp"

namespace kissmath {

	typedef float3 lrgb;
	typedef float4 lrgba;
	typedef uint8v3 srgb8;
	typedef uint8v4 srgba8;

	// color [0,255] to color [0,1]
	inline float u8_to_float (uint8_t srgb) {
		return (float)srgb / 255.0f;
	}
	// color [0,1] to color [0,255]
	// (clamped)
	inline uint8_t float_to_u8 (float linear) {
		return (uint8_t)roundi(clamp(linear * 255.0f, 0.0f, 255.0f));
	}

	// srgb [0,1] to linear [0,1]
	inline float to_linear (float srgb) {
		if (srgb <= 0.0404482362771082f) {
			return srgb * 1/12.92f;
		} else {
			return pow( (srgb +0.055f) * 1/1.055f, 2.4f );
		}
	}
	// linear [0,1] to srgb [0,1]
	inline float to_srgb (float linear) {
		if (linear <= 0.00313066844250063f) {
			return linear * 12.92f;
		} else {
			return ( 1.055f * pow(linear, 1/2.4f) ) -0.055f;
		}
	}

	inline lrgb to_linear (srgb8 srgb) {
		return lrgb(	to_linear(u8_to_float(srgb.x)),
						to_linear(u8_to_float(srgb.y)),
						to_linear(u8_to_float(srgb.z)) );
	}
	inline lrgba to_linear (srgba8 srgba) {
		return lrgba(	to_linear(u8_to_float(srgba.x)),
						to_linear(u8_to_float(srgba.y)),
						to_linear(u8_to_float(srgba.z)),
								  u8_to_float(srgba.w) );
	}

	inline srgb8 to_srgb (lrgb lrgb) {
		return srgb8(	float_to_u8(to_srgb(lrgb.x)),
						float_to_u8(to_srgb(lrgb.y)),
						float_to_u8(to_srgb(lrgb.z)) );
	}
	inline srgba8 to_srgb (lrgba lrgba) {
		return srgba8(	float_to_u8(to_srgb(lrgba.x)),
						float_to_u8(to_srgb(lrgba.y)),
						float_to_u8(to_srgb(lrgba.z)),
						float_to_u8(        lrgba.w ) );
	}

	inline lrgb srgb (uint8_t all) {
		return to_linear(srgb8(all));
	}
	inline lrgb srgb (uint8_t r, uint8_t g, uint8_t b) {
		return to_linear(srgb8(r,g,b));
	}

	inline lrgba srgba (uint8_t r, uint8_t g, uint8_t b) {
		return lrgba(to_linear(srgb8(r,g,b)), 1);
	}

	// 0xrrbbgg format like in web
	//inline lrgba srgba (int32_t rgb_hex) {
	//	return srgba((rgb_hex >> 16) & 0xff, (rgb_hex >> 8) & 0xff, rgb_hex & 0xff);
	//}

	// alpha is linear
	inline lrgba srgba (uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
		return to_linear(srgba8(r,g,b,a));
	}
	inline lrgba srgba (uint8_t col, uint8_t a=255) {
		return to_linear(srgba8(col,col,col,a));
	}

	inline lrgb hsl_to_rgb (float3 hsl) {
#if 0
		// modified from http://www.easyrgb.com/en/math.php
		f32 H = hsl.x;
		f32 S = hsl.y;
		f32 L = hsl.z;

		auto Hue_2_RGB = [] (f32 a, f32 b, f32 vH) {
			if (vH < 0) vH += 1;
			if (vH > 1) vH -= 1;
			if ((6 * vH) < 1) return a +(b -a) * 6 * vH;
			if ((2 * vH) < 1) return b;
			if ((3 * vH) < 2) return a +(b -a) * ((2.0f/3) -vH) * 6;
			return a;
		};

		fv3 rgb;
		if (S == 0) {
			rgb = fv3(L);
		} else {
			f32 a, b;

			if (L < 0.5f)	b = L * (1 +S);
			else			b = (L +S) -(S * L);

			a = 2 * L -b;

			rgb = fv3(	Hue_2_RGB(a, b, H +(1.0f / 3)),
				Hue_2_RGB(a, b, H),
				Hue_2_RGB(a, b, H -(1.0f / 3)) );
		}

		return to_linear(rgb);
#else
		float hue = hsl.x;
		float sat = hsl.y;
		float lht = hsl.z;

		float hue6 = hue*6.0f;

		float c = sat*(1.0f -abs(2.0f*lht -1.0f));
		float x = c * (1.0f -abs(wrap(hue6, 2.0f) -1.0f));
		float m = lht -(c/2.0f);

		float3 rgb;
		if (		hue6 < 1.0f )	rgb = float3(c,x,0);
		else if (	hue6 < 2.0f )	rgb = float3(x,c,0);
		else if (	hue6 < 3.0f )	rgb = float3(0,c,x);
		else if (	hue6 < 4.0f )	rgb = float3(0,x,c);
		else if (	hue6 < 5.0f )	rgb = float3(x,0,c);
		else						rgb = float3(c,0,x);
		rgb += m;

		return rgb; // TODO: colors already linear?
#endif
	}
}
