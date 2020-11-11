
#ifdef _VERTEX
	#define vs2fs out
#endif
#ifdef _FRAGMENT
	#define vs2fs in

	layout(location = 0) out vec4 frag_col;
#endif


float map (float x, float a, float b) {
	return (x - a) / (b - a);
}

#ifndef NO_VIEW
	layout(std140, set = 0, binding = 0)
	uniform View {
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

		/*
		//	// mouse cursor pos in pixels
		//	vec2 cursor_pos;
		//
		//	int wireframe;
		//	// 0 -> off   !=0 -> on
		//	// bit 1 = shaded
		//	// bit 2 = colored
		*/
	};
#endif

#ifdef _FRAGMENT

	//bool RIGHT () {
	//	if (gl_FragCoord.x > cursor_pos.x)
	//		return true;
	//	if (gl_FragCoord.x == cursor_pos.x)
	//		DEBUG(0);
	//	return false;
	//}

	#ifdef DEBUG_COLOR
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
			if (_dicarded)
				discard;
			else if (!_debug)
				frag_col = col;
		}
	#else
		#define DISCARD() discard;

		#define DEBUG(col)

		#define FRAG_COL(col) frag_col = col
	#endif
#endif
