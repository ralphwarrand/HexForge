#pragma once
#include <string>
#include <unordered_map>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <iostream>
#include <fstream>
#include <sstream>

namespace Hex
{
    class Shader{
    public:
        Shader(const std::string& vertexPath, const std::string& fragmentPath);
        ~Shader();

        void Bind() const;
        void Unbind() const;

        GLuint GetProgramID();

        // Uniform setting methods
        void SetUniform1i(const std::string& name, int value);
        void SetUniform1f(const std::string& name, float value);
        void SetUniformVec3(const std::string& name, const glm::vec3& value);
        void SetUniformMat4(const std::string& name, const glm::mat4& matrix);

    private:
        GLuint m_programID;
        std::unordered_map<std::string, GLint> m_uniformLocationCache;

        std::string LoadShaderSource(const std::string& filepath);
        GLuint CompileShader(GLenum type, const std::string& source);
        void LinkProgram(GLuint vertexShader, GLuint fragmentShader);
        GLint GetUniformLocation(const std::string& name);
    };
}