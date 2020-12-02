#pragma once
#include "common.hpp"

template <typename T>
struct myvector {
	T*			_data = nullptr;
	uint32_t	_length = 0
	uint32_t	_capacity = 0;
	
	friend void std::swap (myvector& l, myvector& r) {
		std::swap(l._data, r._data);
		std::swap(l._length, r._length);
		std::swap(l._capacity, r._capacity);
	}

	myvector () {}

	myvector (uint32_t capacity) {
		_alloc(capacity);
	}
	myvector (uint32_t length, uint32_t capacity=0) {
		_alloc(std::max(capacity, length));
		resize(length);
	}

	~myvector () {
		resize(0);
		_free();
	}

	// no implicit copy
	myvector (myvector const& r) = delete;
	myvector& operator= (myvector const& r) = delete;

	myvector (myvector&& r) {
		swap(*this, r);
	}
	myvector& operator= (myvector&& r) {
		swap(*this, r);
		return *this;
	}

	void reserve (uint32_t capacity) {
		if (capacity > _capacity)
			_realloc(_length, _align_cap(capacity));
	}
	void resize (uint32_t length) {
		if (length > _capacity) {
			_realloc(length, _grow_cap(_capacity));
		} else {
			auto shrinked = _shrink_cap(_capacity);

			if (length <= shrinked)
			_realloc(length, shrinked);
		}

		_init(_length, length - _length);
		_length = length;
	}

	void push_back (T val) {
		
	}

	template <typename... ARGS>
	void emplace_back (ARGS... args) {
		_length++;
		if (length > _capacity) {
			_realloc(length, _grow_cap(_capacity));
	}

	myvector copy () {
		myvector c (_count);
		memcpy(c._data, _data, sizeof(T) * _length);
		return c;
	}

	T* begin () {
		return _data;
	}
	T* end () {
		return _data + _length;
	}

	size_t size () {
		return _length;
	}

	void _alloc (uint32_t capacity) {
		ZoneScopedC(tracy::Color::Crimson);

		assert(_data == nullptr);

		_data = nullptr;
		_length = 0;
		_capacity = capacity;

		if (capacity > 0) {
			_data = _aligned_malloc(sizeof(T) * capacity, alignof(T));
			TracyAlloc(_data, sizeof(T) * capacity);
		}
	}
	void _free () {
		if (_data) { // avoid free calls in move operator etc.
			ZoneScopedC(tracy::Color::Crimson);

			free(_data);
			TracyFree(_data);
		}
		_length = 0;
		_capacity = 0;
	}

	void _init (uint32_t first, uint32_t count) {
		for (uint32_t i=first; i<count; ++i)
			new (_data + i) T ();
	}
	void _deinit (uint32_t first, uint32_t count) {
		for (uint32_t i=first; i<count; ++i)
			_data[i].~T();
	}

	void _realloc (uint32_t new_length, uint32_t new_capacity) {
		ZoneScopedc(tracy::Color::Crimson);

		new_capacity = std::max(new_length, new_capacity);

		T* new_data = nullptr;
		if (new_capacity > 0) {
			new_data = _aligned_malloc(sizeof(T) * new_capacity, alignof(T));
			TracyAlloc(_data, sizeof(T) * capacity);
		}

		uint32_t copy_end = std::min(new_length, _length);
		memcpy(new_data, _data, copy_end);
		
		if (new_length > _length)
			_init(_length, new_length - _length);
		else
			_deinit(new_length, _length - new_length);

		if (_data) {
			free(_data);
			TracyFree(_data);
		}

		_data = new_capacity;
		_length = _length;
		_capacity = new_capacity;
	}
};
