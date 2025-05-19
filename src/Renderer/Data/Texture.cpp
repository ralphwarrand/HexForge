#include "pch.h"

#include "Renderer/Data/Texture.h"

namespace Hex {

    Texture::Texture() {
        glGenTextures(1, &m_id);
    }

    Texture::Texture(Texture&& other) noexcept
      : m_id(other.m_id)
    {
        other.m_id = 0;
    }

    Texture& Texture::operator=(Texture&& other) noexcept {
        if (this != &other) {
            if (m_id) glDeleteTextures(1, &m_id);
            m_id = other.m_id;
            other.m_id = 0;
        }
        return *this;
    }

    Texture::~Texture() {
        if (m_id) glDeleteTextures(1, &m_id);
    }

    void Texture::Bind(GLuint unit) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, m_id);
    }

    void Texture::Unbind(GLuint unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Texture::SetWrap(GLint s, GLint t) const {
        glBindTexture(GL_TEXTURE_2D, m_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, s);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, t);
    }

    void Texture::SetFilter(GLint minFilter, GLint magFilter) const {
        glBindTexture(GL_TEXTURE_2D, m_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    }

} // namespace Hex
