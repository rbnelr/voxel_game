#pragma once
#include "kissmath/output/int3.hpp"
#include "macros.hpp"
#include "assert.h"

template <typename T>
struct array3D {
	MOVE_ONLY_CLASS(array3D)
public:

	T*		data = nullptr;
	int3	size = 0;

	friend void swap (array3D& l, array3D& r) {
		std::swap(l.data, r.data);
		std::swap(l.size, r.size);
	}

	array3D () {}

	array3D (int3 const& size) {
		resize(size);
	}

	~array3D () {
		if (data) ::free(data);
	}

	void resize (int3 new_size) {
		size = new_size;
		if (data) ::free(data);
		data = (T*)malloc(sizeof(T) * size.x * size.y * size.z);
	}

	size_t count () {
		return size.z * size.y * size.x;
	}

	void clear (T const& val) {
		size_t n = count();

		if (sizeof(T) == 1) {
			memset(data, val, n);
		} else {
			for (size_t i=0; i<n; ++i) {
				data[i] = val;
			}
		}
	}

	size_t index (int x, int y, int z) {
		assert( (unsigned)x < (unsigned)size.x &&
			(unsigned)y < (unsigned)size.y &&
			(unsigned)z < (unsigned)size.z );
		return (z * size.y + y) * size.x + x;
	}

	T const& get (int x, int y, int z) const {
		return data[index(x,y,z)];
	}
	T& get (int x, int y, int z) {
		return data[index(x,y,z)];
	}

	T const& get (int3 const& pos) const {
		return get(pos.x, pos.y, pos.z);
	}
	T& get (int3 const& pos) {
		return get(pos.x, pos.y, pos.z);
	}
	T const& operator[] (int3 const& pos) const {
		return get(pos.x, pos.y, pos.z);
	}
	T& operator[] (int3 const& pos) {
		return get(pos.x, pos.y, pos.z);
	}
};
