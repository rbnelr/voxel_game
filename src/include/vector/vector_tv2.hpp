
union V2 {
	struct {
		T	x, y;
	};
	T		arr[2];
	
	T& operator[] (u32 i) {					return arr[i]; }
	constexpr T operator[] (u32 i) const {	return arr[i]; }
	
	INL V2 () {}
	INL constexpr V2 (T val):				x{val},	y{val} {}
	INL constexpr V2 (T x, T y):			x{x},	y{y} {}
	
#if !BOOLVEC
	V2& operator+= (V2 r) {					return *this = V2(x +r.x, y +r.y); }
	V2& operator-= (V2 r) {					return *this = V2(x -r.x, y -r.y); }
	V2& operator*= (V2 r) {					return *this = V2(x * r.x, y * r.y); }
	V2& operator/= (V2 r) {					return *this = V2(x / r.x, y / r.y); }
	
	#if I_TO_F_CONV
	operator fv2() {						return fv2((f32)x, (f32)y); }
	#endif
#endif
};

#if BOOLVEC
	static constexpr bool all (V2 b) {				return b.x && b.y; }
	static constexpr bool any (V2 b) {				return b.x || b.y; }
	
	static constexpr V2 operator! (V2 b) {			return V2(!b.x,			!b.y); }
	static constexpr V2 operator&& (V2 l, V2 r) {	return V2(l.x && r.x,	l.y && r.y); }
	static constexpr V2 operator|| (V2 l, V2 r) {	return V2(l.x || r.x,	l.y || r.y); }
	static constexpr V2 XOR (V2 l, V2 r) {			return V2(BOOL_XOR(l.x, r.x),	BOOL_XOR(l.y, r.y)); }
	
#else
	static constexpr BV2 operator < (V2 l, V2 r) {	return BV2(l.x  < r.x,	l.y  < r.y); }
	static constexpr BV2 operator<= (V2 l, V2 r) {	return BV2(l.x <= r.x,	l.y <= r.y); }
	static constexpr BV2 operator > (V2 l, V2 r) {	return BV2(l.x  > r.x,	l.y  > r.y); }
	static constexpr BV2 operator>= (V2 l, V2 r) {	return BV2(l.x >= r.x,	l.y >= r.y); }
	static constexpr BV2 operator== (V2 l, V2 r) {	return BV2(l.x == r.x,	l.y == r.y); }
	static constexpr BV2 operator!= (V2 l, V2 r) {	return BV2(l.x != r.x,	l.y != r.y); }
	static constexpr V2 select (V2 l, V2 r, BV2 c) {
		return V2(	c.x ? l.x : r.x,	c.y ? l.y : r.y );
	}
	
	static constexpr V2 operator+ (V2 v) {			return v; }
	static constexpr V2 operator- (V2 v) {			return V2(-v.x, -v.y); }
	
	static constexpr V2 operator+ (V2 l, V2 r) {	return V2(l.x +r.x, l.y +r.y); }
	static constexpr V2 operator- (V2 l, V2 r) {	return V2(l.x -r.x, l.y -r.y); }
	static constexpr V2 operator* (V2 l, V2 r) {	return V2(l.x * r.x, l.y * r.y); }
	static constexpr V2 operator/ (V2 l, V2 r) {	return V2(l.x / r.x, l.y / r.y); }
	
	static constexpr V2 lerp (V2 a, V2 b, T t) {	return (a * V2(T(1) -t)) +(b * V2(t)); }
	static constexpr V2 lerp (V2 a, V2 b, V2 t) {	return (a * (V2(1) -t)) +(b * t); }
	static constexpr V2 map (V2 x, V2 a, V2 b) {	return (x -a)/(b -a); }
	
	static constexpr T dot (V2 l, V2 r) {			return l.x*r.x +l.y*r.y; }
	
	T length (V2 v) {								return sqrt(v.x*v.x +v.y*v.y); }
	V2 normalize (V2 v) {							return v / V2(length(v)); }
	V2 normalize_or_zero (V2 v) { // TODO: epsilon?
		T len = length(v);
		if (len != 0) {
			 v /= len;
		}
		return v;
	}
	
	#if 1
	static constexpr V2 min (V2 l, V2 r) {			return V2( min(l.x, r.x), min(l.y, r.y) ); }
	static constexpr V2 max (V2 l, V2 r) {			return V2( max(l.x, r.x), max(l.y, r.y) ); }
	#endif
	
	static constexpr V2 clamp (V2 val, V2 l, V2 h) {return min( max(val,l), h ); }
	static V2 mymod (V2 val, V2 range) {
		return V2(	mymod(val.x, range.x),	mymod(val.y, range.y) );
	}
#endif
