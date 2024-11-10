#include "Renderer/Shader.h"

namespace Hex
{
    Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath) {
        // Load and compile shaders
        std::string vertexSource = LoadShaderSource(vertexPath);
        std::string fragmentSource = LoadShaderSource(fragmentPath);
        GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);

        // Link shaders into a program
        m_programID = glCreateProgram();
        LinkProgram(vertexShader, fragmentShader);

        // Clean up shaders (no longer needed after linking)
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    Shader::~Shader() {
        glDeleteProgram(m_programID);
    }

    void Shader::Bind() const {
        glUseProgram(m_programID);
    }

    void Shader::Unbind() const {
        glUseProgram(0);
    }

    GLuint Shader::GetProgramID()
    {
        return m_programID;
    }

    // Uniform setting functions
    void Shader::SetUniform1i(const std::string& name, int value) {
        glUniform1i(GetUniformLocation(name), value);
    }

    void Shader::SetUniform1f(const std::string& name, float value) {
        glUniform1f(GetUniformLocation(name), value);
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
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    GLuint Shader::CompileShader(GLenum type, const std::string& source) {
        GLuint shader = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        // Check for compilation errors
        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
        }

        return shader;
    }

    void Shader::LinkProgram(GLuint vertexShader, GLuint fragmentShader) {
        glAttachShader(m_programID, vertexShader);
        glAttachShader(m_programID, fragmentShader);
        glLinkProgram(m_programID);

        // Check for linking errors
        GLint success;
        glGetProgramiv(m_programID, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(m_programID, 512, nullptr, infoLog);
            std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        }
    }

    GLint Shader::GetUniformLocation(const std::string& name) {
        // Check cache for location
        if (m_uniformLocationCache.find(name) != m_uniformLocationCache.end()) {
            return m_uniformLocationCache[name];
        }

        // Get location and cache it
        GLint location = glGetUniformLocation(m_programID, name.c_str());
        if (location == -1) {
            std::cerr << "Warning: uniform '" << name << "' doesn't exist!" << std::endl;
        }
        m_uniformLocationCache[name] = location;
        return location;
    }
}