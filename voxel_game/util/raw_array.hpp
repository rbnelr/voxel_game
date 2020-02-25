#pragma once
#include "stdlib.h"
#include "assert.h"

// Raw malloced array to avoid unique_ptr<T[]> clearing the data with really bad performance
template <typename T>
struct RawArray {
	T* ptr = nullptr;
	size_t size = 0;

	RawArray () {}

	RawArray (size_t size) {
		ptr = (T*)malloc(size * sizeof(T));
		this->size = size;
	}
	~RawArray () {
		if (ptr)
			free(ptr);
	}

	T const& operator[] (size_t index) const {
		assert(index < size);
		return ptr[index];
	}
	T& operator[] (size_t index) {
		assert(index < size);
		return ptr[index];
	}

	RawArray (RawArray const& r) {				std::swap(ptr, r.ptr); std::swap(size, r.size); }
	RawArray (RawArray&& r) {					std::swap(ptr, r.ptr); std::swap(size, r.size); }
	RawArray& operator= (RawArray const& r) {	std::swap(ptr, r.ptr); std::swap(size, r.size); return *this; }
	RawArray& operator= (RawArray&& r) {		std::swap(ptr, r.ptr); std::swap(size, r.size); return *this; }
};
