#pragma once
#include "nlohmann/json_fwd.hpp"
using nlohmann::json;

static constexpr const char* settings_filepath = "settings.json";

class Game;

//void save_settings (Game const& game);
//void load_settings (Game& game);

#include "game.hpp"
#include "util/file_io.hpp"

template <typename T>
void save (std::string_view filename, T const& obj) {
	json json = obj;
	save_text_file(filename.data(), json.dump(2));
}
template <typename T>
void load (std::string_view filename, T& obj) {
	std::string str;
	if (!load_text_file(filename.data(), &str))
		return;
	json json = json::parse(str);
	obj = json.get<T>();
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
#define _JSON_PASTE11(func, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10) _JSON_PASTE2(func, v1)_JSON_PASTE8(func, v2, v3, v4, v5, v6, v7, v8, v9, v10)

#define _JSON_TO(v1) j[#v1] = t.v1;
#define _JSON_FROM(v1) if (j.contains(#v1)) j.at(#v1).get_to(t.v1); // try get value from json

#define SERIALIZE(Type, ...)  \
    friend void to_json(nlohmann::json& j, const Type& t) { _JSON_EXPAND(_JSON_PASTE(_JSON_TO, __VA_ARGS__)) } \
    friend void from_json(const nlohmann::json& j, Type& t) { _JSON_EXPAND(_JSON_PASTE(_JSON_FROM, __VA_ARGS__)) }

#define SERIALIZE_OUT_OF_CLASS(Type, ...)  \
    void to_json(nlohmann::json& j, const Type& t) { _JSON_EXPAND(_JSON_PASTE(_JSON_TO, __VA_ARGS__)) } \
    void from_json(const nlohmann::json& j, Type& t) { _JSON_EXPAND(_JSON_PASTE(_JSON_FROM, __VA_ARGS__)) }

