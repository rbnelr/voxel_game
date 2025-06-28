#pragma once
#include "kissmath.hpp"
#include <vector>
#include "assert.h"

enum AnimInterpMode {
	AIM_NONE,
	AIM_LINEAR,
};

template <typename VAL_T, AnimInterpMode AIM, bool LOOP=true>
struct Animation {
	struct Keyframe {
		float t;
		VAL_T val;
	};

	std::vector<Keyframe> keyframes;
	float duration = 1;
	bool loop_interp = true;

	Animation (std::initializer_list<Keyframe> keyframes, float duration=1, bool loop_interp=LOOP): keyframes{keyframes}, duration{duration}, loop_interp{loop_interp} {
		assert(this->keyframes.size() >= 1);
	}

	VAL_T calc (float t) {
		int count = (int)keyframes.size();
		
		assert(count >= 1);
		if (count == 1) {
			return keyframes[0].val;
		}

		int left_i = count - 1; // if no keyframe at 0 then wrap around left to the rightmost keyframe

		for (int i=0; i<count; ++i) {
			if (keyframes[i].t <= t) {
				left_i = i;
			}
		}

		int right_i = left_i + 1;

		bool wrapped = false;
		if (right_i == count) {
			if (loop_interp) {
				right_i = 0;
				wrapped = true;
			} else {
				right_i = left_i;
			}
		}

		Keyframe& left  = keyframes[left_i];
		Keyframe& right = keyframes[right_i];
		float right_t = right.t;
		if (wrapped)
			right_t += duration; // right is wrapped

		switch (AIM) {
			case AIM_NONE:
				return left.val;

			case AIM_LINEAR:
				float len = right_t - left.t;
				if (len == 0.0f)
					return left.val;

				float inter_t = (t - left.t) / len;
				return lerp(left.val, right.val, inter_t);
		}
		return {};
	}
};

#define ROTATION_MODE 0

struct AnimRotation {
#if ROTATION_MODE==0
	float3 euler; // z * y * x angles (like blender default)
#elif ROTATION_MODE==1
	float3x3 matrix;
#else
	// implement quaternion
#endif

	// z * y * x angles (like blender default)
	static AnimRotation from_euler (float x, float y, float z) {
	#if ROTATION_MODE==0
		return { float3(x,y,z) };
	#elif ROTATION_MODE==1
		return { rotate3_Z(z) * rotate3_Y(y) * rotate3_X(x) };
	#endif
	}

	static AnimRotation lerp (AnimRotation const& l, AnimRotation const& r, float t) {
	#if ROTATION_MODE==0
		return { kissmath::lerp(l.euler, r.euler, t) };
	#elif ROTATION_MODE==1
		return { l.matrix * (1.0f - t) + r.matrix * t };
	#endif
	}

	operator float3x3 () {
	#if ROTATION_MODE==0
		return rotate3_Z(euler.z) * rotate3_Y(euler.y) * rotate3_X(euler.x);
	#elif ROTATION_MODE==1
		return matrix;
	#endif
	}
};

struct AnimPosRot {
	float3 pos;
	AnimRotation rot;
};
inline AnimPosRot lerp (AnimPosRot const& l, AnimPosRot const& r, float t) {
	AnimPosRot ret;
	ret.pos = lerp(l.pos, r.pos, t);
	ret.rot = AnimRotation::lerp(l.rot, r.rot, t);
	return ret;
}

typedef Animation<lrgba, AIM_LINEAR, false> Gradient;
