#include "HexForge/pch.h"
#include "HexForge/Renderer/ParticleRenderer.h"
#include "HexForge/Renderer/ShaderManager.h"
#include "HexForge/Physics/PhysicsMaterial.h"
#include "HexForge/Core/Logger.h"

namespace Hex
{
    namespace
    {
        // Layout matches the std140 block in particle.vert. Each element is 16-byte aligned.
        struct alignas(16) MaterialGPU { glm::vec4 color; };
    }

    ParticleRenderer::ParticleRenderer() = default;
    ParticleRenderer::~ParticleRenderer() { Shutdown(); }

    void ParticleRenderer::Init()
    {
        // Unit quad in clip-space-like local frame [-1,1]^2 — billboarded in the vertex shader.
        const float quad_vertices[] = {
            -1.0f, -1.0f,
             1.0f, -1.0f,
             1.0f,  1.0f,
            -1.0f,  1.0f,
        };
        const GLuint quad_indices[] = { 0, 1, 2, 0, 2, 3 };

        glGenVertexArrays(1, &m_vao);
        glBindVertexArray(m_vao);

        // Quad geometry — vertex attrib 0 (vec2 local coord).
        glGenBuffers(1, &m_quad_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

        glGenBuffers(1, &m_quad_ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quad_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_indices), quad_indices, GL_STATIC_DRAW);

        glBindVertexArray(0);

        // Materials UBO — size for kMaxMaterials slots.
        glGenBuffers(1, &m_materials_ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, m_materials_ubo);
        glBufferData(GL_UNIFORM_BUFFER, kMaxMaterials * sizeof(MaterialGPU), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        m_shader = ShaderManager::GetOrCreateShader(
            RESOURCES_PATH "shaders/particle.vert",
            RESOURCES_PATH "shaders/particle.frag");
    }

    void ParticleRenderer::Shutdown()
    {
        if (m_quad_vbo)      { glDeleteBuffers(1, &m_quad_vbo);      m_quad_vbo = 0; }
        if (m_quad_ebo)      { glDeleteBuffers(1, &m_quad_ebo);      m_quad_ebo = 0; }
        if (m_materials_ubo) { glDeleteBuffers(1, &m_materials_ubo); m_materials_ubo = 0; }
        if (m_vao)           { glDeleteVertexArrays(1, &m_vao);      m_vao = 0; }
    }

    void ParticleRenderer::UpdateMaterials(const std::vector<PhysicsMaterial>& materials)
    {
        std::vector<MaterialGPU> packed;
        packed.reserve(kMaxMaterials);
        size_t n = std::min<size_t>(materials.size(), kMaxMaterials);
        for (size_t i = 0; i < n; ++i) {
            MaterialGPU m{};
            m.color = materials[i].color;
            packed.push_back(m);
        }
        // Pad to kMaxMaterials with white so out-of-range ids fail visibly rather than UB.
        while (packed.size() < kMaxMaterials) packed.push_back({ glm::vec4(1.0f) });

        glBindBuffer(GL_UNIFORM_BUFFER, m_materials_ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, packed.size() * sizeof(MaterialGPU), packed.data());
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void ParticleRenderer::Render(GLuint position_vbo, GLuint material_id_vbo, GLuint flags_vbo,
                                  uint32_t particle_count,
                                  const glm::mat4& view, const glm::mat4& proj,
                                  const glm::vec3& view_pos, const glm::vec3& light_dir,
                                  float particle_radius,
                                  bool hide_rigid, bool hide_fluid, bool hide_cloth)
    {
        if (particle_count == 0 || !position_vbo || !material_id_vbo) return;

        glEnable(GL_DEPTH_TEST);

        glBindVertexArray(m_vao);

        // Per-instance position (location 1).
        glBindBuffer(GL_ARRAY_BUFFER, position_vbo);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        glVertexAttribDivisor(1, 1);

        // Per-instance material id (location 2). Note glVertexAttribIPointer for integer attrib.
        glBindBuffer(GL_ARRAY_BUFFER, material_id_vbo);
        glEnableVertexAttribArray(2);
        glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(uint32_t), nullptr);
        glVertexAttribDivisor(2, 1);

        // Per-instance flags (location 3). bit 0 = rigid body member.
        if (flags_vbo) {
            glBindBuffer(GL_ARRAY_BUFFER, flags_vbo);
            glEnableVertexAttribArray(3);
            glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(uint32_t), nullptr);
            glVertexAttribDivisor(3, 1);
        } else {
            glDisableVertexAttribArray(3);
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        m_shader->Bind();
        m_shader->SetUniformMat4("u_view", view);
        m_shader->SetUniformMat4("u_proj", proj);
        // Push the sun direction into view space here so the frag shader doesn't have to.
        // light_dir points "from the light toward the scene"; we want "from surface toward light".
        glm::vec3 view_light_dir = glm::normalize(glm::mat3(view) * (-light_dir));
        m_shader->SetUniformVec3("u_view_light_dir", view_light_dir);
        m_shader->SetUniform1f("u_radius", particle_radius);
        m_shader->SetUniform1i("u_hide_rigid", hide_rigid ? 1 : 0);
        m_shader->SetUniform1i("u_hide_fluid", hide_fluid ? 1 : 0);
        m_shader->SetUniform1i("u_hide_cloth", hide_cloth ? 1 : 0);
        (void)view_pos;

        glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_materials_ubo);
        GLuint blockIdx = glGetUniformBlockIndex(m_shader->GetProgramID(), "Materials");
        if (blockIdx != GL_INVALID_INDEX) {
            glUniformBlockBinding(m_shader->GetProgramID(), blockIdx, 2);
        }

        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, particle_count);

        Shader::Unbind();
        glBindVertexArray(0);
    }
}
