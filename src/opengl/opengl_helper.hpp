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

//// RAII Wrappers for managing lifetime

class Vbo {
	GLuint vbo = 0;
public:
	MOVE_ONLY_CLASS_MEMBER(Vbo, vbo);

	Vbo () {} // not allocated
	Vbo (std::string_view label) { // allocate
		glGenBuffers(1, &vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		OGL_DBG_LABEL(GL_BUFFER, vbo, label);
	}
	~Vbo () {
		if (vbo)
			glDeleteBuffers(1, &vbo);
	}

	operator GLuint () const { return vbo; }
};
class Ebo {
	GLuint ebo = 0;
public:
	MOVE_ONLY_CLASS_MEMBER(Ebo, ebo);

	Ebo () {} // not allocated
	Ebo (std::string_view label) { // allocate
		glGenBuffers(1, &ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		OGL_DBG_LABEL(GL_BUFFER, ebo, label);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	~Ebo () {
		if (ebo) glDeleteBuffers(1, &ebo);
	}

	operator GLuint () const { return ebo; }
};
class Ubo {
	GLuint ubo = 0;
public:
	MOVE_ONLY_CLASS_MEMBER(Ubo, ubo);

	Ubo () {} // not allocated
	Ubo (std::string_view label) { // allocate
		glGenBuffers(1, &ubo);
		glBindBuffer(GL_UNIFORM_BUFFER, ubo);
		OGL_DBG_LABEL(GL_BUFFER, ubo, label);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
	~Ubo () {
		if (ubo) glDeleteBuffers(1, &ubo);
	}

	operator GLuint () const { return ubo; }
};
class Vao {
	GLuint vao = 0;
public:
	MOVE_ONLY_CLASS_MEMBER(Vao, vao);

	Vao () {} // not allocated
	Vao (std::string_view label) { // allocate
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		OGL_DBG_LABEL(GL_VERTEX_ARRAY, vao, label);
		glBindVertexArray(0);
	}
	~Vao () {
		if (vao) glDeleteVertexArrays(1, &vao);
	}

	operator GLuint () const { return vao; }
};
class Texture {
	GLuint tex = 0;
public:
	MOVE_ONLY_CLASS_MEMBER(Texture, tex);

	Texture () {} // not allocated
	Texture (std::string_view label) { // allocate
		glGenTextures(1, &tex);
		OGL_DBG_LABEL(GL_TEXTURE, tex, label);
	}
	~Texture () {
		if (tex) glDeleteTextures(1, &tex);
	}

	operator GLuint () const { return tex; }
};
class Sampler {
	GLuint sampler = 0;
public:
	MOVE_ONLY_CLASS_MEMBER(Sampler, sampler);

	Sampler () {} // not allocated
	Sampler (std::string_view label) { // allocate
		glGenSamplers(1, &sampler);
		OGL_DBG_LABEL(GL_SAMPLER, sampler, label);
	}
	~Sampler () {
		if (sampler) glDeleteSamplers(1, &sampler);
	}

	operator GLuint () const { return sampler; }
};

//// Shader & Shader uniform stuff

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
inline void _set_uniform (ShaderUniform& u, bool     val) { assert(u.type == GlslType::BOOL  ); glUniform1i( u.location, val ? GL_TRUE : GL_FALSE); }
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

//// Uniform Buffer objects

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

//// Vertex buffers

inline constexpr GLenum _get_gltype (ScalarType type) {
	switch (type) {
		case ScalarType::FLOAT:		return GL_FLOAT;
		case ScalarType::INT:		return GL_INT;
		case ScalarType::UINT8:		return GL_UNSIGNED_BYTE;
		case ScalarType::UINT16:	return GL_UNSIGNED_SHORT;
		case ScalarType::INT16:		return GL_SHORT;
		default: return 0;
	}
}

/* Use like:
	struct LineVertex {
		float3 pos;
		float4 col;

		template <typename ATTRIBS>
		static void attributes (ATTRIBS& a) {
			int loc = 0;
			a.init(sizeof(LineVertex));
			a.template add<AttribMode::FLOAT, decltype(pos)>(loc++, "pos", offsetof(LineVertex, pos));
			a.template add<AttribMode::FLOAT, decltype(col)>(loc++, "col", offsetof(LineVertex, col));
		}
	};
*/
struct VertexAttributes {
	GLsizei stride;
	bool instanced;

	void init (size_t vertex_size, bool instanced=false) {
		stride = (GLsizei)vertex_size;
		this->instanced = instanced;
	}

	template <AttribMode M, ScalarType T, int N>
	void addv (int location, char const* name, size_t offset) {
		glEnableVertexAttribArray((GLuint)location);

		bool AttribI;
		bool normalized = false;
		switch (M) {
			case AttribMode::FLOAT:		// simply pass float to shader
			case AttribMode::SINT2FLT:	// convert sint to float 
			case AttribMode::UINT2FLT:	// convert uint to float
				AttribI = false;
				break;

			case AttribMode::SINT:		// simply pass sint to shader
			case AttribMode::UINT:		// simply pass uint to shader
				AttribI = true;
				break;

			case AttribMode::SNORM:		// sint turns into [-1, 1] float    // TODO: this is how it was in vulkan, same in opengl?
			case AttribMode::UNORM:		// uint turns into [0, 1] float     // TODO: this is how it was in vulkan, same in opengl?
				AttribI = true;
				normalized = true;
				break;
		}

		GLenum type = _get_gltype(T);

		if (AttribI)
			glVertexAttribIPointer((GLuint)location, N, type, stride, (void*)offset);
		else
			glVertexAttribPointer((GLuint)location, N, type, normalized, stride, (void*)offset);
	}

	template <AttribMode M, typename T>
	void add (int location, char const* name, size_t offset) {
		addv<M, kissmath::get_type<T>().type, kissmath::get_type<T>().components>(location, name, offset);
	}
};

template <typename T> Vao setup_vao (std::string_view label, GLuint vertex_buf, GLuint indices_buf=0) {
	Vao vao = { label };
	glBindVertexArray(vao);

	glBindBuffer(GL_ARRAY_BUFFER, vertex_buf);
	if (indices_buf)
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_buf);

	VertexAttributes a;
	T::attributes(a);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if (indices_buf)
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	return vao;
}

//// Opengl global state management

enum CullFace {
	CULL_BACK,
	CULL_FRONT,
};
enum PrimitiveMode {
	PRIM_TRIANGELS=0,
	PRIM_LINES,
};

struct BlendFunc {
	GLenum equation = GL_FUNC_ADD;
	GLenum sfactor = GL_SRC_ALPHA;
	GLenum dfactor = GL_ONE_MINUS_SRC_ALPHA;
};

struct PipelineState {
	bool depth_test = true;
	bool depth_write = true;
	GLenum depth_func = GL_GEQUAL;

	bool scissor_test = false;

	bool culling = true;
	CullFace cull_face = CULL_BACK;

	bool blend_enable = false;
	BlendFunc blend_func = BlendFunc();
};

struct StateManager {
	PipelineState state;

	StateManager () {
		//set_default();
	}

	void set_default () {
		state = PipelineState();

		gl_enable(GL_DEPTH_TEST, state.depth_test);
		// use_reverse_depth
		glDepthFunc(GL_GEQUAL);
		glClearDepth(0.0f);
		glDepthRange(0.0f, 1.0f);
		glDepthMask(state.depth_write ? GL_TRUE : GL_FALSE);

		gl_enable(GL_SCISSOR_TEST, state.scissor_test);

		// culling
		gl_enable(GL_CULL_FACE, state.culling);
		glCullFace(state.cull_face == CULL_FRONT ? GL_FRONT : GL_BACK);
		glFrontFace(GL_CCW);
		// blending
		gl_enable(GL_BLEND, state.blend_enable);
		glBlendEquation(state.blend_func.equation);
		glBlendFunc(state.blend_func.sfactor, state.blend_func.dfactor);
	}

	void set (PipelineState const& s) {
		if (state.depth_test != s.depth_test)
			gl_enable(GL_DEPTH_TEST, s.depth_test);
		if (state.depth_func != s.depth_func)
			glDepthFunc(s.depth_func);
		if (state.depth_write != s.depth_write)
			glDepthMask(s.depth_write ? GL_TRUE : GL_FALSE);

		if (state.scissor_test != s.scissor_test)
			gl_enable(GL_SCISSOR_TEST, s.scissor_test);

		if (state.culling != s.culling)
			gl_enable(GL_CULL_FACE, s.culling);
		if (state.culling != s.culling)
			glCullFace(s.culling == CULL_FRONT ? GL_FRONT : GL_BACK);

		// blending
		if (state.blend_enable != s.blend_enable)
			gl_enable(GL_BLEND, state.blend_enable);
		if (state.blend_func.equation != s.blend_func.equation)
			glBlendEquation(s.blend_func.equation);
		if (state.blend_func.sfactor != s.blend_func.sfactor || state.blend_func.dfactor != s.blend_func.dfactor)
			glBlendFunc(s.blend_func.sfactor, s.blend_func.dfactor);

		state = s;
	}
};

} // namespace gl
