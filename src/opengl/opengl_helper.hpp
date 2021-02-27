#pragma once
#include "common.hpp"
#include "glad/glad.h"
#include "tracyOpenGL.hpp"
#include "kisslib/stb_image_write.hpp"

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

// NOTE: objects need to be bound to something (eg. glBindBuffer) before I can call glObjectLabel on them or the call will fail

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
class Ssbo {
	GLuint ssbo = 0;
public:
	MOVE_ONLY_CLASS_MEMBER(Ssbo, ssbo);

	Ssbo () {} // not allocated
	Ssbo (std::string_view label) { // allocate
		glGenBuffers(1, &ssbo);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
		OGL_DBG_LABEL(GL_BUFFER, ssbo, label);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}
	~Ssbo () {
		if (ssbo) glDeleteBuffers(1, &ssbo);
	}

	operator GLuint () const { return ssbo; }
};
class Sampler {
	GLuint sampler = 0;
public:
	MOVE_ONLY_CLASS_MEMBER(Sampler, sampler);

	Sampler () {} // not allocated
	Sampler (std::string_view label) { // allocate
		glGenSamplers(1, &sampler);
		glBindSampler(0, sampler);
		OGL_DBG_LABEL(GL_SAMPLER, sampler, label);
	}
	~Sampler () {
		if (sampler) glDeleteSamplers(1, &sampler);
	}

	operator GLuint () const { return sampler; }
};
class Texture2D {
	GLuint tex = 0;
public:
	MOVE_ONLY_CLASS_MEMBER(Texture2D, tex);

	Texture2D () {} // not allocated
	Texture2D (std::string_view label) { // allocate
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		OGL_DBG_LABEL(GL_TEXTURE, tex, label);
	}
	~Texture2D () {
		if (tex) glDeleteTextures(1, &tex);
	}

	operator GLuint () const { return tex; }
};
class Texture2DArray {
	GLuint tex = 0;
public:
	MOVE_ONLY_CLASS_MEMBER(Texture2DArray, tex);

	Texture2DArray () {} // not allocated
	Texture2DArray (std::string_view label) { // allocate
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
		OGL_DBG_LABEL(GL_TEXTURE, tex, label);
	}
	~Texture2DArray () {
		if (tex) glDeleteTextures(1, &tex);
	}

	operator GLuint () const { return tex; }
};

//// Shader & Shader uniform stuff

enum class GlslType {
	FLOAT,	FLOAT2,	FLOAT3,	FLOAT4,
	INT,	INT2,	INT3,	INT4,
	UINT,	UINT2,	UINT3,	UINT4,
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
	{ "uint",				GlslType::UINT					},
	{ "uvec2",				GlslType::UINT2					},
	{ "uvec3",				GlslType::UINT3					},
	{ "uvec4",				GlslType::UINT4					},
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
	std::string	name;
	GlslType	type;
	GLint		location;
};

inline void _set_uniform (ShaderUniform& u, float    val) { assert(u.type == GlslType::FLOAT ); glUniform1f( u.location, val); }
inline void _set_uniform (ShaderUniform& u, float2   val) { assert(u.type == GlslType::FLOAT2); glUniform2fv(u.location, 1, &val.x); }
inline void _set_uniform (ShaderUniform& u, float3   val) { assert(u.type == GlslType::FLOAT3); glUniform3fv(u.location, 1, &val.x); }
inline void _set_uniform (ShaderUniform& u, float4   val) { assert(u.type == GlslType::FLOAT4); glUniform4fv(u.location, 1, &val.x); }
inline void _set_uniform (ShaderUniform& u, int      val) { assert(u.type == GlslType::INT   ); glUniform1i( u.location, val); }
inline void _set_uniform (ShaderUniform& u, uint32_t val) { assert(u.type == GlslType::UINT  ); glUniform1ui(u.location, val); }
inline void _set_uniform (ShaderUniform& u, int2     val) { assert(u.type == GlslType::INT2  ); glUniform2iv(u.location, 1, &val.x); }
inline void _set_uniform (ShaderUniform& u, int3     val) { assert(u.type == GlslType::INT3  ); glUniform3iv(u.location, 1, &val.x); }
inline void _set_uniform (ShaderUniform& u, int4     val) { assert(u.type == GlslType::INT4  ); glUniform4iv(u.location, 1, &val.x); }
inline void _set_uniform (ShaderUniform& u, bool     val) { assert(u.type == GlslType::BOOL  ); glUniform1i( u.location, val ? GL_TRUE : GL_FALSE); }
inline void _set_uniform (ShaderUniform& u, float2x2 val) { assert(u.type == GlslType::MAT2  ); glUniformMatrix2fv(u.location, 1, GL_FALSE, &val.arr[0].x); }
inline void _set_uniform (ShaderUniform& u, float3x3 val) { assert(u.type == GlslType::MAT3  ); glUniformMatrix3fv(u.location, 1, GL_FALSE, &val.arr[0].x); }
inline void _set_uniform (ShaderUniform& u, float4x4 val) { assert(u.type == GlslType::MAT4  ); glUniformMatrix4fv(u.location, 1, GL_FALSE, &val.arr[0].x); }

//typedef ordered_map<std::string, ShaderUniform> uniform_set;
typedef std::vector<ShaderUniform> uniform_set;

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

inline void upload_ubo (GLuint ubo, void* data, size_t size) {
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_STREAM_DRAW);
	glBufferData(GL_UNIFORM_BUFFER, size, data, GL_STREAM_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
inline void bind_ubo (GLuint ubo, int binding_point) {
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);
	glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}
inline void upload_bind_ubo (GLuint ubo, int binding_point, void* data, size_t size) {
	glBindBuffer(GL_UNIFORM_BUFFER, ubo);

	glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_STREAM_DRAW);
	glBufferData(GL_UNIFORM_BUFFER, size, data, GL_STREAM_DRAW);

	glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, ubo);

	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

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

		if (instanced)
			glVertexAttribDivisor((GLuint)location, 1); // vertex attribute is per-instance
	}

	template <AttribMode M, typename T, int N>
	void addv (int location, char const* name, size_t offset) {
		addv<M, kissmath::get_type<T>().type, N>(location, name, offset);
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

	glBindVertexArray(0); // unbind vao before unbinding EBO or GL_ELEMENT_ARRAY_BUFFER will be unbound from VAO

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if (indices_buf)
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	return vao;
}

struct VertexBuffer {
	Vao vao;
	Vbo vbo;
};
template <typename T>
inline VertexBuffer vertex_buffer (std::string_view label) {
	VertexBuffer v;
	v.vbo = Vbo(label);
	v.vao = setup_vao<T>(label, v.vbo);
	return v;
}

struct InstancedBuffer {
	Vao vao;
	Vbo mesh_vbo;
	Vbo instance_vbo;
};
template <typename MeshVertex, typename InstanceVertVertex>
inline InstancedBuffer instanced_buffer (std::string_view label) {
	InstancedBuffer v;
	v.mesh_vbo = Vbo(label);
	v.instance_vbo = Vbo(label);
	
	v.vao = { label };

	glBindVertexArray(v.vao);

	glBindBuffer(GL_ARRAY_BUFFER, v.mesh_vbo);
	{
		VertexAttributes a;
		MeshVertex::attributes(a);
	}
	glBindBuffer(GL_ARRAY_BUFFER, v.instance_vbo);
	{
		VertexAttributes a;
		InstanceVertVertex::attributes(a);
	}
	glBindVertexArray(0); // unbind vao before unbinding EBO or GL_ELEMENT_ARRAY_BUFFER will be unbound from VAO

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return v;
}

struct IndexedInstancedBuffer {
	Vao vao;
	Vbo mesh_vbo;
	Ebo mesh_ebo;
	Vbo instance_vbo;
};
template <typename MeshVertex, typename InstanceVertVertex>
inline IndexedInstancedBuffer indexed_instanced_buffer (std::string_view label) {
	IndexedInstancedBuffer v;
	v.mesh_vbo = Vbo(label);
	v.mesh_ebo = Ebo(label);
	v.instance_vbo = Vbo(label);

	v.vao = { label };

	glBindVertexArray(v.vao);

	glBindBuffer(GL_ARRAY_BUFFER, v.mesh_vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, v.mesh_ebo);
	{
		VertexAttributes a;
		MeshVertex::attributes(a);
	}
	glBindBuffer(GL_ARRAY_BUFFER, v.instance_vbo);
	{
		VertexAttributes a;
		InstanceVertVertex::attributes(a);
	}
	glBindVertexArray(0); // unbind vao before unbinding EBO or GL_ELEMENT_ARRAY_BUFFER will be unbound from VAO

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	return v;
}

struct Mesh {
	Vao vao;
	Vbo vbo;
	int vertex_count;
};

template <typename T>
inline Mesh upload_mesh (std::string_view label, T* vertices, size_t vertex_count) {
	Mesh m;
	m.vbo = Vbo(label);
	m.vao = setup_vao<T>(label, m.vbo);
	m.vertex_count = (int)vertex_count;

	glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(T), vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return m;
}

struct IndexedMesh {
	Vao vao;
	Vbo vbo;
	Ebo ebo;
	uint32_t vertex_count;
	uint32_t index_count;
};

template <typename T>
inline IndexedMesh upload_mesh (std::string_view label,
		T* vertices, size_t vertex_count, uint16_t* indices, size_t index_count) {
	IndexedMesh m;
	m.vbo = Vbo(label);
	m.ebo = Ebo(label);
	m.vao = setup_vao<T>(label, m.vbo, m.ebo);
	m.vertex_count = (uint32_t)vertex_count;
	m.index_count = (uint32_t)index_count;

	glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(T), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(uint16_t), indices, GL_STATIC_DRAW);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	return m;
}

//// Opengl global state management

enum DepthFunc {
	DEPTH_INFRONT, // normal: draw infront (or equal depth) of other things
	DEPTH_BEHIND, // inverted: draw behind other things
};
inline GLenum map_depth_func (DepthFunc func) {
	switch (func) { // all inverted because reverse depth
		case DEPTH_INFRONT:	return GL_GEQUAL;
		case DEPTH_BEHIND:	return GL_LESS;
		default: return 0;
	}
}

enum CullFace {
	CULL_BACK,
	CULL_FRONT,
};
enum PrimitiveMode {
	PRIM_TRIANGELS=0,
	PRIM_LINES,
};
enum PolyMode {
	POLY_FILL=0,
	POLY_LINE,
};

struct BlendFunc {
	GLenum equation = GL_FUNC_ADD;
	GLenum sfactor = GL_SRC_ALPHA;
	GLenum dfactor = GL_ONE_MINUS_SRC_ALPHA;
};

struct PipelineState {
	bool depth_test = true;
	bool depth_write = true;
	DepthFunc depth_func = DEPTH_INFRONT;

	bool scissor_test = false;

	bool culling = true;
	CullFace cull_face = CULL_BACK;

	bool blend_enable = false;
	BlendFunc blend_func = BlendFunc();

	PolyMode poly_mode = POLY_FILL;
};

struct StateManager {
	PipelineState state;

	// overrides
	bool override_poly = false;
	bool override_cull = false;
	PipelineState override_state;

	StateManager () {
		//set_default();
	}

	PipelineState _override (PipelineState const& s) {
		PipelineState o = s;

		if (override_poly)
			o.poly_mode = override_state.poly_mode;
		if (override_cull) {
			o.culling = override_state.culling;
			o.cull_face = override_state.cull_face;
		}

		return o;
	}

	void set_default () {
		state = PipelineState();

		auto o = _override(state);

		gl_enable(GL_DEPTH_TEST, o.depth_test);
		// use_reverse_depth
		glDepthFunc(map_depth_func(o.depth_func));
		glClearDepth(0.0f);
		glDepthRange(0.0f, 1.0f);
		glDepthMask(o.depth_write ? GL_TRUE : GL_FALSE);

		gl_enable(GL_SCISSOR_TEST, o.scissor_test);

		// culling
		gl_enable(GL_CULL_FACE, o.culling);
		glCullFace(o.cull_face == CULL_FRONT ? GL_FRONT : GL_BACK);
		glFrontFace(GL_CCW);
		// blending
		gl_enable(GL_BLEND, o.blend_enable);
		glBlendEquation(o.blend_func.equation);
		glBlendFunc(o.blend_func.sfactor, state.blend_func.dfactor);

		glPolygonMode(GL_FRONT_AND_BACK, o.poly_mode == POLY_FILL ? GL_FILL : GL_LINE);
	}

	void set (PipelineState const& s) {
		auto o = _override(s);

		if (state.depth_test != o.depth_test)
			gl_enable(GL_DEPTH_TEST, o.depth_test);
		if (state.depth_func != o.depth_func)
			glDepthFunc(map_depth_func(o.depth_func));
		if (state.depth_write != o.depth_write)
			glDepthMask(o.depth_write ? GL_TRUE : GL_FALSE);

		if (state.scissor_test != o.scissor_test)
			gl_enable(GL_SCISSOR_TEST, o.scissor_test);

		if (state.culling != o.culling)
			gl_enable(GL_CULL_FACE, o.culling);
		if (state.culling != o.culling)
			glCullFace(o.culling == CULL_FRONT ? GL_FRONT : GL_BACK);

		// blending
		if (state.blend_enable != o.blend_enable)
			gl_enable(GL_BLEND, o.blend_enable);
		if (state.blend_func.equation != o.blend_func.equation)
			glBlendEquation(o.blend_func.equation);
		if (state.blend_func.sfactor != o.blend_func.sfactor || state.blend_func.dfactor != o.blend_func.dfactor)
			glBlendFunc(o.blend_func.sfactor, o.blend_func.dfactor);

		if (state.poly_mode != o.poly_mode)
			glPolygonMode(GL_FRONT_AND_BACK, o.poly_mode == POLY_FILL ? GL_FILL : GL_LINE);

		state = o;
	}
};

////

// framebuffer for rendering at different resolution and to make sure we get float buffers
struct Framebuffer {
	// https://nlguillemot.wordpress.com/2016/12/07/reversed-z-in-opengl/

	GLuint color	= 0;
	GLuint depth	= 0;
	GLuint fbo		= 0;

	int2 size = 0;
	float renderscale = 1.0f;

	bool nearest = false;

	void imgui () {
		if (imgui_push("Renderscale")) {
			ImGui::Text("res: %4d x %4d px (%5.2f Mpx)", size.x, size.y, (float)(size.x * size.y) / (1000*1000));
			ImGui::SliderFloat("renderscale", &renderscale, 0.02f, 2.0f);

			ImGui::Checkbox("nearest", &nearest);

			imgui_pop();
		}
	}

	static constexpr auto color_format = GL_RGBA16F;// GL_RGBA32F   GL_SRGB8_ALPHA8
	static constexpr auto depth_format = GL_DEPTH_COMPONENT32F;

	void update (int2 window_size) {
		auto old_size = size;
		size = max(1, roundi((float2)window_size * renderscale));

		if (old_size != size) {
			// delete old
			glDeleteTextures(1, &color);
			glDeleteTextures(1, &depth);
			glDeleteFramebuffers(1, &fbo);

			// create new (textures created with glTexStorage2D cannot be resized)
			glGenTextures(1, &color);
			glBindTexture(GL_TEXTURE_2D, color);
			glTexStorage2D(GL_TEXTURE_2D, 1, color_format, size.x, size.y);
			glBindTexture(GL_TEXTURE_2D, 0);

			glGenTextures(1, &depth);
			glBindTexture(GL_TEXTURE_2D, depth);
			glTexStorage2D(GL_TEXTURE_2D, 1, depth_format, size.x, size.y);
			glBindTexture(GL_TEXTURE_2D, 0);

			glGenFramebuffers(1, &fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);

			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE) {
				fprintf(stderr, "glCheckFramebufferStatus: %x\n", status);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
	}

	void blit (int2 window_size) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // default FBO

		// TODO: using blit here, but if a post processing pass is desired the rescaling (sampling) can also just be done in our post shader
		glBlitFramebuffer(
			0, 0, size.x, size.y,
			0, 0, window_size.x, window_size.y,
			GL_COLOR_BUFFER_BIT, nearest ? GL_NEAREST : GL_LINEAR);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
};

// take screenshot of current bound framebuffer
inline void take_screenshot (int2 size) {
	Image<srgb8> img (size);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0,0, size.x, size.y, GL_RGB, GL_UNSIGNED_BYTE, img.pixels);

	time_t t = time(0); // get time now
	struct tm* now = localtime(&t);

	char timestr [80];
	strftime(timestr, 80, "%g%m%d-%H%M%S", now); // yy-mm-dd_hh-mm-ss

	static int counter = 0; // counter to avoid overwriting files in edge cases
	auto filename = prints("../screenshots/screen_%s_%d.jpg", timestr, counter++);
	counter %= 100;

	stbi_flip_vertically_on_write(true);
	stbi_write_jpg(filename.c_str(), size.x, size.y, 3, img.pixels, 95);
}

} // namespace gl
