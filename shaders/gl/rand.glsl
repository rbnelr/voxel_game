
// https://riptutorial.com/opencl/example/20715/using-thomas-wang-s-integer-hash-function
uint wang_hash (uint seed) {
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);
	return seed;
}

#if 0

// Compound versions of the hashing algorithm I whipped together.
uint hash( uvec2 v ) { return hash( v.x ^ hash(v.y)                         ); }
uint hash( uvec3 v ) { return hash( v.x ^ hash(v.y) ^ hash(v.z)             ); }
uint hash( uvec4 v ) { return hash( v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w) ); }

// Construct a float with half-open range [0:1] using low 23 bits.
// All zeroes yields 0.0, all ones yields the next smallest representable value below 1.0.
float floatConstruct( uint m ) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = uintBitsToFloat( m );       // Range [1:2]
    return f - 1.0;                        // Range [0:1]
}



// Pseudo-random value in half-open range [0:1].
float random( float x ) { return floatConstruct(hash(floatBitsToUint(x))); }
float random( vec2  v ) { return floatConstruct(hash(floatBitsToUint(v))); }
float random( vec3  v ) { return floatConstruct(hash(floatBitsToUint(v))); }
float random( vec4  v ) { return floatConstruct(hash(floatBitsToUint(v))); }
#endif

uint rand_state = 0;

// seed using 3 indices (eg. pixel pos + time, world voxel position)
void srand (uint x, uint y, uint z) {
	rand_state = wang_hash(wang_hash(wang_hash(x) ^ y) ^ z);
}

uint irand () {
	rand_state = wang_hash(rand_state);
	return rand_state;
}

const float _rand_scale = 1.0 / 4294967296.0; // 2^32 instead of 2^32-1 to generate [0,1) range instead of [0,1]

float rand () {
	return float(irand()) * _rand_scale;
}
vec2 rand2 () {
	return vec2(rand(), rand());
}
vec3 rand3 () {
	return vec3(rand(), rand(), rand());
}
