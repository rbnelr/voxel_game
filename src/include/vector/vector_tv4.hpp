
union V4 {
	struct {
		T	x, y, z, w;
	};
	T		arr[4];
	
	T& operator[] (u32 i) {					return arr[i]; }
	constexpr T operator[] (u32 i) const {	return arr[i]; }
	
	INL V4 () {}
	INL constexpr V4 (T val):				x{val},	y{val},	z{val},	w{val} {}
	INL constexpr V4 (T x, T y, T z, T w):	x{x},	y{y},	z{z},	w{w} {}
	INL constexpr V4 (V2 v, T z, T w):		x{v.x},	y{v.y},	z{z},	w{w} {}
	INL constexpr V4 (V3 v, T w):			x{v.x},	y{v.y},	z{v.z},	w{w} {}
	
	INL constexpr V2 xy () const {			return V2(x,y) ;};
	INL constexpr V3 xyz () const {			return V3(x,y,z) ;};
	
#if !BOOLVEC
	V4& operator+= (V4 r) {					return *this = V4(x +r.x, y +r.y, z +r.z, w +r.w); }
	V4& operator-= (V4 r) {					return *this = V4(x -r.x, y -r.y, z -r.z, w -r.w); }
	V4& operator*= (V4 r) {					return *this = V4(x * r.x, y * r.y, z * r.z, w * r.w); }
	V4& operator/= (V4 r) {					return *this = V4(x / r.x, y / r.y, z / r.z, w / r.w); }
	
	#if I_TO_F_CONV
	operator fv4() {						return fv4((f32)x, (f32)y, (f32)z, (f32)w); }
	#endif
#endif
};

#if BOOLVEC
	static constexpr bool all (V4 b) {				return b.x && b.y && b.z && b.w; }
	static constexpr bool any (V4 b) {				return b.x || b.y || b.z || b.w; }
	
	static constexpr V4 operator! (V4 b) {			return V4(!b.x,			!b.y,		!b.z,		!b.w); }
	static constexpr V4 operator&& (V4 l, V4 r) {	return V4(l.x && r.x,	l.y && r.y,	l.z && r.z,	l.w && r.w); }
	static constexpr V4 operator|| (V4 l, V4 r) {	return V4(l.x || r.x,	l.y || r.y,	l.z || r.z,	l.w || r.w); }
	static constexpr V4 XOR (V4 l, V4 r) {			return V4(BOOL_XOR(l.x, r.x),	BOOL_XOR(l.y, r.y),	BOOL_XOR(l.z, r.z),	BOOL_XOR(l.w, r.w)); }
	
#else
	static constexpr BV4 operator < (V4 l, V4 r) {	return BV4(l.x  < r.x,	l.y  < r.y,	l.z  < r.z,	l.w  < r.w); }
	static constexpr BV4 operator<= (V4 l, V4 r) {	return BV4(l.x <= r.x,	l.y <= r.y,	l.z <= r.z,	l.w <= r.w); }
	static constexpr BV4 operator > (V4 l, V4 r) {	return BV4(l.x  > r.x,	l.y  > r.y,	l.z  > r.z,	l.w  > r.w); }
	static constexpr BV4 operator>= (V4 l, V4 r) {	return BV4(l.x >= r.x,	l.y >= r.y,	l.z >= r.z,	l.w >= r.w); }
	static constexpr BV4 operator== (V4 l, V4 r) {	return BV4(l.x == r.x,	l.y == r.y,	l.z == r.z,	l.w == r.w); }
	static constexpr BV4 operator!= (V4 l, V4 r) {	return BV4(l.x != r.x,	l.y != r.y,	l.z != r.z,	l.w != r.w); }
	static constexpr V4 select (V4 l, V4 r, BV4 c) {
		return V4(	c.x ? l.x : r.x,	c.y ? l.y : r.y,	c.z ? l.z : r.z,	c.w ? l.w : r.w );
	}
	
	static constexpr V4 operator+ (V4 v) {			return v; }
	static constexpr V4 operator- (V4 v) {			return V4(-v.x, -v.y, -v.z, -v.w); }

	static constexpr V4 operator+ (V4 l, V4 r) {	return V4(l.x +r.x, l.y +r.y, l.z +r.z, l.w +r.w); }
	static constexpr V4 operator- (V4 l, V4 r) {	return V4(l.x -r.x, l.y -r.y, l.z -r.z, l.w -r.w); }
	static constexpr V4 operator* (V4 l, V4 r) {	return V4(l.x * r.x, l.y * r.y, l.z * r.z, l.w	* r.w); }
	static constexpr V4 operator/ (V4 l, V4 r) {	return V4(l.x / r.x, l.y / r.y, l.z / r.z, l.w / r.w); }

	static constexpr V4 lerp (V4 a, V4 b, T t) {	return (a * V4(T(1) -t)) +(b * V4(t)); }
	static constexpr V4 lerp (V4 a, V4 b, V4 t) {	return (a * (V4(1) -t)) +(b * t); }
	static constexpr V4 map (V4 x, V4 a, V4 b) {	return (x -a)/(b -a); }

	static constexpr T dot (V4 l, V4 r) {			return l.x*r.x +l.y*r.y +l.z*r.z +l.w*r.w; }

	T length (V4 v) {								return sqrt(v.x*v.x +v.y*v.y +v.z*v.z +v.w*v.w); }
	V4 normalize (V4 v) {							return v / V4(length(v)); }
	V4 normalize_or_zero (V4 v) { // TODO: epsilon?
		T len = length(v);
		if (len != 0) {
			 v /= len;
		}
		return v;
	}

	#if 1
	static constexpr V4 min (V4 l, V4 r) {			return V4( min(l.x, r.x), min(l.y, r.y), min(l.z, r.z), min(l.w, r.w) ); }
	static constexpr V4 max (V4 l, V4 r) {			return V4( max(l.x, r.x), max(l.y, r.y), max(l.z, r.z), max(l.w, r.w) ); }
	#endif

	static constexpr V4 clamp (V4 val, V4 l, V4 h) {return min( max(val,l), h ); }
	static V4 mymod (V4 val, V4 range) {
		return V4(	mymod(val.x, range.x),
					mymod(val.y, range.y),
					mymod(val.z, range.z),
					mymod(val.w, range.w) );
	}
#endif
