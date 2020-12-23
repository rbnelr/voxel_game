
#ifdef _FRAGMENT
	layout(location = 0) out vec4 frag_col;

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