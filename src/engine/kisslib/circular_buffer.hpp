#pragma once
#include <cstddef>
#include <memory>
#include "assert.h"

// like a queue but is just implemented as a flat array, push() pop() like queue, but a push when the size is at it's max capacity will pop() automatically
//  push() add the items to the "head" of the collection which is accessed via [0] (so everything shifts by one index in the process, but the implementation does not copy anything)
//  pop() removes the items from the "tail" of the collection which is accessed via [size() - 1]
template <typename T>
class circular_buffer {
	std::unique_ptr<T[]> arr = nullptr;
	size_t cap = 0;
	size_t head; // next item index to be written
	size_t cnt = 0;

public:
	circular_buffer () {}
	circular_buffer (size_t capacity) {
		resize(capacity);
	}

	T* data () {
		return arr.get();
	}
	size_t capacity () const {
		return cap;
	}
	size_t count () const {
		return cnt;
	}

	void resize (size_t new_capacity) {
		auto old = std::move(*this);

		cap = new_capacity;
		arr = cap > 0 ? std::make_unique<T[]>(cap) : nullptr;

		cnt = cap <= old.cnt ? cap : old.cnt;
		for (int i=0; i<cnt; ++i) {
			arr[i] = old.arr[(old.head -cnt + old.cap + i) % old.cap];
		}

		head = cnt % cap;
	}

	void push (T const& item) {
		assert(cap > 0);

		// write in next free slot or overwrite if count == cap
		arr[head++] = item;
		head %= cap;

		if (cnt < cap)
			cnt++;
	}
	void push (T&& item) {
		assert(cap > 0);

		// write in next free slot or overwrite if count == cap
		arr[head++] = std::move(item);
		head %= cap;

		if (cnt < cap)
			cnt++;
	}

	T pop () {
		assert(cap > 0 && cnt > 0);

		size_t tail = head + (cap - cnt);
		tail %= cap;

		cnt--;
		return std::move( arr[tail] );
	}

	// get ith oldest value
	T& get_oldest (size_t index) {
		assert(index >= 0 && index < cnt);
		size_t i = head + (cap - cnt);
		i += index;
		i %= cap;
		return arr[i];
	}

	// get ith newest value
	T& get_newest (size_t index) {
		assert(index >= 0 && index < cnt);
		size_t i = head + cap - 1;
		i -= index;
		i = (i + cap) % cap;
		return arr[i];
	}

	T& operator [] (size_t index) {
		return get_newest(index);
	}
};
