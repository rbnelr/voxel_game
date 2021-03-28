#version 460 core
#include "fullscreen_triangle.glsl"

float luminance (vec3 col) {
	return dot(col, vec3(0.2126, 0.7152, 0.0722));
}


vec3 convertRGB2XYZ(vec3 _rgb)
{
	// Reference(s):
	// - RGB/XYZ Matrices
	//   https://web.archive.org/web/20191027010220/http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
	vec3 xyz;
	xyz.x = dot(vec3(0.4124564, 0.3575761, 0.1804375), _rgb);
	xyz.y = dot(vec3(0.2126729, 0.7151522, 0.0721750), _rgb);
	xyz.z = dot(vec3(0.0193339, 0.1191920, 0.9503041), _rgb);
	return xyz;
}

vec3 convertXYZ2RGB(vec3 _xyz)
{
	vec3 rgb;
	rgb.x = dot(vec3( 3.2404542, -1.5371385, -0.4985314), _xyz);
	rgb.y = dot(vec3(-0.9692660,  1.8760108,  0.0415560), _xyz);
	rgb.z = dot(vec3( 0.0556434, -0.2040259,  1.0572252), _xyz);
	return rgb;
}

vec3 convertXYZ2Yxy(vec3 _xyz)
{
	// Reference(s):
	// - XYZ to xyY
	//   https://web.archive.org/web/20191027010144/http://www.brucelindbloom.com/index.html?Eqn_XYZ_to_xyY.html
	float inv = 1.0/dot(_xyz, vec3(1.0, 1.0, 1.0) );
	return vec3(_xyz.y, _xyz.x*inv, _xyz.y*inv);
}

vec3 convertYxy2XYZ(vec3 _Yxy)
{
	// Reference(s):
	// - xyY to XYZ
	//   https://web.archive.org/web/20191027010036/http://www.brucelindbloom.com/index.html?Eqn_xyY_to_XYZ.html
	vec3 xyz;
	xyz.x = _Yxy.x*_Yxy.y/_Yxy.z;
	xyz.y = _Yxy.x;
	xyz.z = _Yxy.x*(1.0 - _Yxy.y - _Yxy.z)/_Yxy.z;
	return xyz;
}

vec3 convertRGB2Yxy(vec3 _rgb)
{
	return convertXYZ2Yxy(convertRGB2XYZ(_rgb) );
}

vec3 convertYxy2RGB(vec3 _Yxy)
{
	return convertXYZ2RGB(convertYxy2XYZ(_Yxy) );
}


// All components are in the range [0…1], including hue.
vec3 rgb2hsv (vec3 c) {
	vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
	vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

	float d = q.x - min(q.w, q.y);
	float e = 1.0e-10;
	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// All components are in the range [0…1], including hue.
vec3 hsv2rgb (vec3 c) {
	vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
	return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

#ifdef _FRAGMENT
	uniform sampler2D input_tex;
	uniform sampler1D gaussian_kernel;
	
	uniform float exposure = 1.0;
	
	uniform vec2 stepsz;
	uniform int radius = 16;
	uniform float cutoff = 1.0;
	uniform float strength = 0.2;
	
	#if PASS == 0
		#define DIR(offs) vec2((offs) * stepsz.x * 2.0, 0.0)
		
		vec3 fcutoff (vec3 col) {
			col *= exposure;
			
			//col = max(col - vec3(cutoff), 0.0) * strength;
			
			vec3 hsv = rgb2hsv(col);
			hsv.z = max(hsv.z - cutoff, 0.0) * strength;
			col = hsv2rgb(hsv);
			return col;
		}
	#else
		#define DIR(offs) vec2(0.0, (offs) * stepsz.y)
		
		vec3 fcutoff (vec3 col) {
			return col;
		}
	#endif
	
	out vec3 frag_col;
	void main () {
		vec3 col = fcutoff(textureLod(input_tex, vs_uv, 0.0).rgb);
		col *= texelFetch(gaussian_kernel, 0, 0).r;
		
		for (int x=1; x <= radius; ++x) {
			float weight = texelFetch(gaussian_kernel, x, 0).r;
			float offs = pow(float(x), 1.4);
			col += fcutoff(textureLod(input_tex, vs_uv + DIR(+offs), 0.0).rgb) * weight;
			col += fcutoff(textureLod(input_tex, vs_uv + DIR(-offs), 0.0).rgb) * weight;
		}
		
		frag_col = col;
	}
#endif
