#pragma once
#include "stdlib.h"
#include "assert.h"

namespace {
	size_t _min (size_t a, size_t b) {
		return a < b ? a : b;
	}
	size_t _max (size_t a, size_t b) {
		return a > b ? a : b;
	}
}

// Array that is implemented with malloc instead of new to avoid default constructors and destructors which can sometimes kill performance
// replaced unique_ptr<T[]> in my code
// WARNING: does not call constructors or destructors, using it with any kind of std:: types is NOT safe
// but UnsafeArray<int> or UnsafeArray<struct with int, float, bool> is perfectly safe, but the data will be uninitialized
template <typename T>
struct UnsafeArray {
	T* ptr = nullptr;
	size_t size = 0;

	inline UnsafeArray () {}

	inline UnsafeArray (size_t size) {
		ptr = (T*)malloc(size * sizeof(T));
		this->size = size;
	}
	inline ~UnsafeArray () {
		if (ptr)
			free(ptr);
	}

	inline void resize (size_t new_size) {
		if (new_size == size)
			return;

		// get old
		T* old_ptr = ptr;
		T* old_size = size;

		// alloc new
		ptr = (T*)malloc(new_size * sizeof(T));
		size = new_size;

		if (old_ptr) {
			// copy old elements into new
			memcpy(ptr, old_ptr, _min(size, old_size) * sizeof(T));

			// free old
			free(old_ptr);
		}
	}

	inline T const& operator[] (size_t index) const {
		assert(index < size);
		return ptr[index];
	}
	inline T& operator[] (size_t index) {
		assert(index < size);
		return ptr[index];
	}

	// copy
	inline UnsafeArray (UnsafeArray const& r) = delete;
	inline UnsafeArray& operator= (UnsafeArray const& r) = delete;
	// move
	inline UnsafeArray (UnsafeArray&& r) {				std::swap(ptr, r.ptr); std::swap(size, r.size); }
	inline UnsafeArray& operator= (UnsafeArray&& r) {	std::swap(ptr, r.ptr); std::swap(size, r.size); return *this; }
};
// std::vector style array that is implemented with malloc instead of new to avoid default constructors and destructors which can sometimes kill performance
// WARNING: does not call constructors or destructors, using it with any kind of std:: types is NOT safe
// but UnsafeVector<int> or UnsafeVector<struct with int, float, bool> is perfectly safe, but the data will be uninitialized
template <typename T, size_t MIN_CAP=16>
struct UnsafeVector {
	static constexpr float DEFAULT_GROW_FAC = 2;

	T* ptr = nullptr;
	size_t size = 0;
	size_t capacity = 0;
	float grow_fac = DEFAULT_GROW_FAC;

	// empty vector, with no memory allocated
	__forceinline UnsafeVector () {}

	// empty vector, with initial allocation
	inline UnsafeVector (size_t capacity, float grow_fac=DEFAULT_GROW_FAC) {
		capacity = _max(capacity, MIN_CAP);

		this->capacity = capacity;
		_grow_capacity(size);

		this->ptr = (T*)malloc(this->capacity * sizeof(T));
		this->grow_fac = grow_fac;
	}
	inline ~UnsafeVector () {
		if (ptr)
			free(ptr);
	}

private:
	// grow capacity to fit new size
	inline void _grow_capacity (size_t new_size) {
		while (capacity < new_size)
			capacity = (size_t)roundf( (float)capacity * grow_fac );
	}

	inline void _change_capacity (size_t new_cap) {
		if (new_cap == capacity)
			return;
		
		// get old
		T* old_ptr = ptr;
		size_t old_cap = capacity;

		// alloc new
		if (new_cap == 0) {
			ptr = nullptr;
		} else {
			ptr = (T*)malloc(new_cap * sizeof(T));
		}

		capacity = new_cap;

		if (old_ptr) {
			{
				// copy old elements into new
				memcpy(ptr, old_ptr, _min(capacity, old_cap) * sizeof(T));
			}
			// free old
			free(old_ptr);
		}
	}
public:

	// never shrinks capacity
	inline void resize (size_t new_size) {
		this->size = new_size;

		if (new_size > capacity) {
			_grow_capacity(new_size);
			_change_capacity(capacity);
		}
	}
	inline void shrink_to_fit () {
		_change_capacity(size);
	}

	inline void push_back (T val) {
		int old_size = size;
		resize(size + 1);
		ptr[old_size] = std::move(val);
	}

	inline T const& operator[] (size_t index) const {
		assert(index < size);
		return ptr[index];
	}
	inline T& operator[] (size_t index) {
		assert(index < size);
		return ptr[index];
	}

	static void swap (UnsafeVector& l, UnsafeVector& r) {
		std::swap(l.ptr, r.ptr);
		std::swap(l.size, r.size);
		std::swap(l.capacity, r.capacity);
		std::swap(l.grow_fac, r.grow_fac);
	}

	// copy
	inline UnsafeVector (UnsafeVector const& r) = delete;
	inline UnsafeVector& operator= (UnsafeVector const& r) = delete;
	// move
	inline UnsafeVector (UnsafeVector&& r) {			swap(*this, r); }
	inline UnsafeVector& operator= (UnsafeVector&& r) {	swap(*this, r); return *this; }
};
