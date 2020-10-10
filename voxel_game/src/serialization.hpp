#pragma once
#include "stdafx.hpp"

#include "nlohmann/json_fwd.hpp"
#include "nlohmann/json.hpp"
#include "util/file_io.hpp"
#include "kissmath.hpp"
using json = nlohmann::ordered_json;

template <typename T>
bool save (char const* filename, T const& obj) {
	json json = obj;
	std::string json_str;
	try {
		json_str = json.dump(1, '\t');
	} catch (std::exception& ex) {
		clog(ERROR, "Error when serializing something: %s", ex.what());
		return true;
	}

	if (!kiss::save_text_file(filename, json_str)) {
		clog(ERROR, "Error when serializing something: Can't save file \"%s\"", filename);
		return false;
	}
	return true;
}

template <typename T>
bool load (char const* filename, T* obj) {
	std::string str;
	if (!kiss::load_text_file(filename, &str)) {
		clog(WARNING, "Can't load file \"%s\", using defaults.", filename);
		return false;
	}
	try {
		json json = json::parse(str);
		*obj = json.get<T>();
	} catch (std::exception& ex) {
		clog(ERROR, "Error when deserializing something: %s", ex.what());
		return false;
	}
	return true;
}

template <typename T>
T load (char const* filename) {
	T t = T();
	load(filename, &t);
	return t;
}

#define _JSON_EXPAND( x ) x
#define _JSON_GET_MACRO(_1,_2,_3,_4,_5,_6, _7, _8, _9, _10, _11, NAME,...) NAME

#define _JSON_PASTE(...) _JSON_EXPAND(_JSON_GET_MACRO(__VA_ARGS__, _JSON_PASTE11, _JSON_PASTE10, _JSON_PASTE9, _JSON_PASTE8, _JSON_PASTE7, \
                                                                   _JSON_PASTE6, _JSON_PASTE5, _JSON_PASTE4, _JSON_PASTE3, _JSON_PASTE2, _JSON_PASTE1)(__VA_ARGS__))
#define _JSON_PASTE2(func,  v1)                                      func(v1)
#define _JSON_PASTE3(func,  v1, v2)                                  _JSON_PASTE2(func, v1) _JSON_PASTE2(func, v2)
#define _JSON_PASTE4(func,  v1, v2, v3)                              _JSON_PASTE2(func, v1) _JSON_PASTE3(func, v2, v3)
#define _JSON_PASTE5(func,  v1, v2, v3, v4)                          _JSON_PASTE2(func, v1) _JSON_PASTE4(func, v2, v3, v4)
#define _JSON_PASTE6(func,  v1, v2, v3, v4, v5)                      _JSON_PASTE2(func, v1) _JSON_PASTE5(func, v2, v3, v4, v5)
#define _JSON_PASTE7(func,  v1, v2, v3, v4, v5, v6)                  _JSON_PASTE2(func, v1) _JSON_PASTE6(func, v2, v3, v4, v5, v6)
#define _JSON_PASTE8(func,  v1, v2, v3, v4, v5, v6, v7)              _JSON_PASTE2(func, v1) _JSON_PASTE7(func, v2, v3, v4, v5, v6, v7)
#define _JSON_PASTE9(func,  v1, v2, v3, v4, v5, v6, v7, v8)          _JSON_PASTE2(func, v1) _JSON_PASTE8(func, v2, v3, v4, v5, v6, v7, v8)
#define _JSON_PASTE10(func, v1, v2, v3, v4, v5, v6, v7, v8, v9)      _JSON_PASTE2(func, v1) _JSON_PASTE8(func, v2, v3, v4, v5, v6, v7, v8, v9)
#define _JSON_PASTE11(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10) _JSON_PASTE2(func, v1) _JSON_PASTE8(func, v2, v3, v4, v5, v6, v7, v8, v9, v10)

#define _JSON_TO(v1) j[#v1] = t.v1;
#define _JSON_FROM(v1) if (j.contains(#v1)) j.at(#v1).get_to(t.v1); // try get value from json

#define SERIALIZE(Type, ...)  \
    friend void to_json(nlohmann::ordered_json& j, const Type& t) { _JSON_EXPAND(_JSON_PASTE(_JSON_TO, __VA_ARGS__)) } \
    friend void from_json(const nlohmann::ordered_json& j, Type& t) { _JSON_EXPAND(_JSON_PASTE(_JSON_FROM, __VA_ARGS__)) }

#define SERIALIZE_OUT_OF_CLASS(Type, ...)  \
    void to_json(nlohmann::ordered_json& j, const Type& t) { _JSON_EXPAND(_JSON_PASTE(_JSON_TO, __VA_ARGS__)) } \
    void from_json(const nlohmann::ordered_json& j, Type& t) { _JSON_EXPAND(_JSON_PASTE(_JSON_FROM, __VA_ARGS__)) }

namespace nlohmann {
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
