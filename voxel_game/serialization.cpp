#include "serialization.hpp"
#include "nlohmann/json.hpp"
#include "kissmath/float2.hpp"
using namespace kissmath;

namespace nlohmann {
	template <>
	struct adl_serializer<float2> {
		using type = float2;
		static void to_json(json& j, const type& val) {
			j = { val.x, val.y };
		}

		static void from_json(const json& j, type& val) {
			if (j.is_array()) {
				j.at(0).get_to(val.x);

				auto sz = j.size();
				if (sz == 1) {
					val.y = val.x;
				} else {
					if (sz >= 2) j.at(1).get_to(val.y);
				}
			} else {
				j.at("x").get_to(val.x);
				j.at("y").get_to(val.y);
			}
		}
	};
}

struct Blah2 {
	float2 a;
	float2 b;

	SERIALIZE(Blah2, a, b)
};

struct Blah {
	Blah2 blah2;

	int i;
	float f;
	std::string s;

	std::vector<int> is;

	enum EEE {
		E_INVALID=0,
		E_STARTING,
		E_FINISHED,
	};

	NLOHMANN_JSON_SERIALIZE_ENUM( Blah::EEE, {
		{ Blah::E_INVALID, nullptr },
		{ Blah::E_STARTING, "E_STARTING" },
		{ Blah::E_FINISHED, "E_FINISHED" },
		})

	EEE eee;

	SERIALIZE(Blah, blah2, i, f, s, is, eee)
};

bool test2 = [] () {

	Blah b;
#if 1
	b.blah2 = { {1, -1}, {2, -2} };
	b.i = 5;
	b.f = 7.9999999f;
	b.s = "Hello WOrld !!!!";
	b.is = { 1,2,2,3,4,4,56,6 };
	b.eee = Blah::E_INVALID;
	
	save("test.json", b);
#else
	load("test.json", b);
#endif

	return true;
} ();
