#pragma once
#include "../util/move_only_class.hpp"
#include "../kissmath.hpp"
#include "../glad/glad.h"
#include <vector>
#include "assert.h"

/////
// OpenGL abstractions
////

//// GL specific stuff, don't use in user code
namespace gl {
	// OpenGL glsl types
	enum type {
		FLOAT			,
		FLOAT2			,
		FLOAT3			,
		FLOAT4			,
		INT				,
		INT2			,
		INT3			,
		INT4			,
		U8V4			,
		MAT3			,
		MAT4			,
		BOOL			,
	};

	// GLenum that i can actually make sense of in the debugger (raw GLenum's just show up like 0x2186)
	enum class Enum : GLenum {
		FLOAT			= GL_FLOAT,
		INT				= GL_INT,
		UNSIGNED_BYTE	= GL_UNSIGNED_BYTE,
		BOOL			= GL_BOOL,

		NEAREST					= GL_NEAREST,
		LINEAR					= GL_LINEAR,
		NEAREST_MIPMAP_NEAREST	= GL_NEAREST_MIPMAP_NEAREST,
		LINEAR_MIPMAP_NEAREST	= GL_LINEAR_MIPMAP_NEAREST,
		NEAREST_MIPMAP_LINEAR	= GL_NEAREST_MIPMAP_LINEAR,
		LINEAR_MIPMAP_LINEAR	= GL_LINEAR_MIPMAP_LINEAR,

		REPEAT					= GL_REPEAT,
		MIRRORED_REPEAT			= GL_MIRRORED_REPEAT,
		CLAMP_TO_EDGE			= GL_CLAMP_TO_EDGE,
		CLAMP_TO_BORDER			= GL_CLAMP_TO_BORDER,
		MIRROR_CLAMP_TO_EDGE	= GL_MIRROR_CLAMP_TO_EDGE,
	};

	// Simple VBO class to manage lifetime
	class Vbo {
		MOVE_ONLY_CLASS_DECL(Vbo)
		GLuint vbo;
	public:

		Vbo () {
			glGenBuffers(1, &vbo);
		}
		~Vbo () {
			glDeleteBuffers(1, &vbo);
		}

		operator GLuint () const { return vbo; }
	};

	// Simple VAO class to manage lifetime
	class Vao {
		MOVE_ONLY_CLASS_DECL(Vao)
		GLuint vao;
	public:

		Vao () {
			glGenVertexArrays(1, &vao);
		}
		~Vao () {
			glDeleteVertexArrays(1, &vao);
		}

		operator GLuint () const { return vao; }
	};

	// Simple UBO class to manage lifetime
	class Ubo {
		MOVE_ONLY_CLASS_DECL(Ubo)
		GLuint ubo;
	public:

		Ubo () {
			glGenBuffers(1, &ubo);
		}
		~Ubo () {
			glDeleteBuffers(1, &ubo);
		}

		operator GLuint () const { return ubo; }
	};

	// Simple UBO class to manage lifetime
	class Texture {
		MOVE_ONLY_CLASS_DECL(Texture)
		GLuint tex;
	public:

		Texture () {
			glGenTextures(1, &tex);
		}
		~Texture () {
			glDeleteTextures(1, &tex);
		}

		operator GLuint () const { return tex; }
	};

	// Sampler Sampler class to manage lifetime
	class Sampler {
		MOVE_ONLY_CLASS_DECL(Sampler)
		GLuint sampler;
	public:

		Sampler () {
			glGenSamplers(1, &sampler);
		}
		~Sampler () {
			glDeleteSamplers(1, &sampler);
		}

		operator GLuint () const { return sampler; }
	};
}

//// user facing stuff that hopefully is generic enough to work across OpenGL, D3D, Vulkan etc.

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

	template<> static constexpr int get_align<float   > () { return N; }
	template<> static constexpr int get_align<float2  > () { return 2*N; }
	template<> static constexpr int get_align<float3  > () { return 4*N; }
	template<> static constexpr int get_align<float4  > () { return 4*N; }
	template<> static constexpr int get_align<float4x4> () { return 4*N; }

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

struct SharedUniformsInfo {
	char const* name;
	int binding_point;
};

template <typename T>
class SharedUniforms {
	gl::Ubo ubo;

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

public:
	// needed because explicit uniform block locations are not core in OpenGL 3.3
	SharedUniformsInfo info;

	SharedUniforms (SharedUniformsInfo info): info{info} {
		glBindBuffer(GL_UNIFORM_BUFFER, ubo);

		glBufferData(GL_UNIFORM_BUFFER, sizeof(T), NULL, GL_STATIC_DRAW);

		glBindBufferBase(GL_UNIFORM_BUFFER, info.binding_point, ubo); 

		// dont unbind GL_UNIFORM_BUFFER
	}

	void set (T const& val) {
		glBindBuffer(GL_UNIFORM_BUFFER, ubo);

		glBufferData(GL_UNIFORM_BUFFER, sizeof(T), &val, GL_STATIC_DRAW);

		// dont unbind GL_UNIFORM_BUFFER
	}
};

// Required to define custom vertex formats
class Attributes {
	struct _Attribute {
		gl::Enum	type;
		int			components;
	};

	template <typename T> static inline constexpr _Attribute to_attrib ();

	template<> static inline constexpr _Attribute to_attrib<float   > () { return { gl::Enum::FLOAT        , 1 }; }
	template<> static inline constexpr _Attribute to_attrib<float2  > () { return { gl::Enum::FLOAT        , 2 }; }
	template<> static inline constexpr _Attribute to_attrib<float3  > () { return { gl::Enum::FLOAT        , 3 }; }
	template<> static inline constexpr _Attribute to_attrib<float4  > () { return { gl::Enum::FLOAT        , 4 }; }
	template<> static inline constexpr _Attribute to_attrib<int     > () { return { gl::Enum::INT          , 1 }; }
	template<> static inline constexpr _Attribute to_attrib<int2    > () { return { gl::Enum::INT          , 2 }; }
	template<> static inline constexpr _Attribute to_attrib<int3    > () { return { gl::Enum::INT          , 3 }; }
	template<> static inline constexpr _Attribute to_attrib<int4    > () { return { gl::Enum::INT          , 4 }; }
	template<> static inline constexpr _Attribute to_attrib<uint8v4 > () { return { gl::Enum::UNSIGNED_BYTE, 4 }; }
	//template<> static inline constexpr _Attribute to_attrib<bool    > (Attribute<bool    > a) { return { a.name, Type::BOOL , 1, a.normalized, a.stride, a.offs }; }

public:
	Attributes () {}

#if 0
	Shader associated_shader;

	Attributes (Shader& associated_shader): associated_shader{associated_shader} {}
#endif

	// Add a interleaved vertex attribute of type T
	template <typename T>
	void add (int location, const char* name, int stride, uintptr_t offset, bool normalized=false) {
		static constexpr auto a = to_attrib<T>();

		glEnableVertexAttribArray(location);
		glVertexAttribPointer(location, a.components, (GLenum)a.type, normalized, (GLsizei)stride, (void*)offset);
	}

#if 0
	// Add a interleaved vertex attribute of type T with implicit location in the shader registered in Attributes
	template <typename T>
	void add (const char* name, int stride, int offset, bool normalized=false) {
		static constexpr a = to_attrib<T>();

		if (associated_shader.shader == nullptr) {
			assert(false);
			return;
		}

		GLuint location = glGetAttribLocation(associated_shader.shader->shad, a.name);
		if (loc <= -1) {
			return; // attribute not used in shader
		}

		glEnableVertexAttribArray(location);
		glVertexAttribPointer(location, a.components, (GLenum)a.type, normalized, (GLsizei)stride, (void*)offs);
	}
#endif
};

/* Add a method to a custom Vertex struct like so to allow creation of Mesh<Vertex>:
	struct Vertex {
		float3	pos_world;
		lrgba	color;

		void bind (Attributes& a) {
			a.add<decltype(pos_world)>("pos_world", sizeof(Vertex), offsetof(Vertex, pos_world));
			a.add<decltype(color    )>("color"    , sizeof(Vertex), offsetof(Vertex, color    ));
		}
};
*/

// GPU mesh data
template <typename T>
class Mesh {
	gl::Vbo vbo;
	gl::Vao vao;

	void init_vao () {
		glBindVertexArray(vao);

		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		Attributes a;
		T::bind(a);

		glBindVertexArray(0);

		// dont unbind GL_ARRAY_BUFFER
	}

public:

	uintptr_t vertex_count = 0;

	// Construct empty Mesh (can upload data later)
	Mesh () {
		init_vao();
	}

	// Construct by Mesh with uploaded data (can reupload different data later)
	Mesh (T const* data, int count) {
		init_vao();

		// assume GL_STATIC_DRAW
		glBufferData(GL_ARRAY_BUFFER, count * sizeof(T), data, GL_STATIC_DRAW);
		vertex_count = count;

		// dont unbind GL_ARRAY_BUFFER
	}
	// Construct by Mesh with uploaded data (can reupload different data later)
	Mesh (std::vector<T> const& data): Mesh(data.data(), data.size()) {}

	// Upload new data to Mesh
	void upload (T const* data, uintptr_t count) {
		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		// assume GL_STATIC_DRAW
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * sizeof(T)), NULL, GL_STATIC_DRAW); // buffer orphan is supposed to be better for streaming
		glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(count * sizeof(T)), data, GL_STATIC_DRAW);
		vertex_count = count;

		// dont unbind
	}
	// Upload new data to Mesh
	void upload (std::vector<T> const& data) {
		upload(data.data(), data.size());
	}

	// Bind to be able to draw
	void bind () {
		glBindVertexArray(vao);
	}

	// Shader and mesh must be bound
	void draw () {
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_count);
	}
};
