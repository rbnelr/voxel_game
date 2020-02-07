#pragma once
#include "circular_buffer.hpp"

// Running average with circular buffer
//  resize allowed (although the array loses it's values then)
//  avg is simply sum(values) / count
//  T should probably be a float type
template <typename T=float>
class RunningAverage {
	circular_buffer<T> buf;
public:

	T* data () {
		return buf.data();
	}
	size_t capacity () {
		return buf.capacity();
	}
	size_t count () {
		return buf.count();
	}

	RunningAverage (int initial_count) {
		buf.resize(initial_count);
	}

	void resize (int new_count) {
		buf.resize(new_count);
	}

	void push (T val) {
		buf.push(val);
	}

	T calc_avg () {
		T total = 0;
		for (size_t i=0; i<buf.count(); ++i)
			total += buf.get_oldest(i);

		return total / (T)buf.count();
	}
};
