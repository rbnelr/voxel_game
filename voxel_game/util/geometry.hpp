#pragma once
#include "../kissmath.hpp"

// Push cube verticies
// edge length is one origin being the geometric center
// face order: -x, +x, -y, +y, -z, +z
template <typename T, typename FUNC>
void push_cube (FUNC vertex) {
	auto face = [&] (float3 a, float3 b, float3 c, float3 d, float3 normal, int face) {
		vertex(b, face, normal, float2(1, 0));
		vertex(c, face, normal, float2(1, 1));
		vertex(a, face, normal, float2(0, 0));
		vertex(a, face, normal, float2(0, 0));
		vertex(c, face, normal, float2(1, 1));
		vertex(d, face, normal, float2(0, 1));
	};

	float3 LLL = float3(-.5f,-.5f,-.5f);
	float3 HLL = float3(+.5f,-.5f,-.5f);
	float3 LHL = float3(-.5f,+.5f,-.5f);
	float3 HHL = float3(+.5f,+.5f,-.5f);
	float3 LLH = float3(-.5f,-.5f,+.5f);
	float3 HLH = float3(+.5f,-.5f,+.5f);
	float3 LHH = float3(-.5f,+.5f,+.5f);
	float3 HHH = float3(+.5f,+.5f,+.5f);

	face(LHL, LLL, LLH, LHH, float3(-1,0,0), 0);
	face(HLL, HHL, HHH, HLH, float3(+1,0,0), 1);

	face(LLL, HLL, HLH, LLH, float3(0,-1,0), 2);
	face(HHL, LHL, LHH, HHH, float3(0,+1,0), 3);

	face(LHL, HHL, HLL, LLL, float3(0,0,-1), 4);
	face(LLH, HLH, HHH, LHH, float3(0,0,+1), 5);
}

// Push cylinder verticies
// radius is one and height is one with origin being the geometric center
// cylinder axis is z
template <typename T, typename FUNC>
void push_cylinder (int sides, FUNC vertex) {
	float2 rv = float2(1, 0);

	float2x2 prev_rot = float2x2::identity();

	for (int i=0; i<sides; ++i) {
		float rot_b = (float)(i + 1) / (float)sides * deg(360);

		float2x2 ma = prev_rot;
		float2x2 mb = rotate2(rot_b);

		prev_rot = mb;

		vertex(float3(0,0,     +.5f));
		vertex(float3(ma * rv, +.5f));
		vertex(float3(mb * rv, +.5f));
		
		vertex(float3(mb * rv, -.5f));
		vertex(float3(mb * rv, +.5f));
		vertex(float3(ma * rv, -.5f));
		vertex(float3(ma * rv, -.5f));
		vertex(float3(mb * rv, +.5f));
		vertex(float3(ma * rv, +.5f));
		
		vertex(float3(0,0,     -.5f));
		vertex(float3(mb * rv, -.5f));
		vertex(float3(ma * rv, -.5f));
	}
}
