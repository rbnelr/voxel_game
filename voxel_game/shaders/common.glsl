
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

float map (float x, float a, float b) {
	return (x - a) / (b - a);
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
$endif
