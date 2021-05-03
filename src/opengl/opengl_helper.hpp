#pragma once
#include "common.hpp"
#include "glad/glad.h"
#include "tracyOpenGL.hpp"
#include "kisslib/stb_image_write.hpp"
#include "shader_preprocessor.hpp"

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

#define GL_PRINT_INT(pname) do { \
		int i; \
		glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &i); \
		printf(#pname ": %d\n", i); \
	} while (0)

// GL_PRINT_INT(GL_MAX_TEXTURE_BUFFER_SIZE);
// GL_PRINT_INT(GL_MAX_TEXTURE_SIZE);
// GL_PRINT_INT(GL_MAX_3D_TEXTURE_SIZE);
// GL_PRINT_INT(GL_MAX_RECTANGLE_TEXTURE_SIZE);

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
class Texture1D {
	GLuint tex = 0;
public:
	MOVE_ONLY_CLASS_MEMBER(Texture1D, tex);

	Texture1D () {} // not allocated
	Texture1D (std::string_view label) { // allocate
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_1D, tex);
		OGL_DBG_LABEL(GL_TEXTURE, tex, label);
	}
	~Texture1D () {
		if (tex) glDeleteTextures(1, &tex);
	}

	operator GLuint () const { return tex; }
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
class Texture3D {
	GLuint tex = 0;
public:
	MOVE_ONLY_CLASS_MEMBER(Texture3D, tex);

	Texture3D () {} // not allocated
	Texture3D (std::string_view label) { // allocate
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_3D, tex);
		OGL_DBG_LABEL(GL_TEXTURE, tex, label);
	}
	~Texture3D () {
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

struct ShaderUniform {
	std::string	name;
	GLenum		type;
	GLint		location;
};

inline void _set_uniform (ShaderUniform& u, float    const* values, int arr_count) { assert(u.type == GL_FLOAT        ); glUniform1fv(u.location, arr_count, values); }
inline void _set_uniform (ShaderUniform& u, float2   const* values, int arr_count) { assert(u.type == GL_FLOAT_VEC2   ); glUniform2fv(u.location, arr_count, &values->x); }
inline void _set_uniform (ShaderUniform& u, float3   const* values, int arr_count) { assert(u.type == GL_FLOAT_VEC3   ); glUniform3fv(u.location, arr_count, &values->x); }
inline void _set_uniform (ShaderUniform& u, float4   const* values, int arr_count) { assert(u.type == GL_FLOAT_VEC4   ); glUniform4fv(u.location, arr_count, &values->x); }
inline void _set_uniform (ShaderUniform& u, int      const* values, int arr_count) { assert(u.type == GL_INT          ); glUniform1iv(u.location, arr_count, values); }
inline void _set_uniform (ShaderUniform& u, uint32_t const* values, int arr_count) { assert(u.type == GL_UNSIGNED_INT ); glUniform1uiv(u.location, arr_count, values); }
inline void _set_uniform (ShaderUniform& u, int2     const* values, int arr_count) { assert(u.type == GL_INT_VEC2     ); glUniform2iv(u.location, arr_count, &values->x); }
inline void _set_uniform (ShaderUniform& u, int3     const* values, int arr_count) { assert(u.type == GL_INT_VEC3     ); glUniform3iv(u.location, arr_count, &values->x); }
inline void _set_uniform (ShaderUniform& u, int4     const* values, int arr_count) { assert(u.type == GL_INT_VEC4     ); glUniform4iv(u.location, arr_count, &values->x); }
inline void _set_uniform (ShaderUniform& u, float2x2 const* values, int arr_count) { assert(u.type == GL_FLOAT_MAT2   ); glUniformMatrix2fv(u.location, arr_count, GL_FALSE, &values->arr[0].x); }
inline void _set_uniform (ShaderUniform& u, float3x3 const* values, int arr_count) { assert(u.type == GL_FLOAT_MAT3   ); glUniformMatrix3fv(u.location, arr_count, GL_FALSE, &values->arr[0].x); }
inline void _set_uniform (ShaderUniform& u, float4x4 const* values, int arr_count) { assert(u.type == GL_FLOAT_MAT4   ); glUniformMatrix4fv(u.location, arr_count, GL_FALSE, &values->arr[0].x); }

inline void _set_uniform (ShaderUniform& u, bool     const& val) { assert(u.type == GL_BOOL         ); glUniform1i(u.location, val ? GL_TRUE : GL_FALSE); }
inline void _set_uniform (ShaderUniform& u, float    const& val) { assert(u.type == GL_FLOAT        ); glUniform1f(u.location, val); }
inline void _set_uniform (ShaderUniform& u, float2   const& val) { assert(u.type == GL_FLOAT_VEC2   ); glUniform2fv(u.location, 1, &val.x); }
inline void _set_uniform (ShaderUniform& u, float3   const& val) { assert(u.type == GL_FLOAT_VEC3   ); glUniform3fv(u.location, 1, &val.x); }
inline void _set_uniform (ShaderUniform& u, float4   const& val) { assert(u.type == GL_FLOAT_VEC4   ); glUniform4fv(u.location, 1, &val.x); }
inline void _set_uniform (ShaderUniform& u, int      const& val) { assert(u.type == GL_INT          ); glUniform1i(u.location, val); }
inline void _set_uniform (ShaderUniform& u, uint32_t const& val) { assert(u.type == GL_UNSIGNED_INT ); glUniform1ui(u.location, val); }
inline void _set_uniform (ShaderUniform& u, int2     const& val) { assert(u.type == GL_INT_VEC2     ); glUniform2iv(u.location, 1, &val.x); }
inline void _set_uniform (ShaderUniform& u, int3     const& val) { assert(u.type == GL_INT_VEC3     ); glUniform3iv(u.location, 1, &val.x); }
inline void _set_uniform (ShaderUniform& u, int4     const& val) { assert(u.type == GL_INT_VEC4     ); glUniform4iv(u.location, 1, &val.x); }
inline void _set_uniform (ShaderUniform& u, float2x2 const& val) { assert(u.type == GL_FLOAT_MAT2   ); glUniformMatrix2fv(u.location, 1, GL_FALSE, &val.arr[0].x); }
inline void _set_uniform (ShaderUniform& u, float3x3 const& val) { assert(u.type == GL_FLOAT_MAT3   ); glUniformMatrix3fv(u.location, 1, GL_FALSE, &val.arr[0].x); }
inline void _set_uniform (ShaderUniform& u, float4x4 const& val) { assert(u.type == GL_FLOAT_MAT4   ); glUniformMatrix4fv(u.location, 1, GL_FALSE, &val.arr[0].x); }

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
	GLsizei	stride;
	bool	instanced;
	size_t	buffer_offset = 0;

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
			glVertexAttribIPointer((GLuint)location, N, type, stride, (void*)(offset + buffer_offset));
		else
			glVertexAttribPointer((GLuint)location, N, type, normalized, stride, (void*)(offset + buffer_offset));

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

struct BufferLocation {
	GLuint buf, offset;
	BufferLocation (GLuint buf, GLuint offset=0): buf{buf}, offset{offset} {}
	BufferLocation (Vbo const& buf, GLuint offset=0): buf{buf}, offset{offset} {}
};

// setup one VAO with a single associated VBO (and optionally EBO)
template <typename MESH_VERTEX> Vao setup_vao (std::string_view label, BufferLocation vbo, GLuint ebo=0) {
	Vao vao = { label };
	glBindVertexArray(vao);

	glBindBuffer(GL_ARRAY_BUFFER, vbo.buf);
	if (ebo) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

	VertexAttributes a;
	a.buffer_offset = vbo.offset;
	MESH_VERTEX::attributes(a);

	glBindVertexArray(0); // unbind vao before unbinding EBO or GL_ELEMENT_ARRAY_BUFFER will be unbound from VAO

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if (ebo) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	return vao;
}

template <typename MESH_VERTEX, typename INSTANCE_VERTEX> Vao setup_instanced_vao (std::string_view label,
		BufferLocation instance_vbo, BufferLocation mesh_vbo, GLuint mesh_ebo=0) {
	Vao vao = { label };
	glBindVertexArray(vao);

	glBindBuffer(GL_ARRAY_BUFFER, mesh_vbo.buf);
	if (mesh_ebo) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_ebo);
	{
		VertexAttributes a;
		a.buffer_offset = mesh_vbo.offset;
		MESH_VERTEX::attributes(a);
	}
	glBindBuffer(GL_ARRAY_BUFFER, instance_vbo.buf);
	{
		VertexAttributes a;
		a.buffer_offset = instance_vbo.offset;
		INSTANCE_VERTEX::attributes(a);
	}
	glBindVertexArray(0); // unbind vao before unbinding EBO else GL_ELEMENT_ARRAY_BUFFER will be unbound from VAO

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	if (mesh_ebo) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	return vao;
}

// VBO (with single associated VAO) and no additional info (like vertex count)
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

// VBO+EBO (with single associated VAO) and no additional info (like vertex count)
struct IndexedBuffer {
	Vao vao;
	Vbo vbo;
	Ebo ebo;
};
template <typename T>
inline IndexedBuffer indexed_buffer (std::string_view label) {
	IndexedBuffer ib;
	ib.vbo = Vbo(label);
	ib.ebo = Ebo(label);
	ib.vao = setup_vao<T>(label, ib.vbo, ib.ebo);
	return ib;
}

// instance VBO + mesh VBO (with single associated VAO) and no additional info (like vertex count)
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
	v.vao = setup_instanced_vao<MeshVertex, InstanceVertVertex>(label, v.instance_vbo, v.mesh_vbo);
	return v;
}

// instance VBO + mesh VBO&EBO (with single associated VAO) and no additional info (like vertex count)
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
	v.vao = setup_instanced_vao<MeshVertex, InstanceVertVertex>(label, v.instance_vbo, v.mesh_vbo, v.mesh_ebo);
	return v;
}

// fill an entire VBO with new data, ie the basic way to upload vertex data
template <typename T> inline void reupload_vbo (GLuint vbo, T const* vertices, size_t vertex_count, GLenum usage) {
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glBufferData(GL_ARRAY_BUFFER, vertex_count*sizeof(T), nullptr, usage);
	if (vertex_count > 0)
		glBufferData(GL_ARRAY_BUFFER, vertex_count*sizeof(T), vertices, usage);

	// keep bound
}
// fill an entire EBO with new data, ie the basic way to upload index data
template <typename IT> inline void reupload_ebo (GLuint vbo, IT const* indices, size_t index_count, GLenum usage) {
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);

	glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count*sizeof(IT), nullptr, usage);
	if (index_count > 0)
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count*sizeof(IT), indices, usage);

	// keep bound
}

template <typename T> inline void upload_buffer (VertexBuffer& vb, std::vector<T> const& vertices) {
	reupload_vbo(vb.vbo, vertices.data(), vertices.size(), GL_STATIC_DRAW);
}
template <typename T, typename IT> inline void upload_buffer (IndexedBuffer& ib, std::vector<T> const& vertices, std::vector<IT> const& indices) {
	reupload_vbo(ib.vbo, vertices.data(), vertices.size(), GL_STATIC_DRAW);
	reupload_ebo(ib.ebo, indices.data(), indices.size(), GL_STATIC_DRAW);
}

template <typename T> inline void stream_buffer (VertexBuffer& vb, std::vector<T> const& vertices) {
	reupload_vbo(vb.vbo, vertices.data(), vertices.size(), GL_STREAM_DRAW);
}
template <typename T, typename IT> inline void stream_buffer (IndexedBuffer& ib, std::vector<T> const& vertices, std::vector<IT> const& indices) {
	reupload_vbo(ib.vbo, vertices.data(), vertices.size(), GL_STREAM_DRAW);
	reupload_ebo(ib.ebo, indices.data(), indices.size(), GL_STREAM_DRAW);
}

typedef struct {
	uint32_t  count;
	uint32_t  instanceCount;
	uint32_t  first;
	uint32_t  baseInstance;
} glDrawArraysIndirectCommand;

typedef struct {
	uint32_t count;
	uint32_t primCount;
	uint32_t firstIndex;
	uint32_t baseVertex;
	uint32_t baseInstance;
} glDrawElementsIndirectCommand;

// 
struct Mesh {
	VertexBuffer vb;
	int vertex_count;
};

template <typename T>
inline Mesh upload_mesh (std::string_view label, T* vertices, size_t vertex_count) {
	Mesh m;
	m.vb = vertex_buffer<T>(label);
	m.vertex_count = (int)vertex_count;

	glBindBuffer(GL_ARRAY_BUFFER, m.vb.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(T), vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return m;
}

//
struct IndexedMesh {
	IndexedBuffer ib;
	uint32_t vertex_count;
	uint32_t index_count;
};

template <typename T, typename IT>
inline IndexedMesh upload_mesh (std::string_view label,
		T* vertices, size_t vertex_count, IT* indices, size_t index_count) {
	IndexedMesh m;
	m.ib = indexed_buffer<T>(label);
	m.vertex_count = (uint32_t)vertex_count;
	m.index_count = (uint32_t)index_count;

	glBindBuffer(GL_ARRAY_BUFFER, m.ib.vbo);
	glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(T), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ib.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(IT), indices, GL_STATIC_DRAW);
	
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	return m;
}

//// Textures

// upload texture, use with sampler please
inline bool upload_texture (GLuint tex2d, const char* filename) {

	Image<srgba8> img;
	if (!img.load_from_file(filename, &img))
		return false;

	glBindTexture(GL_TEXTURE_2D, tex2d);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, img.size.x, img.size.y, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, img.pixels);

	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	return true;
}
inline bool upload_normal_map (GLuint tex2d, const char* filename) {

	Image<srgba8> img;
	if (!img.load_from_file(filename, &img))
		return false;

	glBindTexture(GL_TEXTURE_2D, tex2d);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.size.x, img.size.y, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, img.pixels);

	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	return true;
}

//// Shader management

enum ShaderStage {
	VERTEX_SHADER,
	FRAGMENT_SHADER,
	COMPUTE_SHADER,
};

static constexpr GLenum SHADER_STAGE_GLENUM[] = {		GL_VERTEX_SHADER,	GL_FRAGMENT_SHADER,		GL_COMPUTE_SHADER	};
static constexpr const char* SHADER_STAGE_NAME[] = {	"vertex",			"fragment",				"compute" };
static constexpr const char* SHADER_STAGE_MACRO[] = {	"_VERTEX",			"_FRAGMENT",			"_COMPUTE" };

struct MacroDefinition {
	std::string name;
	std::string value;
};

struct Shader {
	std::string						name;

	std::vector<ShaderStage>		stages;
	std::vector<MacroDefinition>	macros;

	uniform_set						uniforms;
	std::vector<std::string>		src_files;

	GLuint	prog = 0;

	bool compile (bool wireframe = false) {

		src_files.clear();
		uniforms.clear();

		std::string source;
		source.reserve(4096);

		std::string filename = prints("shaders/gl/%s.glsl", name.c_str());

		// Load shader base source file
		if (!preprocess_include_file(name.c_str(), filename.c_str(), &source, &src_files)) {
			clog(ERROR, "[Shaders] \"%s\": shader compilation error!\n", name.c_str());
			return false;
		}

		// Compile shader stages

		prog = glCreateProgram();
		OGL_DBG_LABEL(GL_PROGRAM, prog, name);

		std::vector<GLuint> compiled_stages;

		bool error = false;

		for (auto stage : stages) {

			GLuint shad = glCreateShader(SHADER_STAGE_GLENUM[stage]);
			glAttachShader(prog, shad);

			std::string macro_text;
			macro_text.reserve(512);

			macro_text += "// Per-shader macro definitions\n";
			macro_text += prints("#define %s\n", SHADER_STAGE_MACRO[stage]);
			if (wireframe)
				macro_text += prints("#define _WIREFRAME\n");
			for (auto& m : macros)
				macro_text += prints("#define %s %s\n", m.name.c_str(), m.value.c_str());
			macro_text += "\n";
			std::string stage_source = preprocessor_insert_macro_defs(source, filename.c_str(), macro_text);

			{
				const char* ptr = stage_source.c_str();
				glShaderSource(shad, 1, &ptr, NULL);
			}

			glCompileShader(shad);

			{
				GLint status;
				glGetShaderiv(shad, GL_COMPILE_STATUS, &status);

				std::string log_str;
				bool log_avail = get_shader_compile_log(shad, &log_str);
				//if (log_avail) log_str = map_shader_log(log_str, stage_source.c_str());

				bool stage_error = status == GL_FALSE;
				if (stage_error) {
					// compilation failed
					clog(ERROR,"[Shaders] OpenGL error in shader compilation \"%s\"!\n>>>\n%s\n<<<\n", name.c_str(), log_avail ? log_str.c_str() : "<no log available>");
					error = true;
				} else {
					// compilation success
					if (log_avail) {
						clog(WARNING,"[Shaders] OpenGL shader compilation log \"%s\":\n>>>\n%s\n<<<\n", name.c_str(), log_str.c_str());
					}
				}
			}

			compiled_stages.push_back(shad);
		}

		if (!error) { // skip linking if stage has error
			glLinkProgram(prog);

			{
				GLint status;
				glGetProgramiv(prog, GL_LINK_STATUS, &status);

				std::string log_str;
				bool log_avail = get_program_link_log(prog, &log_str);

				error = status == GL_FALSE;
				if (error) {
					// linking failed
					clog(ERROR,"[Shaders] OpenGL error in shader linkage \"%s\"!\n>>>\n%s\n<<<\n", name.c_str(), log_avail ? log_str.c_str() : "<no log available>");
				} else {
					// linking success
					if (log_avail) {
						clog(WARNING,"[Shaders] OpenGL shader linkage log \"%s\":\n>>>\n%s\n<<<\n", name.c_str(), log_str.c_str());
					}
				}
			}

			get_uniform_locations();
		}

		for (auto stage : compiled_stages) {
			glDetachShader(prog, stage);
			glDeleteShader(stage);
		}

		{
			GLint uniform_count = 0;
			glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &uniform_count);

			if (uniform_count != 0) {
				GLint max_name_len = 0;
				glGetProgramiv(prog, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_len);

				auto uniform_name = std::make_unique<char[]>(max_name_len);

				for (GLint i=0; i<uniform_count; ++i) {
					GLsizei length = 0;
					GLsizei count = 0;
					GLenum 	type = GL_NONE;
					glGetActiveUniform(prog, i, max_name_len, &length, &count, &type, uniform_name.get());

					ShaderUniform u;
					u.location = glGetUniformLocation(prog, uniform_name.get());
					u.name = std::string(uniform_name.get(), length);
					u.type = type;

					uniforms.emplace_back(std::move(u));
				}
			}
		}

		if (error) {
			clog(ERROR, "[Shaders] \"%s\": shader compilation error!\n", name.c_str());

			glDeleteProgram(prog);
			prog = 0;
		}
		return !error;
	}

	~Shader () {
		if (prog)
			glDeleteProgram(prog);
	}

	void recompile (char const* reason, bool wireframe) {
		auto old_prog = prog;

		clog(INFO, "[Shaders] Recompile shader %-35s due to %s", name.c_str(), reason);

		if (!compile(wireframe)) {
			// new compilation failed, revert old shader
			prog = old_prog;
			return;
		}

		if (old_prog)
			glDeleteProgram(old_prog);
	}

	void get_uniform_locations () {
		for (auto& u : uniforms) {
			u.location = glGetUniformLocation(prog, u.name.c_str());
			if (u.location < 0) {
				// unused uniform? ignore
			}
		}
	}

	inline static bool _findUniform (ShaderUniform const& l, std::string_view const& r) { return l.name == r; }

	inline GLint get_uniform_location (std::string_view const& name) {
		int i = indexof(uniforms, name, _findUniform);
		if (i < 0)
			return i;
		return uniforms[i].location;
	}

	template <typename T>
	inline void set_uniform (std::string_view const& name, T const& val) {
		int i = indexof(uniforms, name, _findUniform);
		if (i >= 0)
			_set_uniform(uniforms[i], val);
	}
	template <typename T>
	inline void set_uniform_array (std::string_view const& name, T const* values, int arr_count) {
		int i = indexof(uniforms, name, _findUniform);
		if (i >= 0)
			_set_uniform(uniforms[i], values, arr_count);
	}
};

struct Shaders {
	std::vector<std::unique_ptr<Shader>> shaders;

	bool wireframe = false;

	void update_recompilation (kiss::ChangedFiles& changed_files, bool wireframe) {
		if (changed_files.any()) {
			for (auto& s : shaders) {
				std::string const* changed_file;
				if (changed_files.contains_any(s->src_files, FILE_ADDED|FILE_MODIFIED|FILE_RENAMED_NEW_NAME, &changed_file)) {
					// any source file was changed
					s->recompile(prints("shader source change (%s)", changed_file->c_str()).c_str(), wireframe);
				}
			}
		}

		if (this->wireframe != wireframe) {
			this->wireframe = wireframe;

			for (auto& s : shaders) {
				s->recompile("wireframe toggle", wireframe);
			}
		}
	}

	Shader* compile (
		char const* name,
		std::vector<MacroDefinition>&& macros,
		std::initializer_list<ShaderStage> stages = { VERTEX_SHADER, FRAGMENT_SHADER }) {
		ZoneScoped;

		auto s = std::make_unique<Shader>();
		s->name = name;
		s->stages = stages;
		s->macros = std::move(macros);

		bool success = s->compile();

		auto* ptr = s.get();
		shaders.push_back(std::move(s));
		return ptr;
	}

	Shader* compile (
		char const* name,
		std::initializer_list<MacroDefinition> macros = {},
		std::initializer_list<ShaderStage> stages = { VERTEX_SHADER, FRAGMENT_SHADER }) {
		return compile(name, std::vector<MacroDefinition>(macros), stages);
	}
};

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
		state = _override(state);

		gl_enable(GL_DEPTH_TEST, state.depth_test);
		// use_reverse_depth
		glDepthFunc(map_depth_func(state.depth_func));
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

		glPolygonMode(GL_FRONT_AND_BACK, state.poly_mode == POLY_FILL ? GL_FILL : GL_LINE);
	}

	// set opengl drawing state to a set of values, where only changes are applied
	// no overrides to not break fullscreen quads etc.
	void set_no_override (PipelineState const& s) {
	#if DEBUGLEVEL >= 3
		{
			assert(state.depth_test == !!glIsEnabled(GL_DEPTH_TEST));
			GLint depth_func;		glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
			assert(map_depth_func(state.depth_func) == depth_func);
			GLint depth_write;		glGetIntegerv(GL_DEPTH_WRITEMASK, &depth_write);
			assert(state.depth_write == !!depth_write);

			assert(state.scissor_test == !!glIsEnabled(GL_SCISSOR_TEST));

			bool culling = !!glIsEnabled(GL_CULL_FACE);
			assert(state.culling == culling);
			GLint cull_face;		glGetIntegerv(GL_CULL_FACE_MODE, &cull_face);
			assert((state.cull_face == CULL_FRONT ? GL_FRONT : GL_BACK) == cull_face);

			assert(state.blend_enable == !!glIsEnabled(GL_BLEND));
			GLint blend_eq;		glGetIntegerv(GL_BLEND_EQUATION, &blend_eq);
			assert(state.blend_func.equation == blend_eq);
			GLint blend_rgbs, blend_rgbd, blend_as, blend_ad;
			glGetIntegerv(GL_BLEND_SRC_RGB, &blend_rgbs);
			glGetIntegerv(GL_BLEND_DST_RGB, &blend_rgbd);
			glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_as);
			glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_ad);
			assert(state.blend_func.sfactor == blend_rgbs && state.blend_func.sfactor == blend_as);
			assert(state.blend_func.dfactor == blend_rgbd && state.blend_func.dfactor == blend_ad);

			GLint poly_mode;		glGetIntegerv(GL_POLYGON_MODE, &poly_mode);
			assert((state.poly_mode == POLY_FILL ? GL_FILL : GL_LINE) == poly_mode);
		}
	#endif


		if (state.depth_test != s.depth_test)
			gl_enable(GL_DEPTH_TEST, s.depth_test);
		if (state.depth_func != s.depth_func)
			glDepthFunc(map_depth_func(s.depth_func));
		if (state.depth_write != s.depth_write)
			glDepthMask(s.depth_write ? GL_TRUE : GL_FALSE);

		if (state.scissor_test != s.scissor_test)
			gl_enable(GL_SCISSOR_TEST, s.scissor_test);

		if (state.culling != s.culling)
			gl_enable(GL_CULL_FACE, s.culling);
		if (state.cull_face != s.cull_face)
			glCullFace(s.cull_face == CULL_FRONT ? GL_FRONT : GL_BACK);

		// blending
		if (state.blend_enable != s.blend_enable)
			gl_enable(GL_BLEND, s.blend_enable);
		if (state.blend_func.equation != s.blend_func.equation)
			glBlendEquation(s.blend_func.equation);
		if (state.blend_func.sfactor != s.blend_func.sfactor || state.blend_func.dfactor != s.blend_func.dfactor)
			glBlendFunc(s.blend_func.sfactor, s.blend_func.dfactor);

		if (state.poly_mode != s.poly_mode)
			glPolygonMode(GL_FRONT_AND_BACK, s.poly_mode == POLY_FILL ? GL_FILL : GL_LINE);

		state = s;
	}

	// set opengl drawing state to a set of values, where only changes are applied
	// override for wireframe etc. are applied to these
	void set (PipelineState const& s) {
		auto o = _override(s);
		set_no_override(o);
	}

	// assume every temporarily bound texture is unbound again
	// (by other texture uploading code etc.)

	std::vector<GLenum> bound_texture_types;

	struct TextureBind {
		struct _Texture {
			GLenum type;
			GLuint tex;

			_Texture (GLenum type, GLuint tex): type{type}, tex{tex} {}

			_Texture (): type{0}, tex{0} {}
			_Texture (Texture1D      const& tex): type{ GL_TEXTURE_1D       }, tex{tex} {}
			_Texture (Texture2D      const& tex): type{ GL_TEXTURE_2D       }, tex{tex} {}
			_Texture (Texture3D      const& tex): type{ GL_TEXTURE_3D       }, tex{tex} {}
			_Texture (Texture2DArray const& tex): type{ GL_TEXTURE_2D_ARRAY }, tex{tex} {}
		};
		std::string_view	uniform_name;
		_Texture			tex;
		GLuint				sampler;

		TextureBind (): uniform_name{}, tex{} {} // null -> empty texture unit

		// allow no null sampler
		TextureBind (std::string_view uniform_name, _Texture const& tex)
			: uniform_name{uniform_name}, tex{tex}, sampler{0} {}
		TextureBind (std::string_view uniform_name, _Texture const& tex, Sampler const& sampl)
			: uniform_name{uniform_name}, tex{tex}, sampler{sampl} {}
	};

	// bind a set of textures into texture units
	// and assign them to shader uniform samplers
	void bind_textures (Shader* shad, TextureBind const* textures, size_t count) {
		bound_texture_types.resize(std::max(bound_texture_types.size(), count));

		for (int i=0; i<(int)count; ++i) {
			auto& to_bind = textures[i];
			auto& bound_type = bound_texture_types[i];
			
			glActiveTexture((GLenum)(GL_TEXTURE0 + i));
			glBindSampler((GLuint)i, to_bind.sampler);

			if (to_bind.tex.type != bound_type && bound_type != 0)
				glBindTexture(bound_type, 0); // unbind previous

			if (to_bind.tex.type != 0) {
				glBindTexture(to_bind.tex.type, to_bind.tex.tex); // bind new

				auto loc = shad->get_uniform_location(to_bind.uniform_name);
				if (loc >= 0)
					glUniform1i(loc, (GLint)i);
			}

			bound_type = to_bind.tex.type;
		}

		// unbind rest
		for (int i=(int)count; i<(int)bound_texture_types.size(); ++i) {
			auto& bound_type = bound_texture_types[i];

			glActiveTexture((GLenum)(GL_TEXTURE0 + i));
			if (bound_type != 0) {
				// unbind previous
				glBindSampler((GLuint)i, 0);
				glBindTexture(bound_type, 0);
				bound_type = 0;
			}
		}
	}
	void bind_textures (Shader* shad, std::initializer_list<TextureBind> textures) {
		bind_textures(shad, textures.begin(), textures.size());
	}
	void bind_textures (Shader* shad, std::vector<TextureBind> const& textures) {
		bind_textures(shad, textures.data(), textures.size());
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
		ImGui::Text("res: %4d x %4d px (%5.2f Mpx)", size.x, size.y, (float)(size.x * size.y) / (1000*1000));
		ImGui::SliderFloat("renderscale", &renderscale, 0.02f, 2.0f);

		ImGui::Checkbox("nearest", &nearest);
	}

	static constexpr GLenum color_format = GL_RGBA16F;// GL_RGBA32F   GL_SRGB8_ALPHA8
	static constexpr GLenum depth_format = GL_DEPTH_COMPONENT32F;
	static constexpr bool color_mips = true;

	int calc_mipmaps (int w, int h) {
		int count = 0;
		for (;;) {
			count++;
			if (w == 1 && h == 1)
				break;

			w = max(w / 2, 1);
			h = max(h / 2, 1);
		}
		return count;
	}

	void update (int2 window_size) {
		auto old_size = size;
		size = max(1, roundi((float2)window_size * renderscale));

		if (old_size != size) {
			glActiveTexture(GL_TEXTURE0); // try clobber consistent texture at least

			 // delete old
			glDeleteTextures(1, &color);
			glDeleteTextures(1, &depth);
			glDeleteFramebuffers(1, &fbo);

			GLint levels = 1;
			if (color_mips)
				levels = calc_mipmaps(size.x, size.y);

			// create new
			glGenTextures(1, &color);
			glBindTexture(GL_TEXTURE_2D, color);
			glTexStorage2D(GL_TEXTURE_2D, levels, color_format, size.x, size.y);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels-1);

			glGenTextures(1, &depth);
			glBindTexture(GL_TEXTURE_2D, depth);
			glTexStorage2D(GL_TEXTURE_2D, 1, depth_format, size.x, size.y);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);


			glGenFramebuffers(1, &fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);

			OGL_DBG_LABEL(GL_TEXTURE, color, "Framebuffer.color");
			OGL_DBG_LABEL(GL_TEXTURE, depth, "Framebuffer.depth");
			OGL_DBG_LABEL(GL_FRAMEBUFFER, fbo, "Framebuffer.fbo");

			//GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			//if (status != GL_FRAMEBUFFER_COMPLETE) {
			//	fprintf(stderr, "glCheckFramebufferStatus: %x\n", status);
			//}

			glBindTexture(GL_TEXTURE_2D, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}
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
