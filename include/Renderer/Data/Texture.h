#pragma once

// Third-party
#include <glad/glad.h>

namespace Hex {

    class Texture {
    public:
        // Constructs an empty texture object (GL_TEXTURE_2D)
        Texture();

        // Move‐constructible/assignable, but not copyable
        Texture(Texture&& other) noexcept;
        Texture& operator=(Texture&& other) noexcept;

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        // Deletes the GL texture
        ~Texture();

        // Bind this texture to the given unit (default unit 0)
        // and target GL_TEXTURE_2D
        void Bind(GLuint unit = 0) const;

        // Unbinds any texture from that unit/target
        static void Unbind(GLuint unit = 0);

        // Convenience to set wrap/filter parameters
        void SetWrap(GLint s = GL_REPEAT, GLint t = GL_REPEAT) const;
        void SetFilter(GLint minFilter = GL_LINEAR_MIPMAP_LINEAR,
                       GLint magFilter = GL_LINEAR) const;

        // Returns the underlying GL handle
        GLuint GetID() const { return m_id; }

    private:
        GLuint m_id = 0;
    };

} // namespace Hex
