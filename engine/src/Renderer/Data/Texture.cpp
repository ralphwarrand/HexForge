// Hex
#include "HexForge/pch.h"
#include "HexForge/Renderer/Data/Texture.h"

namespace Hex
{

    static GLuint s_whiteTex = 0, s_defaultNormalTex = 0;

    Texture::Texture() {
        glGenTextures(1, &m_id);
    }

    void Texture::InitDefaults() {
        // **** WHITE ****
        glGenTextures(1,&s_whiteTex);
        glBindTexture(GL_TEXTURE_2D, s_whiteTex);
        uint8_t white[4]={255,255,255,255};
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,white);
        // clamp + filter don't really matter here

        // **** DEFAULT NORMAL ****
        glGenTextures(1,&s_defaultNormalTex);
        glBindTexture(GL_TEXTURE_2D, s_defaultNormalTex);
        // normal in [0,1] = (0.5,0.5,1.0)
        uint8_t norm[4]={128,128,255,255};
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,norm);
    }

    void Texture::BindWhite() {
        glBindTexture(GL_TEXTURE_2D, s_whiteTex);
    }
    void Texture::BindDefaultNormal() {
        glBindTexture(GL_TEXTURE_2D, s_defaultNormalTex);
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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    }

    void Texture::SetFilter(GLint minFilter, GLint magFilter) const {
        glBindTexture(GL_TEXTURE_2D, m_id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    }

} // namespace Hex
