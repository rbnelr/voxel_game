#pragma once
#include "common.hpp"
#include "glad/glad.h"
#include "tracyOpenGL.hpp"

namespace gl {

#if RENDERER_DEBUG_LABELS
	inline void set_object_label (GLenum type, GLuint handle, std::string_view label) {
		glObjectLabel(type, handle, (GLsizei)label.size(), label.data());
	}

	struct _ScopedGpuTrace {
		_ScopedGpuTrace (std::string_view name) {
			glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, (GLsizei)name.size(), name.data());
		}
		~_ScopedGpuTrace () {
			glPopDebugGroup();
		}
	};

#define OGL_DBG_LABEL(type, handle, label) gl::set_object_label(type, handle, label)
#define OGL_TRACE(name) \
		_ScopedGpuTrace __scoped_##__COUNTER__ (name); \
		TracyGpuZone(name)

#else
#define OGL_DBG_LABEL(type, handle, label)
#define OGL_TRACE(name) TracyGpuZone(name)
#endif
	
enum class GlslType {
	FLOAT,	FLOAT2,	FLOAT3,	FLOAT4,
	INT,	INT2,	INT3,	INT4,
	BOOL,

	MAT2, MAT3, MAT4,

	SAMPLER_1D,		SAMPLER_2D,		SAMPLER_3D,
	ISAMPLER_1D,	ISAMPLER_2D,	ISAMPLER_3D,
	USAMPLER_1D,	USAMPLER_2D,	USAMPLER_3D,
	SAMPLER_CUBE,

	SAMPLER_1D_ARRAY,	SAMPLER_2D_ARRAY,	SAMPLER_CUBE_ARRAY,
};

constexpr inline bool is_sampler_type (GlslType t) {
	return t >= GlslType::SAMPLER_1D && t <= GlslType::SAMPLER_CUBE_ARRAY;
}

const std::unordered_map<std::string_view, GlslType> glsl_type_map = {
	{ "float",				GlslType::FLOAT					},
	{ "vec2",				GlslType::FLOAT2				},
	{ "vec3",				GlslType::FLOAT3				},
	{ "vec4",				GlslType::FLOAT4				},
	{ "int",				GlslType::INT					},
	{ "ivec2",				GlslType::INT2					},
	{ "ivec3",				GlslType::INT3					},
	{ "ivec4",				GlslType::INT4					},
	{ "bool",				GlslType::BOOL					},
	{ "mat2",				GlslType::MAT2					},
	{ "mat3",				GlslType::MAT3					},
	{ "mat4",				GlslType::MAT4					},
	{ "sampler1D",			GlslType::SAMPLER_1D			},
	{ "sampler2D",			GlslType::SAMPLER_2D			},
	{ "sampler3D",			GlslType::SAMPLER_3D			},
	{ "isampler1D",			GlslType::ISAMPLER_1D			},
	{ "isampler2D",			GlslType::ISAMPLER_2D			},
	{ "isampler3D",			GlslType::ISAMPLER_3D			},
	{ "usampler1D",			GlslType::USAMPLER_1D			},
	{ "usampler2D",			GlslType::USAMPLER_2D			},
	{ "usampler3D",			GlslType::USAMPLER_3D			},
	{ "samplerCube",		GlslType::SAMPLER_CUBE			},
	{ "sampler1DArray",		GlslType::SAMPLER_1D_ARRAY		},
	{ "sampler2DArray",		GlslType::SAMPLER_2D_ARRAY		},
	{ "samplerCubeArray",	GlslType::SAMPLER_CUBE_ARRAY	},
};

struct ShaderUniform {
	GlslType	type;
	GLint		location;
};

inline void _set_uniform (ShaderUniform& u, float    val) { assert(u.type == GlslType::FLOAT ); glUniform1f( u.location, val); }
inline void _set_uniform (ShaderUniform& u, float2   val) { assert(u.type == GlslType::FLOAT2); glUniform2fv(u.location, 1, &val.x); }
inline void _set_uniform (ShaderUniform& u, float3   val) { assert(u.type == GlslType::FLOAT3); glUniform3fv(u.location, 1, &val.y); }
inline void _set_uniform (ShaderUniform& u, float4   val) { assert(u.type == GlslType::FLOAT4); glUniform4fv(u.location, 1, &val.z); }
inline void _set_uniform (ShaderUniform& u, int      val) { assert(u.type == GlslType::INT   ); glUniform1i( u.location, val); }
inline void _set_uniform (ShaderUniform& u, int2     val) { assert(u.type == GlslType::INT2  ); glUniform2iv(u.location, 1, &val.x); }
inline void _set_uniform (ShaderUniform& u, int3     val) { assert(u.type == GlslType::INT3  ); glUniform3iv(u.location, 1, &val.y); }
inline void _set_uniform (ShaderUniform& u, int4     val) { assert(u.type == GlslType::INT4  ); glUniform4iv(u.location, 1, &val.z); }
inline void _set_uniform (ShaderUniform& u, bool     val) { assert(u.type == GlslType::BOOL  ); glUniform1i(u.location, val ? GL_TRUE : GL_FALSE); }
inline void _set_uniform (ShaderUniform& u, float2x2 val) { assert(u.type == GlslType::MAT2  ); glUniformMatrix2fv(u.location, 1, GL_FALSE, &val.arr[0].x); }
inline void _set_uniform (ShaderUniform& u, float3x3 val) { assert(u.type == GlslType::MAT3  ); glUniformMatrix3fv(u.location, 1, GL_FALSE, &val.arr[0].x); }
inline void _set_uniform (ShaderUniform& u, float4x4 val) { assert(u.type == GlslType::MAT4  ); glUniformMatrix4fv(u.location, 1, GL_FALSE, &val.arr[0].x); }

typedef ordered_map<std::string, ShaderUniform> uniform_set;

inline bool get_shader_compile_log (GLuint shad, std::string* log) {
	GLsizei log_len;
	{
		GLint temp = 0;
		glGetShaderiv(shad, GL_INFO_LOG_LENGTH, &temp);
		log_len = (GLsizei)temp;
	}

	if (log_len <= 1) {
		return false; // no log available
	} else {
		// GL_INFO_LOG_LENGTH includes the null terminator, but it is not allowed to write the null terminator in std::string, so we have to allocate one additional char and then resize it away at the end

		log->resize(log_len);

		GLsizei written_len = 0;
		glGetShaderInfoLog(shad, log_len, &written_len, &(*log)[0]);
		assert(written_len == (log_len -1));

		log->resize(written_len);

		return true;
	}
}
inline bool get_program_link_log (GLuint prog, std::string* log) {
	GLsizei log_len;
	{
		GLint temp = 0;
		glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &temp);
		log_len = (GLsizei)temp;
	}

	if (log_len <= 1) {
		return false; // no log available
	} else {
		// GL_INFO_LOG_LENGTH includes the null terminator, but it is not allowed to write the null terminator in std::string, so we have to allocate one additional char and then resize it away at the end

		log->resize(log_len);

		GLsizei written_len = 0;
		glGetProgramInfoLog(prog, log_len, &written_len, &(*log)[0]);
		assert(written_len == (log_len -1));

		log->resize(written_len);

		return true;
	}
}

// VBO class to manage lifetime
class Vbo {
	GLuint vbo;
public:
	MOVE_ONLY_CLASS_MEMBER(Vbo, vbo)

	Vbo (std::string_view label) {
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		OGL_DBG_LABEL(GL_BUFFER, vbo, label);
	}
	~Vbo () {
		glDeleteBuffers(1, &vbo);
	}

	operator GLuint () const { return vbo; }
};

// EBO class to manage lifetime
class Ebo {
	GLuint ebo;
public:
	MOVE_ONLY_CLASS_MEMBER(Ebo, ebo)

	Ebo (std::string_view label) {
		glGenBuffers(1, &ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		OGL_DBG_LABEL(GL_BUFFER, ebo, label);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	~Ebo () {
		glDeleteBuffers(1, &ebo);
	}

	operator GLuint () const { return ebo; }
};

// UBO class to manage lifetime
class Ubo {
	GLuint ubo;
public:
	MOVE_ONLY_CLASS_MEMBER(Ubo, ubo)

	Ubo (std::string_view label) {
		glGenBuffers(1, &ubo);
		glBindBuffer(GL_UNIFORM_BUFFER, ubo);
		OGL_DBG_LABEL(GL_BUFFER, ubo, label);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
	~Ubo () {
		glDeleteBuffers(1, &ubo);
	}

	operator GLuint () const { return ubo; }
};

// VAO class to manage lifetime
class Vao {
	GLuint vao;
public:
	MOVE_ONLY_CLASS_MEMBER(Vao, vao)

	Vao (std::string_view label) {
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		OGL_DBG_LABEL(GL_VERTEX_ARRAY, vao, label);
		glBindVertexArray(0);
	}
	~Vao () {
		glDeleteVertexArrays(1, &vao);
	}

	operator GLuint () const { return vao; }
};

// Texture class to manage lifetime
class Texture {
	GLuint tex;
public:
	MOVE_ONLY_CLASS_MEMBER(Texture, tex)

	Texture (std::string_view label) {
		glGenTextures(1, &tex);
		OGL_DBG_LABEL(GL_TEXTURE, tex, label);
	}
	~Texture () {
		glDeleteTextures(1, &tex);
	}

	operator GLuint () const { return tex; }
};

// Sampler class to manage lifetime
class Sampler {
	GLuint sampler;
public:
	MOVE_ONLY_CLASS_MEMBER(Sampler, sampler)

	Sampler (std::string_view label) {
		glGenSamplers(1, &sampler);
		OGL_DBG_LABEL(GL_SAMPLER, sampler, label);
	}
	~Sampler () {
		glDeleteSamplers(1, &sampler);
	}

	operator GLuint () const { return sampler; }
};

inline void gl_enable (GLenum cap, bool on) {
	if (on)
		glEnable(cap);
	else
		glDisable(cap);
}

// use glsl_bool bool instead of bool in uniform blocks because glsl bools are 32 bit and the padding after a c++ bool might be unitialized -> bool has random value in shader
struct glsl_bool {
	uint32_t val;

	glsl_bool () {}
	constexpr glsl_bool (bool b): val{(uint32_t)b} {}
	constexpr operator bool () {
		return val != 0;
	}
};

/*
struct SharedUniformsLayoutChecker {
	int offset = 0;
	bool valid = true;

	static constexpr int N = 4;

	static constexpr int align (int offs, int alignment) {
		int mod = offs % alignment;
		return offs + (mod == 0 ? 0 : alignment - mod);
	}

	template <typename T>
	static constexpr int get_align ();

	template<> static constexpr int get_align<float    > () { return N; }
	template<> static constexpr int get_align<int      > () { return N; }
	template<> static constexpr int get_align<glsl_bool> () { return N; }
	template<> static constexpr int get_align<float2   > () { return 2*N; }
	template<> static constexpr int get_align<float3   > () { return 4*N; }
	template<> static constexpr int get_align<float4   > () { return 4*N; }
	template<> static constexpr int get_align<float4x4 > () { return 4*N; }

	template<typename T>
	constexpr void member (int offs) {
		offset = align(offset, get_align<T>());
		valid = valid && offset == offs;
		offset += sizeof(T);
	}

	template <typename T>
	constexpr bool is_valid () {
		return valid && sizeof(T) == offset;
	}
};
*/

template <typename T>
class UniformBuffer {
	Ubo ubo;

	/*
	static constexpr bool check_std140_layout () {
	SharedUniformsLayoutChecker c;
	T::check_layout(c);
	return c.is_valid<T>();
	}

	// NOTE:
	// error C2131: expression did not evaluate to a constant
	// note: failure was caused by call of undefined function or one not declared 'constexpr'
	// note: see usage of 'SharedUniformsLayoutChecker::get_align'
	// when a type is not registered via a get_align template
	static_assert(SharedUniforms<T>::check_std140_layout(), "SharedUniforms: layout of T is not how std140 expects it!");
	*/
public:
	//operator GLuint () const { return ubo; }
	
	UniformBuffer (char const* name, int binding_point): ubo{prints("%s_ubo", name)} {
		glBindBuffer(GL_UNIFORM_BUFFER, ubo);

		glBufferData(GL_UNIFORM_BUFFER, sizeof(T), NULL, GL_STATIC_DRAW);

		glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, ubo);

		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void set (T const& val) {
		glBindBuffer(GL_UNIFORM_BUFFER, ubo);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(T), &val, GL_STATIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
};

} // namespace gl
