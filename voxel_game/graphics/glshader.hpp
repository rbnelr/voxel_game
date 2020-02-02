#pragma once
#include "../kissmath.hpp"
#include "../string.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include "assert.h"
#include "gl.hpp"

namespace gl {
	struct Uniform {
		GLint			loc = -1;
		gl::type		type;

		Uniform () {}
		Uniform (gl::type t): type{t} {}
	};

	struct Shader {
		GLuint shad = 0; // 0 == shader no loaded successfully

						 // all files used in the shader, ie. the source itself and all included files
		std::vector<std::string> sources;

		// all uniforms found in the shader
		std::unordered_map<kiss::map_string, Uniform> uniforms;

		bool get_uniform (std::string_view name, Uniform* type);

		~Shader () {
			glDeleteProgram(shad);
		}
	};

	GLuint load_shader_part (const char* type, GLenum gl_type, std::string const& source, std::string_view path, gl::Shader* shader, std::string const& name, bool* error);
	
	std::unique_ptr<gl::Shader> load_shader (std::string const& name, const char* shaders_directory);
}
