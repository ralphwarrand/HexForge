//Hex
#include "Renderer/Shader.h"
#include "Engine/Logger.h"

//Lib
#include <glm/glm.hpp>

//STL
#include <iostream>
#include <fstream>
#include <sstream>

namespace Hex
{
    Shader::Shader(const std::string& vertex_path, const std::string& fragment_path) {
        // Load and compile shaders
        const std::string vertex_source = LoadShaderSource(vertex_path);
        const std::string fragment_source = LoadShaderSource(fragment_path);
        const GLuint vertex_shader = CompileShader(GL_VERTEX_SHADER, vertex_source);
        const GLuint fragment_shader = CompileShader(GL_FRAGMENT_SHADER, fragment_source);

        // Link shaders into a program
        m_program_id = glCreateProgram();
        LinkProgram(vertex_shader, fragment_shader);

        // Clean up shaders (no longer needed after linking)
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
    }

    Shader::~Shader() {
        //glDeleteProgram(m_program_id);
    }

    void Shader::Bind() const {
        glUseProgram(m_program_id);
    }

    void Shader::Unbind()
    {
        glUseProgram(0);
    }

    GLuint Shader::GetProgramID() const
    {
        return m_program_id;
    }

    // Uniform setting functions
    void Shader::SetUniform1i(const std::string& name, const int value) {
        glUniform1i(GetUniformLocation(name), value);
    }

    void Shader::SetUniform1f(const std::string& name, const float value) {
        glUniform1f(GetUniformLocation(name), value);
    }

    void Shader::SetUniform2f(const std::string& name, const float x, const float y) {
        glUniform2f(GetUniformLocation(name), x, y);
    }

    void Shader::SetUniformVec3(const std::string& name, const glm::vec3& value) {
        glUniform3fv(GetUniformLocation(name), 1, &value[0]);
    }

    void Shader::SetUniformMat4(const std::string& name, const glm::mat4& matrix) {
        glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, &matrix[0][0]);
    }

    // Private utility functions
    std::string Shader::LoadShaderSource(const std::string& filepath) {
        std::ifstream file(filepath);

        // Check if the file is open
        if (!file.is_open()) {
           Log(LogLevel::Error, std::format("Unable to open shader file: {}", filepath));
        }

        // Read file contents
        std::stringstream buffer;
        buffer << file.rdbuf();

        return buffer.str();
    }

    GLuint Shader::CompileShader(const GLenum type, const std::string& source) {
        const GLuint shader = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        // Check for compilation errors
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char info_log[512];
            glGetShaderInfoLog(shader, 512, nullptr, info_log);
            std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << info_log << std::endl;
            Log(LogLevel::Error, std::format("ERROR::SHADER::COMPILATION_FAILED\n{}", info_log));
        }

        return shader;
    }

    void Shader::LinkProgram(const GLuint vertex_shader, const GLuint fragment_shader) const
    {
        glAttachShader(m_program_id, vertex_shader);
        glAttachShader(m_program_id, fragment_shader);
        glLinkProgram(m_program_id);

        // Check for linking errors
        GLint success;
        glGetProgramiv(m_program_id, GL_LINK_STATUS, &success);
        if (!success) {
            char info_log[512];
            glGetProgramInfoLog(m_program_id, 512, nullptr, info_log);
            Log(LogLevel::Error, std::format("ERROR::SHADER::PROGRAM::LINKING_FAILED\n{}", info_log));
        }
    }

    GLint Shader::GetUniformLocation(const std::string& name) {
        // Check cache for location
        if (m_uniform_location_cache.contains(name)) {
            return m_uniform_location_cache[name];
        }

        // Get location and cache it
        const GLint location = glGetUniformLocation(m_program_id, name.c_str());
        if (location == -1) {
            Log(LogLevel::Warning, std::format("Uniform {} doesn't exist", name));
        }
        m_uniform_location_cache[name] = location;
        return location;
    }
}
