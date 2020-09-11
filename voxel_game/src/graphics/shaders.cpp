#include "stdafx.hpp"
#include "shaders.hpp"

gl::Shader* ShaderManager::load_shader (std::string name, std::initializer_list<SharedUniformsInfo> UBOs) {
	auto it = shaders.find(name);
	if (it != shaders.end()) {
		return it->second.get();
	}

	auto ptr = gl::load_shader(name, shaders_directory.c_str(), UBOs);
	gl::Shader* s = ptr.get();
	shaders.emplace(std::move(name), std::move(ptr));

	return s;
}

std::unique_ptr<ShaderManager> shaders = nullptr;
