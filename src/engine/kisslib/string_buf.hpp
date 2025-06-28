#pragma once
#include "stdlib.h"

void* buffer_realloc (void* cur_ptr, size_t cur_size, size_t new_size) {

}

template <typename T>
struct array_buf {
	T* buf = nullptr;
	size_t size = 0;
	size_t capacity = 0;

	array_buf () {}
	array_buf (int capacity) {
		realloc(capacity);
	}

	~array_buf () {
		if (buf)
			_free(buf);
	}



	// MOVE ONLY

	static void swap (array_buf& l, array_buf& r) {
		std::swap(l.buf, r.buf);
		std::swap(l.size, r.size);
		std::swap(l.capacity, r.capacity);
	}

	// copy
	array_buf& operator= (array_buf& r) = delete;
	array_buf (array_buf& r) = delete;
	// move
	array_buf& operator= (array_buf&& r) {	swap(*this, r);	return *this; }
	array_buf (array_buf&& r) {				swap(*this, r); }
};
