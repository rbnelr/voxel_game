
////// Global UBOS
layout(std140) uniform View {
	// world space to cam space, ie. view transform
	mat4 world_to_cam;
	// cam space to world space, ie. inverse view transform
	mat4 cam_to_world;
	// cam space to clip space, ie. projection transform
	mat4 cam_to_clip;
	// clip space to cam space, ie. inverse projection transform
	mat4 clip_to_cam;
	// world space to clip space, ie. view projection transform, to avoid two matrix multiplies when they are not needed
	mat4 world_to_clip;
	// near clip plane distance (positive)
	float clip_near;
	// far clip plane distance (positive)
	float clip_far;
	// viewport size in pixels
	vec2 viewport_size;
};

layout(std140) uniform Debug {
	// mouse cursor pos in pixels
	vec2 cursor_pos;

	int wireframe;
	// 0 -> off   !=0 -> on
	// bit 1 = shaded
	// bit 2 = colored
};

///// Utility
const float PI = 3.1415926535897932384626433832795;

// inverse lerp
float map (float x, float a, float b) {
	return (x - a) / (b - a);
}

// color
vec3 srgb (vec3 c) {
	return pow(c / 255.0, vec3(2.2));
}

vec3 hsl_to_rgb (vec3 hsl) { // hue is periodic since it represents the angle on the color wheel, so it can be out of the range [0,1]
	float hue = hsl.x;
	float sat = hsl.y;
	float lht = hsl.z;

	float hue6 = mod(hue, 1.0) * 6.0;

	float c = sat*(1.0 -abs(2.0*lht -1.0));
	float x = c * (1.0 -abs(mod(hue6, 2.0) -1.0));
	float m = lht -(c/2.0);

	vec3 rgb;
	if (		hue6 < 1.0 )	rgb = vec3(c,x,0);
	else if (	hue6 < 2.0 )	rgb = vec3(x,c,0);
	else if (	hue6 < 3.0 )	rgb = vec3(0,c,x);
	else if (	hue6 < 4.0 )	rgb = vec3(0,x,c);
	else if (	hue6 < 5.0 )	rgb = vec3(x,0,c);
	else						rgb = vec3(c,0,x);
	rgb += vec3(m);

	return rgb;
}
vec3 hsl_to_rgb (float hue, float saturation, float lightness) {
	return hsl_to_rgb(vec3(hue, saturation, lightness));
}

$if fragment
	//// https://stackoverflow.com/questions/4200224/random-noise-functions-for-glsl
	// Gold Noise ©2015 dcerisano@standard3d.com
	// - based on the Golden Ratio
	// - uniform normalized distribution
	// - fastest static noise generator function (also runs at low precision)

	const float PHI = 1.61803398874989484820459;  // Φ = Golden Ratio   

	float gold_noise(in vec2 xy, in float seed){
		return fract(tan(distance(xy*PHI, xy)*seed)*xy.x);
	}
	////
	
	float seed = 0.0;//time + 1.0;

	float rand () {
		return gold_noise(gl_FragCoord.xy, seed++);
	}
	vec2 rand2 () {
		return vec2(rand(), rand());
	}
	vec3 rand3 () {
		return vec3(rand(), rand(), rand());
	}
$endif

vec2 equirectangular_from_dir (vec3 v) {
	float phi = atan(v.x, v.y);
	float theta = acos(-v.z);
	return vec2(phi * (0.5 / PI) + 0.5, theta * (1.0 / PI));
}

vec3 fresnel (vec3 F0, float cos) {
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cos, 0.0, 1.0), 5.0);
}

//// Wireframe
$if vertex
	#if ALLOW_WIREFRAME
		out		vec3	vs_barycentric;

		const vec3[] BARYCENTRIC = vec3[] ( vec3(1,0,0), vec3(0,1,0), vec3(0,0,1) );

		#define WIREFRAME_MACRO		vs_barycentric = BARYCENTRIC[gl_VertexID % 3]
	#endif
$endif

$if fragment
	#if ALLOW_WIREFRAME
		in		vec3	vs_barycentric;

		float wireframe_edge_factor () {
			vec3 d = fwidth(vs_barycentric);
			vec3 a3 = smoothstep(vec3(0.0), d*1.5, vs_barycentric);
			return min(min(a3.x, a3.y), a3.z);
		}
	#endif

	out		vec4	frag_col;

	bool _dicarded = false;
	void DISCARD () {
		_dicarded = true;
	}

	bool _debug = false;
	void DEBUG (float f) {
		if (!_debug) {
			frag_col = vec4(f,f,f, 1.0);
			_debug = true;
		}
	}
	void DEBUG (vec2 col) {
		if (!_debug) {
			frag_col = vec4(col, 0.0, 1.0);
			_debug = true;
		}
	}
	void DEBUG (vec3 col) {
		if (!_debug) {
			frag_col = vec4(col, 1.0);
			_debug = true;
		}
	}
	void DEBUG (vec4 col) {
		if (!_debug) {
			frag_col = col;
			_debug = true;
		}
	}

	void FRAG_COL (vec4 col) {
		//if (length(gl_FragCoord.xy - cursor_pos) < 30)
		//	col = vec4(1,0,0,1);

		#if ALLOW_WIREFRAME
			if (wireframe != 0) {
				bool shaded = (wireframe & 2) != 0;
				bool colored = (wireframe & 4) != 0;

				float factor = wireframe_edge_factor();
				float thres = 0.25;
				
				//vec4 color = mix(vec4(1,1,0,1), vec4(0,0,0,1), factor);
				vec4 color = vec4(0,0,0,1);

				if (shaded) {

					if (factor < thres) {
						col = color;
						_dicarded = false;
					}

				} else {
					_dicarded = false;

					if (!colored) {
						col = color;
					}

					if (factor >= thres)
						discard;
				}
			}
		#endif

		if (_dicarded)
			discard;
		else if (!_debug)
			frag_col = col;
	}

	#if CURSOR_COMPARE
		bool RIGHT () {
			if (gl_FragCoord.x > cursor_pos.x)
				return true;
			if (gl_FragCoord.x == cursor_pos.x)
				DEBUG(0);
			return false;
		}
	#endif

	#if BIT_DEBUGGER
		uniform sampler2D dbg_font;
		uniform vec2 dbg_font_size = vec2(9, 17);
		uniform vec2 debug_view_pos = vec2(.3, .2);

		int counter = 0;

		void debug_print (float val) {
			vec2 pos = floor(viewport_size * debug_view_pos);
			vec2 char_pos = (gl_FragCoord.xy - pos) / (dbg_font_size + 1); // why does +1 here fix the ugly artefact next to the glyphs?
			ivec2 char_index = ivec2(floor(char_pos));

			if (char_index.x >= 0 && char_index.x < 32 && char_index.y == counter++) {
				int ascii = 0;
				if (char_index.x == 31) { // sign
					ascii = val < 0.0 ? 45 : 43; // 45='-' 43='+'
				} else if (char_index.x < 15) {

				} else { // decimal digits
					int digit_index = 30 - char_index.x;
					float divided = abs(val) * pow(10.0, -float(digit_index));
					float digit = divided > 1.0 || digit_index == 0 ? (mod(divided, 10) + 48) : 32; // 48='0' 32=' '
					ascii = int(digit);
				}

				vec2 uv = (vec2(ascii % 16, 7 - ascii / 16) + fract(char_pos)) / vec2(16,8);
				DEBUG(texture(dbg_font, uv));
			}
		}
		void debug_print_binary (uint val) {
			vec2 pos = floor(viewport_size * debug_view_pos);
			vec2 char_pos = (gl_FragCoord.xy - pos) / (dbg_font_size + 1); // why does +1 here fix the ugly artefact next to the glyphs?
			ivec2 char_index = ivec2(floor(char_pos));

			if (char_index.x >= 0 && char_index.x < 32 && char_index.y == counter++) {
				int ascii = (val & (0x80000000u >> char_index.x)) != 0 ? 49 : 48; // '1' : '0'
				vec2 uv = (vec2(ascii % 16, 7 - ascii / 16) + fract(char_pos)) / vec2(16,8);
				DEBUG(texture(dbg_font, uv));
			}
		}
	#endif
$endif
