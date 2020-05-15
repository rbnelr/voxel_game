
struct Test {
	float3 a;
	float3 b;
	//bool3 c;
};

void error (struct Test t) {
	// error
}

#define RECURSE(name, name2) \
void name (struct Test t) { \
	name2(t); \
}

RECURSE( a1, error)
RECURSE( a2,  a1)
RECURSE( a4,  a2)
RECURSE( a8,  a4)
RECURSE(a16,  a8)
RECURSE(a32, a16)
RECURSE(a64, a32)

void kernel raycast (global const uint* SVO, global const float* image, const int width) {
	struct Test t = { 1, 2 };
	a64(t);
}
