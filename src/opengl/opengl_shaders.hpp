#pragma once
#include "common.hpp"
#include "opengl_helper.hpp"
#include "shader_preprocessor.hpp"
#include <regex>

namespace gl {

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
		if (!preprocess_include_file(filename.c_str(), &source, &src_files)) {
			clog(ERROR, "[Shaders] \"%s\": shader compilation error!\n", name.c_str());
			return false;
		}

		uniforms = parse_shader_uniforms(source);

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

		if (error) {
			clog(ERROR, "[Shaders] \"%s\": shader compilation error!\n", name.c_str());
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
			std::initializer_list<MacroDefinition> macros = {},
			std::initializer_list<ShaderStage> stages = { VERTEX_SHADER, FRAGMENT_SHADER }) {
		ZoneScoped;

		auto s = std::make_unique<Shader>();
		s->name = name;
		s->stages = stages;
		s->macros = macros;

		s->compile();

		auto* ptr = s.get();
		shaders.push_back(std::move(s));
		return ptr;
	}
};

} // namespace gl
