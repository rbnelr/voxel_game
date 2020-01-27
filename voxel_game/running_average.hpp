#pragma once
#include <memory>

// Running average with circular buffer
//  resize allowed (although the array loses it's values then)
//  avg is simply sum(values) / count
//  T should probably be a float type
template <typename T>
class RunningAverage {

public:
	std::unique_ptr<T[]> values;
	int count;
	int cur = 0;

	RunningAverage (int initial_count, T initial_values = 0) {
		resize(initial_count, initial_values);
	}

	void resize (int new_count, T initial_values = 0) {
		values = std::make_unique<T[]>(new_count);
		count = new_count;

		for (int i=0; i<count; ++i)
			values[i] = initial_values;
	}

	void push (T val) {
		if (cur >= count)
			cur = 0; // safeguard against size changes

		values[cur++] = val;
		cur %= count;
	}

	T calc_avg () {
		T total = 0;
		for (int i=0; i<count; ++i)
			total += values[i];

		return total / count;
	}
};
