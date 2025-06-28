#pragma once
#include "nlohmann/json_fwd.hpp"
#include "nlohmann/json.hpp"
#include "file_io.hpp"
#include "kissmath.hpp"
#include "imgui/dear_imgui.hpp"
#include "tracy/Tracy.hpp"
#include <memory>
using json = nlohmann::ordered_json;

#ifndef SERIALIZE_LOG
	#include "stdio.h"
	#define SERIALIZE_LOG(type, ...) fprintf(stderr, __VA_ARGS__)
#endif

namespace serialize {
	inline bool load_json (char const* filename, json* j) {
		ZoneScoped;

		std::string str;
		if (!kiss::load_text_file(filename, &str)) {
			SERIALIZE_LOG(WARNING, "[load_json] Can't load file \"%s\", using defaults.", filename);
			return false;
		}
		try {
			*j = json::parse(str, nullptr, true, true); // last arg: ignore_comments
		} catch (std::exception& ex) {
			SERIALIZE_LOG(ERROR, "[load_json] Error in json::parse: %s", ex.what());
			return false;
		}
		return true;
	}
	inline json load_json (char const* filename) {
		json j;
		load_json(filename, &j);
		return std::move(j);
	}

	inline bool save_json (char const* filename, json const& json) {
		ZoneScoped;

		std::string json_str;
		try {
			json_str = json.dump(1, '\t');
		} catch (std::exception& ex) {
			SERIALIZE_LOG(ERROR, "Error when serializing something: %s", ex.what());
			return true;
		}

		if (!kiss::save_text_file(filename, json_str)) {
			SERIALIZE_LOG(ERROR, "Error when serializing something: Can't save file \"%s\"", filename);
			return false;
		}
		return true;
	}

	template <typename FUNC>
	inline void json_load (const char* filepath, FUNC from_json) {
		ZoneScoped;
		try {
			json json;
			if (load_json(filepath, &json)) {
				from_json(json);
			}
		} catch (std::exception& ex) {
			log(ERROR, "Error when deserializing something: %s", ex.what());
		}
	}
	template <typename FUNC>
	inline void json_save (const char* filepath, FUNC to_json) {
		ZoneScoped;
		try {
			json json;
			to_json(json);
			save_json(filepath, json);
		} catch (std::exception& ex) {
			log(ERROR, "Error when serializing something: %s", ex.what());
		}
	}
}

#if 0
template <typename T>
inline bool save (char const* filename, T const& obj) {
	ZoneScoped;
	json json = obj;
	return save_json(filename, json);
}

template <typename T>
inline bool load (char const* filename, T* obj) {
	ZoneScoped;

	try {
		json json;
		if (load_json(filename, &json)) {
			json.get_to(*obj);
			return true;
		}
	} catch (std::exception& ex) {
		SERIALIZE_LOG(ERROR, "Error when deserializing something: %s", ex.what());
	}
	return false;
}

template <typename T>
inline T load (char const* filename) {
	T t = T();
	load(filename, &t);
	return t;
}
#endif

#define _JSON_EXPAND( x ) x
#define _JSON_GET_MACRO(_1,_2,_3,_4,_5,_6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, NAME,...) NAME

#define _JSON_PASTE(...) _JSON_EXPAND(_JSON_GET_MACRO(__VA_ARGS__, \
	_JSON_PASTE20, _JSON_PASTE19, _JSON_PASTE18, _JSON_PASTE17, _JSON_PASTE16, _JSON_PASTE15, _JSON_PASTE14, _JSON_PASTE13, _JSON_PASTE12, _JSON_PASTE11, \
	_JSON_PASTE10, _JSON_PASTE9, _JSON_PASTE8, _JSON_PASTE7, _JSON_PASTE6, _JSON_PASTE5, _JSON_PASTE4, _JSON_PASTE3, _JSON_PASTE2, _JSON_PASTE1 \
	)(__VA_ARGS__))
#define _JSON_PASTE2(func,  v1)																						func(v1)
#define _JSON_PASTE3(func,  v1, v2)																					_JSON_PASTE2(func, v1) _JSON_PASTE2(func, v2)
#define _JSON_PASTE4(func,  v1, v2, v3)																				_JSON_PASTE2(func, v1) _JSON_PASTE3(func, v2, v3)
#define _JSON_PASTE5(func,  v1, v2, v3, v4)																			_JSON_PASTE2(func, v1) _JSON_PASTE4(func, v2, v3, v4)
#define _JSON_PASTE6(func,  v1, v2, v3, v4, v5)																		_JSON_PASTE2(func, v1) _JSON_PASTE5(func, v2, v3, v4, v5)
#define _JSON_PASTE7(func,  v1, v2, v3, v4, v5, v6)																	_JSON_PASTE2(func, v1) _JSON_PASTE6(func, v2, v3, v4, v5, v6)
#define _JSON_PASTE8(func,  v1, v2, v3, v4, v5, v6, v7)																_JSON_PASTE2(func, v1) _JSON_PASTE7(func, v2, v3, v4, v5, v6, v7)
#define _JSON_PASTE9(func,  v1, v2, v3, v4, v5, v6, v7, v8)															_JSON_PASTE2(func, v1) _JSON_PASTE8(func, v2, v3, v4, v5, v6, v7, v8)
#define _JSON_PASTE10(func, v1, v2, v3, v4, v5, v6, v7, v8, v9)														_JSON_PASTE2(func, v1) _JSON_PASTE9(func, v2, v3, v4, v5, v6, v7, v8, v9)
#define _JSON_PASTE11(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10)												_JSON_PASTE2(func, v1) _JSON_PASTE10(func, v2, v3, v4, v5, v6, v7, v8, v9, v10)
#define _JSON_PASTE12(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11)											_JSON_PASTE2(func, v1) _JSON_PASTE11(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11)
#define _JSON_PASTE13(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12)										_JSON_PASTE2(func, v1) _JSON_PASTE12(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12)
#define _JSON_PASTE14(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13)									_JSON_PASTE2(func, v1) _JSON_PASTE13(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13)
#define _JSON_PASTE15(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14)							_JSON_PASTE2(func, v1) _JSON_PASTE14(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14)
#define _JSON_PASTE16(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15)						_JSON_PASTE2(func, v1) _JSON_PASTE15(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15)
#define _JSON_PASTE17(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16)					_JSON_PASTE2(func, v1) _JSON_PASTE16(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16)
#define _JSON_PASTE18(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17)				_JSON_PASTE2(func, v1) _JSON_PASTE17(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17)
#define _JSON_PASTE19(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18)		_JSON_PASTE2(func, v1) _JSON_PASTE18(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18)
#define _JSON_PASTE20(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19)	_JSON_PASTE2(func, v1) _JSON_PASTE19(func, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19)

#define _JSON_TO(v1) j[#v1] = t.v1;
#define _JSON_FROM(v1) if (j.contains(#v1)) j.at(#v1).get_to(t.v1); // try get value from json

#define SERIALIZE(Type, ...)  \
    friend void to_json(nlohmann::ordered_json& j, const Type& t) { _JSON_EXPAND(_JSON_PASTE(_JSON_TO, __VA_ARGS__)) } \
    friend void from_json(const nlohmann::ordered_json& j, Type& t) { _JSON_EXPAND(_JSON_PASTE(_JSON_FROM, __VA_ARGS__)) }

#define SERIALIZE_OUT_OF_CLASS(Type, ...)  \
    void to_json(nlohmann::ordered_json& j, const Type& t) { _JSON_EXPAND(_JSON_PASTE(_JSON_TO, __VA_ARGS__)) } \
    void from_json(const nlohmann::ordered_json& j, Type& t) { _JSON_EXPAND(_JSON_PASTE(_JSON_FROM, __VA_ARGS__)) }

namespace nlohmann {
	template<typename T>
	struct adl_serializer<std::unique_ptr<T>> {
		using type = std::unique_ptr<T>;
		static void to_json(ordered_json& j, const type& val) {
			j = *val;
		}
		static void from_json(const ordered_json& j, type& val) {
			val = std::make_unique<T>();
			j.get_to(*val);
		}
	};
	
	template <>	struct adl_serializer<int2> {
		using type = int2;
		static void to_json(ordered_json& j, const type& val) {
			j = { val.x, val.y };
		}
		static void from_json(const ordered_json& j, type& val) {
			j.at(0).get_to(val.x);
			j.at(1).get_to(val.y);
		}
	};

	template <>	struct adl_serializer<int3> {
		using type = int3;
		static void to_json(ordered_json& j, const type& val) {
			j = { val.x, val.y, val.z };
		}
		static void from_json(const ordered_json& j, type& val) {
			j.at(0).get_to(val.x);
			j.at(1).get_to(val.y);
			j.at(2).get_to(val.z);
		}
	};

	template <>	struct adl_serializer<int4> {
		using type = int4;
		static void to_json(ordered_json& j, const type& val) {
			j = { val.x, val.y, val.z, val.w };
		}
		static void from_json(const ordered_json& j, type& val) {
			j.at(0).get_to(val.x);
			j.at(1).get_to(val.y);
			j.at(2).get_to(val.z);
			j.at(3).get_to(val.w);
		}
	};

	template <>	struct adl_serializer<float2> {
		using type = float2;
		static void to_json(ordered_json& j, const type& val) {
			j = { val.x, val.y };
		}
		static void from_json(const ordered_json& j, type& val) {
			j.at(0).get_to(val.x);
			j.at(1).get_to(val.y);
		}
	};

	template <>	struct adl_serializer<float3> {
		using type = float3;
		static void to_json(ordered_json& j, const type& val) {
			j = { val.x, val.y, val.z };
		}
		static void from_json(const ordered_json& j, type& val) {
			j.at(0).get_to(val.x);
			j.at(1).get_to(val.y);
			j.at(2).get_to(val.z);
		}
	};

	template <>	struct adl_serializer<float4> {
		using type = float4;
		static void to_json(ordered_json& j, const type& val) {
			j = { val.x, val.y, val.z, val.w };
		}
		static void from_json(const ordered_json& j, type& val) {
			j.at(0).get_to(val.x);
			j.at(1).get_to(val.y);
			j.at(2).get_to(val.z);
			j.at(3).get_to(val.w);
		}
	};

	template <>	struct adl_serializer<srgb8> {
		using type = srgb8;
		static void to_json(ordered_json& j, const type& val) {
			j = { val.x, val.y, val.z };
		}
		static void from_json(const ordered_json& j, type& val) {
			j.at(0).get_to(val.x);
			j.at(1).get_to(val.y);
			j.at(2).get_to(val.z);
		}
	};
	template <>	struct adl_serializer<srgba8> {
		using type = srgba8;
		static void to_json(ordered_json& j, const type& val) {
			j = { val.x, val.y, val.z, val.w };
		}
		static void from_json(const ordered_json& j, type& val) {
			j.at(0).get_to(val.x);
			j.at(1).get_to(val.y);
			j.at(2).get_to(val.z);
			j.at(3).get_to(val.w);
		}
	};
}

#undef SERIALIZE_LOG
