#pragma once
#include "stdafx.hpp"
#include "../kissmath.hpp"
#include "../util/string.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include "assert.h"
#include "gl.hpp"
#include "../util/file_io.hpp"

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

		// for debugging purposes
		std::vector<std::string> preprocessed_sources;

		std::vector<SharedUniformsInfo> UBOs;

		bool get_uniform (std::string_view name, Uniform* type);

		void bind_uniform_block (SharedUniformsInfo const& u);

		kiss::raw_data get_program_binary (uint64_t* size);

		~Shader () {
			glDeleteProgram(shad);
		}
	};

	// to be able to reload shader we need to overwrite a shader with it's new version without updating the pointer itself
	// so just call this to do a swap in place
	inline void swap (Shader& l, Shader& r) {
		std::swap(l.shad, r.shad);
		std::swap(l.sources, r.sources);
		std::swap(l.uniforms, r.uniforms);
		std::swap(l.preprocessed_sources, r.preprocessed_sources);
		std::swap(l.UBOs, r.UBOs);
	}

	GLuint load_shader_part (const char* type, GLenum gl_type, std::string const& source, std::string_view path, gl::Shader* shader, std::string const& name, bool* error);
	
	std::unique_ptr<gl::Shader> load_shader (std::string const& name, const char* shaders_directory, std::vector<SharedUniformsInfo> UBOs);
}
