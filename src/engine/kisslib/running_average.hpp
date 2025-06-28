#pragma once
#include "circular_buffer.hpp"

// Running average with circular buffer
//  resize allowed (array loses its values)
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

	T calc_avg (T* out_min=nullptr, T* out_max=nullptr, T* out_std_dev=nullptr) {
		T total = 0;

		for (size_t i=0; i<buf.count(); ++i) {
			auto val = buf.get_oldest(i);
			total += val;
		}

		T mean = total / (T)buf.count();

		if (out_min || out_max || out_std_dev) {
			T min = +INF;
			T max = -INF;
			T variance = 0;

			for (size_t i=0; i<buf.count(); ++i) {
				auto val = buf.get_oldest(i);

				min = std::min(min, val);
				max = std::max(max, val);

				T tmp = val - mean;
				variance += tmp*tmp;
			}

			if (out_min) *out_min = min;
			if (out_max) *out_max = max;
			if (out_std_dev) *out_std_dev = std::sqrt(variance / ((T)buf.count() - 1));
		}
		return mean;
	}
};
