#pragma once
#include "globjects.hpp"
#include "../kissmath.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include "assert.h"

struct Uniform {
	GLint			loc = 0;
	const char*		name = nullptr;
	gl::data_type	type;

	void set (float v) {
		assert(type == gl::T_FLT);
		glUniform1f(loc, v);
	}
	void set (float2 v) {
		assert(type == gl::T_V2);
		glUniform2fv(loc, 1, &v.x);
	}
	void set (float3 v) {
		assert(type == gl::T_V3);
		glUniform3fv(loc, 1, &v.x);
	}
	void set (float4 v) {
		assert(type == gl::T_V4);
		glUniform4fv(loc, 1, &v.x);
	}
	void set (int v) {
		assert(type == gl::T_INT);
		glUniform1i(loc, v);
	}
	void set (int2 v) {
		assert(type == gl::T_IV2);
		glUniform2iv(loc, 1, &v.x);
	}
	void set (int3 v) {
		assert(type == gl::T_IV3);
		glUniform3iv(loc, 1, &v.x);
	}
	void set (int4 v) {
		assert(type == gl::T_IV4);
		glUniform4iv(loc, 1, &v.x);
	}
	void set (float3x3 v) {
		assert(type == gl::T_M3);
		glUniformMatrix3fv(loc, 1, GL_FALSE, &v.arr[0][0]);
	}
	void set (float4x4 v) {
		assert(type == gl::T_M4);
		glUniformMatrix4fv(loc, 1, GL_FALSE, &v.arr[0][0]);
	}
	void set (bool b) {
		assert(type == gl::T_BOOL);
		glUniform1i(loc, (int)b);
	}
};

struct Shader {
	std::vector<std::string> sources; // all files used in the shader, ie. the source itself and all included files

	gl::ShaderProgram		 shad;

	Uniform get_uniform (char const* name, gl::data_type type);
};

class ShaderManager {
	GLuint load_shader_part (const char* type, GLenum gl_type, std::string const& source, std::string_view path, std::vector<std::string>* sources, GLuint prog, std::string const& name, bool* error);

public:
	const std::string shaders_directory = "shaders/";

	std::unordered_map<std::string, std::shared_ptr<Shader>> shaders; // string is shader name, ie. source file path without .glsl

	// Loads a shader file with filename "{name}.glsl" in shaders_directory which gets preprocessed into vertex, fragment, etc. shaders by only including
	// $if {type}
	//    ...
	// $endif
	// sections for the matching type (ie. $if vertex gets included in vertex shader but preprocessed out in fragment shader
	// vertex and fragment $if are required the other shader types are optional and will cause their respective shader type to not be compiled
	// code outside if $if $endif is included in all shaders
	// $include "filepath" includes a file relative to the path of the current file
	// $if $endif get processed at the same time as the $if $endif, so a $if fragment part in a file only included inside a $if vertex will never be included in the fragment shader
	// Shaders gets stored into shaders hashmap and never removed until program exit
	std::shared_ptr<Shader> load_shader (std::string name);

	bool check_file_changes ();
};

// global ShaderManager
extern ShaderManager shader_manager;
