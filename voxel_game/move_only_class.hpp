#pragma once

////// Helper macros for declaring classes containing resources that can not be copied
// useful for opengl textures, buffers, etc., or other resources like memory buffers, that you would rather copy explicitly
// basicly any class that contains a raw pointer or system handle where copying the class would cause the ptr to be copied which is wrong since there will probably now be a double free or no free at all

// default constructor should result in "empty" class that still destructs, but the destructor does nothing since the class is empty, this is useful
// no copy ctor/assign, only move
// use like:
/*
	class T {
		MOVE_ONLY_CLASS(T) // move operators implemented with swap
		void* my_resource = nullptr;
	public:
		~T () {
			// destructor can destruct default constructed class
			free(my_resource); // free(nullptr) is ok
			// or
			if (my_resource)
				api_delete(my_resource);
		}
		static T my_factory (args...) {
			T t;
			// construct a T
			return t;
		}
	};
	void swap (T& l, T& r) {
		std::swap(l.my_resource, r.my_resource);
	}
*/
#define MOVE_ONLY_CLASS(CLASS) \
	public: \
	friend void swap (CLASS& l, CLASS& r); \
	CLASS& operator= (CLASS& r) = delete; \
	CLASS (CLASS& r) = delete; \
	CLASS& operator= (CLASS&& r) {	swap(*this, r);	return *this; } \
	CLASS (CLASS&& r) {				swap(*this, r); } \
	private:

#define MOVE_ONLY_CLASS_DECL(CLASS) \
	public: \
	friend void swap (CLASS& l, CLASS& r); \
	CLASS& operator= (CLASS& r) = delete; \
	CLASS (CLASS& r) = delete; \
	CLASS& operator= (CLASS&& r); \
	CLASS (CLASS&& r); \
	private:
#define MOVE_ONLY_CLASS_DEF(CLASS) \
	CLASS& CLASS::operator= (CLASS&& r) {	swap(*this, r);	return *this; } \
	CLASS::CLASS (CLASS&& r) {				swap(*this, r); }

// For classes that cannot be copied or moved at all, for example because they contain data that has to stay allocated at the same address (eg. ReadDirectoryChangesW in overlapped mode needs a pointer to a buffer)
// Can still pass the class around by allocating it with new or make_unique
#define NO_MOVE_COPY_CLASS(CLASS) \
	public: \
	CLASS& operator= (CLASS& r) = delete; \
	CLASS (CLASS& r) = delete; \
	CLASS& operator= (CLASS&& r) = delete; \
	CLASS (CLASS&& r) = delete; \
	private:
