#pragma once
#include "globjects.hpp"
#include "glshader.hpp"
#include <array>

template <typename T>
struct Attribute {
	const char*		name;
	uint64_t		stride;
	uint64_t		offs;
	bool			normalized = false;
};

namespace gl {
	template <typename T> static inline constexpr bool int_normalized_by_default () { return false; }
	template<> static inline constexpr bool int_normalized_by_default<uint8v4> () { return true; }

	struct _Attribute {
		const char*		name;
		Enum			type;
		int				components;
		bool			normalized = false;
		uint64_t		stride;
		uint64_t		offs;
	};

	template <typename T> static inline constexpr _Attribute to_attrib (Attribute<T> a);

	template<> static inline constexpr _Attribute to_attrib<float   > (Attribute<float   > a) { return { a.name, Enum::FLOAT, 1, a.normalized, a.stride, a.offs }; }
	template<> static inline constexpr _Attribute to_attrib<float2  > (Attribute<float2  > a) { return { a.name, Enum::FLOAT, 2, a.normalized, a.stride, a.offs }; }
	template<> static inline constexpr _Attribute to_attrib<float3  > (Attribute<float3  > a) { return { a.name, Enum::FLOAT, 3, a.normalized, a.stride, a.offs }; }
	template<> static inline constexpr _Attribute to_attrib<float4  > (Attribute<float4  > a) { return { a.name, Enum::FLOAT, 4, a.normalized, a.stride, a.offs }; }
	template<> static inline constexpr _Attribute to_attrib<int     > (Attribute<int     > a) { return { a.name, Enum::INT  , 1, a.normalized, a.stride, a.offs }; }
	template<> static inline constexpr _Attribute to_attrib<int2    > (Attribute<int2    > a) { return { a.name, Enum::INT  , 2, a.normalized, a.stride, a.offs }; }
	template<> static inline constexpr _Attribute to_attrib<int3    > (Attribute<int3    > a) { return { a.name, Enum::INT  , 3, a.normalized, a.stride, a.offs }; }
	template<> static inline constexpr _Attribute to_attrib<int4    > (Attribute<int4    > a) { return { a.name, Enum::INT  , 4, a.normalized, a.stride, a.offs }; }
	template<> static inline constexpr _Attribute to_attrib<uint8v4 > (Attribute<uint8v4 > a) { return { a.name, Enum::UNSIGNED_BYTE, 4, a.normalized, a.stride, a.offs }; }
	//template<> static inline constexpr _Attribute to_attrib<bool    > (Attribute<bool    > a) { return { a.name, Type::BOOL , 1, a.normalized, a.stride, a.offs }; }
}

template<typename... Vals>
static inline constexpr std::array<gl::_Attribute, sizeof...(Vals)> _to_vertex_layout (Vals&&... vals) {
	return { gl::to_attrib(vals)... };
}

struct Vertex_Layout {
	gl::_Attribute const* attribs;
	int count;

	template<size_t N>
	Vertex_Layout (std::array<gl::_Attribute, N> const* arr): attribs{&(*arr)[0]}, count{N} {}
};

#undef _STRINGIFY
#undef STRINGIFY
#define _STRINGIFY(s) #s
#define STRINGIFY(s) _STRINGIFY(s)

// Assign a static constexpr std::array with the vertex layout data and return a pointer to it, This seems to be the only way to get the compiler
//  to pass a pointer to the layout data into bind_attrib_arrays without making a copy
// layout must be a static function because a static constexpr member gets evaluated when the struct is still "incomplete" ie. sizeof(Vertex) won't work
#define VERTEX_LAYOUT(vertexname, ...) static auto* layout () { \
		static constexpr auto a = _to_vertex_layout(__VA_ARGS__); \
		return &a; \
	}
#define VERTEX_ATTRIBUTE(vertexname, name) Attribute<decltype(name)>{ STRINGIFY(name), sizeof(vertexname), offsetof(vertexname, name) }

void bind_attrib_arrays (Vertex_Layout layout, Shader& shad);
