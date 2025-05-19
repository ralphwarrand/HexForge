#pragma once

//STL
#include <string>
#include <unordered_map>

// Third-party
#include <glad/glad.h>
#include <glm/fwd.hpp>

namespace Hex
{
    class Shader{
    public:
        Shader(const std::string& vertex_path, const std::string& fragment_path);
        ~Shader();

        Shader(const Shader&) = default;
        Shader(Shader&&)  noexcept = default;

        Shader& operator = (const Shader&) = default;
        Shader& operator = (Shader&&) = default;

        void Bind() const;
        static void Unbind();

        [[nodiscard]] GLuint GetProgramID() const;

        // Uniform setting methods
        void SetUniform1i(const std::string& name, int value);
        void SetUniform1f(const std::string& name, float value);
        void SetUniform2f(const std::string& name, float x, float y);
        void SetUniformVec3(const std::string& name, const glm::vec3& value);
        void SetUniformMat4(const std::string& name, const glm::mat4& matrix);

    private:
        GLuint m_program_id;
        std::unordered_map<std::string, GLint> m_uniform_location_cache;

        static std::string LoadShaderSource(const std::string& filepath);
        static GLuint CompileShader(GLenum type, const std::string& source);
        void LinkProgram(GLuint vertex_shader, GLuint fragment_shader) const;
        GLint GetUniformLocation(const std::string& name);
    };
}
