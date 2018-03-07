#include "compiler_platform_arch.hpp"
#include "preprocessor_stuff.hpp"
#include "assert.hpp"

//
template<typename CAST_T, typename T>
static constexpr bool _safe_cast (T x);

template<> constexpr bool _safe_cast<u32, u64> (u64 x) { return x <= 0xffffffffull; }
template<> constexpr bool _safe_cast<s32, u64> (u64 x) { return x <= 0x7fffffffull; }
template<> constexpr bool _safe_cast<u64, s64> (s64 x) { return x >= 0; }

#define safe_cast(cast_t, val) _safe_cast<cast_t>(val)

//

static u32 strlen (utf32 const* str) {
	u32 ret = 0;
	while (*str++) ++ret;
	return ret;
}
template <u32 N>
static u32 strlen (utf32 const (& str)[N]) {
	STATIC_ASSERT(N >= 1);
	return N -1;
}

template <typename FUNC>
struct At_Scope_Exit {
	FUNC	f;
	void operator= (At_Scope_Exit &) = delete;
	
	FORCEINLINE At_Scope_Exit (FUNC f): f(f) {}
	FORCEINLINE ~At_Scope_Exit () { f(); }
};

struct _Defer_Helper {};

template<typename FUNC>
static FORCEINLINE At_Scope_Exit<FUNC> operator+(_Defer_Helper, FUNC f) {
	return At_Scope_Exit<FUNC>(f);
}

#define _defer(counter) auto CONCAT(_defer_helper, counter) = _Defer_Helper() +[&] () 
#define defer _defer(__COUNTER__)
// use like: defer { lambda code };

#undef DEFINE_ENUM_FLAG_OPS
#define DEFINE_ENUM_FLAG_OPS(TYPE, UNDERLYING_TYPE) \
	static FORCEINLINE TYPE& operator|= (TYPE& l, TYPE r) { \
		return l = (TYPE)((UNDERLYING_TYPE)l | (UNDERLYING_TYPE)r); \
	} \
	static FORCEINLINE TYPE& operator&= (TYPE& l, TYPE r) { \
		return l = (TYPE)((UNDERLYING_TYPE)l & (UNDERLYING_TYPE)r); \
	} \
	static FORCEINLINE TYPE operator| (TYPE l, TYPE r) { \
		return (TYPE)((UNDERLYING_TYPE)l | (UNDERLYING_TYPE)r); \
	} \
	static FORCEINLINE TYPE operator& (TYPE l, TYPE r) { \
		return (TYPE)((UNDERLYING_TYPE)l & (UNDERLYING_TYPE)r); \
	} \
	static FORCEINLINE TYPE operator~ (TYPE e) { \
		return (TYPE)(~(UNDERLYING_TYPE)e); \
	}

#define DEFINE_ENUM_ITER_OPS(TYPE, UNDERLYING_TYPE) \
	static FORCEINLINE TYPE& operator++ (TYPE& val) { \
		return val = (TYPE)((UNDERLYING_TYPE)val +1); \
	}

typedef std::string		str;
typedef str const& 		strcr;

template<typename T, typename FUNC>
static T* lsearch (std::vector<T>& arr, FUNC comp_with) {
	for (T& x : arr) {
		if (comp_with(&x)) return &x; // found
	}
	return nullptr; // not found
}

template <typename T> static typename std::vector<T>::iterator vector_append (std::vector<T>* vec) {
	uptr old_len = vec->size();
	vec->resize( old_len +1 );
	return vec->begin() +old_len;
	
}
template <typename T> static typename std::vector<T>::iterator vector_append (std::vector<T>* vec, uptr n) {
	uptr old_len = vec->size();
	vec->resize( old_len +n );
	return vec->begin() +old_len;
}

template <typename T> static uptr vector_size_bytes (std::vector<T> const& vec) {
	return vec.size() * sizeof(T);
}

// "foo/bar/README.txt"	-> "foo/bar/"
// "README.txt"			-> ""
// ""					-> ""
static str get_path_dir (strcr path) {
	auto last_slash = path.begin();
	for (auto ch=path.begin(); ch!=path.end(); ++ch) {
		if (*ch == '/') last_slash = ch +1;
	}
	return std::string(path.begin(), last_slash);
}
// "foo/bar/README.txt"	-> return true; ext = "txt"
// "README.txt"			-> return true; ext = "txt"
// "README."			-> return true; ext = ""
// "README"				-> return false;
// ""					-> return false;
// ".ext"				-> return true; ext = "ext";
static bool get_fileext (strcr path, str* ext) {
	for (auto c = path.end(); c != path.begin();) {
		--c;
		if (*c == '.') {
			*ext = str(c +1, path.end());
			return true;
		}
	}
	return false;
}

static u64 get_file_size (FILE* f) {
	fseek(f, 0, SEEK_END);
	u64 file_size = ftell(f); // only 32 support for now
	rewind(f);
	return file_size;
}

struct Data_Block {
	byte*		data;
	u64			size;
	
	void free () {
		delete[] data;
	}
	
	static Data_Block alloc (u64 s) {
		return { new byte[s], s };
	}
};

// reads entire file into already allocated buffer
static bool read_entire_file (cstr filename, void* buf, u64 expected_file_size) {
	FILE* f = fopen(filename, "rb"); // read binary
	if (!f) return false; // fail
	
	defer { fclose(f); };
	
	u64 file_size = get_file_size(f);
	if (file_size != expected_file_size) return false; // fail
	
	auto ret = fread(buf, 1,file_size, f);
	dbg_assert(ret == file_size);
	
	return true;
}
// reads entire file into std::vector
static bool read_entire_file (cstr filename, Data_Block* data) {
	FILE* f = fopen(filename, "rb"); // read binary because i don't want to convert "\r\n" to "\n"
	if (!f) return false; // fail
	
	defer { fclose(f); };
	
	data->size = get_file_size(f);
	
	data->data = (byte*)malloc(data->size);
	
	auto ret = fread(data->data, 1,data->size, f);
	dbg_assert(ret == data->size);
	
	return true;
}
// reads text file into a std::string by overwriting it's previous contents
static bool read_text_file (cstr filename, std::string* out) {
	FILE* f = fopen(filename, "rb"); // read binary because i don't want to convert "\r\n" to "\n"
	if (!f) return false; // fail
	
	defer { fclose(f); };
	
	u64 file_size = get_file_size(f);
	
	out->resize(file_size);
	
	auto ret = fread(&(*out)[0], 1,file_size, f);
	dbg_assert(ret == file_size);
	
	return true;
}

// overwrites or creates a file with buf
static bool overwrite_file (cstr filename, void const* buf, u64 write_size) {
	FILE* f = fopen(filename, "wb"); // write binary (overwrite file if exists / create if not exists)
	if (!f) return false; // fail
	
	defer { fclose(f); };
	
	auto ret = fwrite(buf, 1,write_size, f);
	dbg_assert(ret == write_size);
	
	return true;
}

static utf32 utf8_to_utf32 (utf8 const** cur) {
	
	if ((*(*cur) & 0b10000000) == 0b00000000) {
		return (utf32)(u8)(*(*cur)++);
	}
	if ((*(*cur) & 0b11100000) == 0b11000000) {
		dbg_assert(((*cur)[1] & 0b11000000) == 0b10000000);
		auto a = (utf32)(u8)(*(*cur)++ & 0b00011111);
		auto b = (utf32)(u8)(*(*cur)++ & 0b00111111);
		return a<<6|b;
	}
	if ((*(*cur) & 0b11110000) == 0b11100000) {
		dbg_assert(((*cur)[1] & 0b11000000) == 0b10000000);
		dbg_assert(((*cur)[2] & 0b11000000) == 0b10000000);
		auto a = (utf32)(u8)(*(*cur)++ & 0b00001111);
		auto b = (utf32)(u8)(*(*cur)++ & 0b00111111);
		auto c = (utf32)(u8)(*(*cur)++ & 0b00111111);
		return a<<12|b<<6|c;
	}
	if ((*(*cur) & 0b11111000) == 0b11110000) {
		dbg_assert(((*cur)[1] & 0b11000000) == 0b10000000);
		dbg_assert(((*cur)[2] & 0b11000000) == 0b10000000);
		dbg_assert(((*cur)[3] & 0b11000000) == 0b10000000);
		auto a = (utf32)(u8)(*(*cur)++ & 0b00000111);
		auto b = (utf32)(u8)(*(*cur)++ & 0b00111111);
		auto c = (utf32)(u8)(*(*cur)++ & 0b00111111);
		auto d = (utf32)(u8)(*(*cur)++ & 0b00111111);
		return a<<18|b<<12|c<<6|d;
	}
	dbg_assert(false);
	
	return (utf32)(u32)-1;
}

static std::basic_string<utf32> utf8_to_utf32 (std::string utf8_str) {
	std::basic_string<utf32> ret;
	ret.reserve(utf8_str.length());
	
	char const* cur = utf8_str.data();
	char const* end = cur +utf8_str.length();
	
	while (cur != end) {
		ret.push_back( utf8_to_utf32(&cur) );
	}
	
	return ret;
}

namespace parse_n {
	static bool whitespace_c (char c) {	return c == ' ' || c == '\t'; }
	
	static bool whitespace (char** pcur) {
		char* cur = *pcur;
		if (!whitespace_c(*cur)) return false;
		
		do {
			++cur;
		} while (whitespace_c(*cur));
		
		*pcur = cur;
		return true;
	}
	
	static bool char_ (char** pcur, char c) {
		if (**pcur != c) return false;
		(*pcur)++;
		return true;
	}
	
}
