#pragma once
#include "../move_only_class.hpp"
#include "../kissmath.hpp"
#include "../glad/glad.h"
#include <vector>

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
}

//// user facing stuff that hopefully is generic enough to work across OpenGL, D3D, Vulkan etc.

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

/*
// CPU mesh data
template <typename T>
struct MeshData {
	std::vector<T> vertex_data;

	void 
};*/

// GPU mesh data
template <typename T>
class Mesh {
	gl::Vbo vbo;
	gl::Vao vao;

	uintptr_t vertex_count = 0;

	void init_vao () {
		glBindVertexArray(vao);

		glBindBuffer(GL_ARRAY_BUFFER, vbo);

		Attributes a;
		T::bind(a);

		glBindVertexArray(0);

		// dont unbind GL_ARRAY_BUFFER
	}

public:

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
	//// Construct by Mesh with uploaded data (can reupload different data later)
	//Mesh (MeshData<T> const& data): Mesh(data.vertex_data.data(), data.vertex_data.size()) {}

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
	//// Upload new data to Mesh
	//void upload (MeshData<T> const& data) {
	//	upload(data.vertex_data.data(), data.vertex_data.size());
	//}

	// Bind to be able to draw
	void bind () {
		glBindVertexArray(vao);
	}

	// Shader and mesh must be bound
	void draw () {
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_count);
	}
};

