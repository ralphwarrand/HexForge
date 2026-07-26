//Hex
#include "HexForge/pch.h"
#include "HexForge/Renderer/Renderer.h"
#include "HexForge/Renderer/ParticleRenderer.h"
#include "HexForge/Core/Logger.h"
#include "HexForge/Renderer/Camera.h"
#include "HexForge/Physics/PhysicsSystem.h"
#include "HexForge/Renderer/DebugRenderer.h"
#include <format>

namespace Hex
{
	// Custom deleter function for GLFWwindow
	static void GLFWwindowDeleter(GLFWwindow* window) {
		if (window) {
			glfwDestroyWindow(window);
		}
	}

	Renderer::Renderer(entt::registry& registry, const AppSpecification& application_spec, const std::shared_ptr<Console>& console): m_window(nullptr, GLFWwindowDeleter)
		, m_registry(registry), m_console(console)  {


		Init(application_spec);
	}

	Renderer::~Renderer()
	{

	}

	void Renderer::Init(const AppSpecification& app_spec)
	{
		InitOpenGLContext(app_spec);
		LogRendererInfo();

		// Create the UBO for RenderData (binding point 0)
		glGenBuffers(1, &m_uboRenderData);
		glBindBuffer(GL_UNIFORM_BUFFER, m_uboRenderData);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(RenderData), nullptr, GL_DYNAMIC_DRAW);
		// Make it available as binding point 0
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_uboRenderData);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);

		InitShadowMap();

		m_camera.reset(new Camera({-10.f, 10.f, 10.f}, -45.0f, -20.f));
		InitFrameBuffer(app_spec.width, app_spec.height);

		m_screen_quad.reset(new ScreenQuad());

		m_particle_renderer = std::make_unique<ParticleRenderer>();
		m_particle_renderer->Init();

		m_post_process = std::make_unique<PostProcess>();
		m_post_process->Init(m_frame_buffer.render_width, m_frame_buffer.render_height);

		m_fluid_renderer = std::make_unique<FluidRenderer>();
		m_fluid_renderer->Init(m_frame_buffer.render_width, m_frame_buffer.render_height);

		// Cloth mesh VAO/VBO/EBO — sized for up to ~16k cloth vertices total per frame.
		glGenVertexArrays(1, &m_cloth_vao);
		glBindVertexArray(m_cloth_vao);
		glGenBuffers(1, &m_cloth_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, m_cloth_vbo);
		glBufferData(GL_ARRAY_BUFFER, kMaxClothVerts * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
		                      (void*)(3 * sizeof(float)));
		glGenBuffers(1, &m_cloth_ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cloth_ebo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, kMaxClothIndices * sizeof(uint32_t),
		             nullptr, GL_DYNAMIC_DRAW);
		glBindVertexArray(0);
		m_cloth_shader = ShaderManager::GetOrCreateShader(
			RESOURCES_PATH "shaders/cloth.vert",
			RESOURCES_PATH "shaders/cloth.frag");

		glfwSetInputMode(m_window.get(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}

	unsigned int Renderer::GetFrameBufferTexture() const
	{
		if (m_post_process && m_post_process->GetOutputTexture()) {
			return m_post_process->GetOutputTexture();
		}
		return m_frame_buffer.texture;
	}

	void Renderer::UpdateRigidBodyVisualTransforms()
	{
		if (!m_physics_system) return;
		const auto& transforms = m_physics_system->GetRigidBodyTransforms();
		const auto& ids        = m_physics_system->GetRigidBodyIds();
		if (transforms.size() != ids.size()) return;

		// Build a quick id → index map.
		std::unordered_map<uint32_t, uint32_t> id_to_index;
		for (uint32_t i = 0; i < ids.size(); ++i) id_to_index[ids[i]] = i;

		auto view = m_registry.view<RigidBodyComponent, RigidBodyVisualComponent, TransformComponent>();
		for (auto e : view) {
			const auto& body = view.get<RigidBodyComponent>(e);
			auto it = id_to_index.find(body.rigidBodyId);
			if (it == id_to_index.end()) continue;
			const auto& vis = view.get<RigidBodyVisualComponent>(e);
			auto& xform = view.get<TransformComponent>(e);
			const auto& bt = transforms[it->second];
			// Centroid offset: where the source mesh's own centroid sat in local space; subtract
			// it so the rendered mesh's centroid lands on the physics centroid.
			xform.position    = bt.centroid - (bt.orientation * vis.centroidOffset * vis.meshScale);
			xform.orientation = bt.orientation;
			xform.scale       = vis.meshScale;
		}
	}

	void Renderer::SetPhysicsSystem(PhysicsSystem* physics)
	{
		m_physics_system = physics;
		m_particle_materials_dirty = true;
		if (m_physics_system) {
			m_physics_system->m_syncTransforms = (m_render_mode != RenderMode::ParticlesOnly);
			// Place the infinite grid right at the physics floor so they always line up.
			m_grid_y = m_physics_system->m_domain.min.y;
		}
	}

	void Renderer::SetRenderMode(RenderMode mode)
	{
		m_render_mode = mode;
		if (m_physics_system) {
			m_physics_system->m_syncTransforms = (mode != RenderMode::ParticlesOnly);
		}
	}

	void Renderer::RenderWorld(const float& delta_time)
	{
		BindWindowBuffer();

		UpdateRenderData();
		// Drive rigid-body visual transforms from the physics readback before any pass that
		// reads TransformComponent (shadow map, scene batch).
		UpdateRigidBodyVisualTransforms();
		if(!m_wireframe_mode) RenderShadowMap();

		BindFrameBuffer();
		if(!m_wireframe_mode) RenderFullScreenQuad();
		RenderSceneBatched();
		RenderInfiniteGrid();
		RenderClothMeshes();
		RenderParticles();

		// Screen-space fluid surface. In MeshesOnly / Both we draw the SSFR sheet; in
		// ParticlesOnly we skip so the user can see the raw simulation. The compose pass uses
		// the scene's HDR colour AND its depth texture for SSR + scene-depth refraction.
		if (m_fluid_renderer && m_physics_system && m_render_mode != RenderMode::ParticlesOnly) {
			m_fluid_renderer->Execute(
				m_physics_system->GetParticleVBO(),
				m_physics_system->GetFlagsVBO(),
				m_physics_system->GetParticleCount(),
				// 3.0x the physics radius. Heavy overlap = the depth pass fills almost every
				// pixel in a fluid region, leaving the smoothing pass with a near-continuous
				// surface to work on. At 2.4× post-dam-break splashes still show as discrete
				// spheres because the water is genuinely globular in places; 3.0× fills the
				// gaps before the bilateral runs.
				m_physics_system->m_particleRadius * 3.0f,
				m_camera->GetViewMatrix(), m_camera->GetProjectionMatrix(),
				glm::normalize(m_light_dir), m_light_color,
				m_frame_buffer.texture, m_frame_buffer.depth_texture,
				m_frame_buffer.render_width, m_frame_buffer.render_height,
				m_frame_buffer.frame_buffer,
				static_cast<float>(glfwGetTime()));
		}

		RenderDebug();

		// HDR -> LDR with bloom + tonemap. Writes to PostProcess's output FBO; UIManager reads
		// it via GetFrameBufferTexture().
		if (m_post_process) {
			m_post_process->Execute(m_frame_buffer.texture);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void Renderer::RenderInfiniteGrid()
	{
		auto shader = ShaderManager::GetOrCreateShader(
			RESOURCES_PATH "shaders/post.vert",
			RESOURCES_PATH "shaders/grid.frag");
		if (!shader) return;

		// Lazy-init a fullscreen quad — same vertex layout as post.vert expects.
		static GLuint s_vao = 0, s_vbo = 0;
		if (s_vao == 0) {
			const float verts[] = {
				-1.f, -1.f, 0.f, 0.f,   1.f, -1.f, 1.f, 0.f,   1.f,  1.f, 1.f, 1.f,
				-1.f, -1.f, 0.f, 0.f,   1.f,  1.f, 1.f, 1.f,  -1.f,  1.f, 0.f, 1.f,
			};
			glGenVertexArrays(1, &s_vao);
			glBindVertexArray(s_vao);
			glGenBuffers(1, &s_vbo);
			glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
			glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
				(void*)(2 * sizeof(float)));
			glBindVertexArray(0);
		}

		glm::mat4 vp     = m_camera->GetProjectionMatrix() * m_camera->GetViewMatrix();
		glm::mat4 inv_vp = glm::inverse(vp);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		shader->Bind();
		shader->SetUniformMat4("u_inv_view_proj", inv_vp);
		shader->SetUniformMat4("u_view_proj",     vp);
		shader->SetUniformVec3("u_view_pos",      m_camera->GetPosition());
		shader->SetUniform1f("u_grid_y",          m_grid_y);
		shader->SetUniform1f("u_minor_spacing",   m_grid_minor_spacing);
		shader->SetUniform1f("u_major_every",     m_grid_major_every);
		shader->SetUniformVec3("u_color_minor",   m_grid_color_minor);
		shader->SetUniformVec3("u_color_major",   m_grid_color_major);
		shader->SetUniformVec3("u_color_base",    m_grid_color_base);
		shader->SetUniform1f("u_fade_distance",   m_grid_fade_distance);

		// Shadows for the floor
		const glm::mat4 light_space_matrix = m_shadow_map.light_projection * m_shadow_map.light_view;
		shader->SetUniformMat4("u_light_space_matrix", light_space_matrix);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_shadow_map.texture);
		shader->SetUniform1i("shadow_map", 0);

		glBindVertexArray(s_vao);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		Shader::Unbind();
		glDisable(GL_BLEND);
	}

	void Renderer::RenderClothMeshes()
	{
		// Cloth meshes draw in MeshesOnly and Both modes; in ParticlesOnly the impostor renderer
		// shows the cloth particles as spheres.
		if (!m_cloth_shader) return;
		if (m_render_mode == RenderMode::ParticlesOnly) return;

		auto cview = m_registry.view<ClothComponent>();
		if (cview.begin() == cview.end()) return;

		// Build a single triangle soup from every cloth in the scene.
		std::vector<float>    verts;   // pos.xyz, normal.xyz interleaved
		std::vector<uint32_t> idx;
		std::vector<glm::vec4> per_cloth_color; // one entry per cloth, mapped to its index range
		std::vector<uint32_t>  per_cloth_start; // start index in `idx`

		auto pos_of = [&](entt::entity e) -> glm::vec3 {
			const auto* tc = m_registry.try_get<TransformComponent>(e);
			return tc ? tc->position : glm::vec3(0.0f);
		};

		for (auto e : cview) {
			const auto& c = cview.get<ClothComponent>(e);
			if (c.nu < 2 || c.nv < 2 || (int)c.particles.size() != c.nu * c.nv) continue;

			per_cloth_color.push_back(c.color);
			per_cloth_start.push_back(static_cast<uint32_t>(idx.size()));

			const uint32_t base = static_cast<uint32_t>(verts.size() / 6);
			verts.resize(verts.size() + size_t(c.nu * c.nv) * 6);

			// Per-vertex normal from neighbour-finite-differences on the grid. Boundary cells
			// fall back to the inside neighbour so the normal is still well-defined.
			for (int j = 0; j < c.nv; ++j)
			for (int i = 0; i < c.nu; ++i) {
				int k = j * c.nu + i;
				const int ip = std::min(i + 1, c.nu - 1);
				const int im = std::max(i - 1, 0);
				const int jp = std::min(j + 1, c.nv - 1);
				const int jm = std::max(j - 1, 0);
				glm::vec3 p  = pos_of(c.particles[k]);
				glm::vec3 du = pos_of(c.particles[j * c.nu + ip]) - pos_of(c.particles[j * c.nu + im]);
				glm::vec3 dv = pos_of(c.particles[jp * c.nu + i]) - pos_of(c.particles[jm * c.nu + i]);
				glm::vec3 n = glm::cross(du, dv);
				float nlen = glm::length(n);
				if (nlen > 1e-6f) n /= nlen;
				else              n = glm::vec3(0.0f, 1.0f, 0.0f);
				size_t off = size_t(base + k) * 6;
				verts[off + 0] = p.x; verts[off + 1] = p.y; verts[off + 2] = p.z;
				verts[off + 3] = n.x; verts[off + 4] = n.y; verts[off + 5] = n.z;
			}

			for (int j = 0; j < c.nv - 1; ++j)
			for (int i = 0; i < c.nu - 1; ++i) {
				uint32_t a = base + j * c.nu + i;
				uint32_t b = base + j * c.nu + i + 1;
				uint32_t cc = base + (j + 1) * c.nu + i;
				uint32_t d = base + (j + 1) * c.nu + i + 1;
				idx.push_back(a); idx.push_back(cc); idx.push_back(b);
				idx.push_back(b); idx.push_back(cc); idx.push_back(d);
			}
		}

		if (verts.empty() || idx.empty()) return;
		if (verts.size() > kMaxClothVerts * 6) return;
		if (idx.size()   > kMaxClothIndices)   return;

		glBindBuffer(GL_ARRAY_BUFFER, m_cloth_vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(float), verts.data());
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cloth_ebo);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, idx.size() * sizeof(uint32_t), idx.data());

		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);            // cloth is two-sided
		glDepthFunc(GL_LEQUAL);

		m_cloth_shader->Bind();
		m_cloth_shader->SetUniformMat4("u_view", m_camera->GetViewMatrix());
		m_cloth_shader->SetUniformMat4("u_proj", m_camera->GetProjectionMatrix());
		m_cloth_shader->SetUniformVec3("u_light_dir", glm::normalize(m_light_dir));
		m_cloth_shader->SetUniformVec3("u_view_pos",  m_camera->GetPosition());

		glBindVertexArray(m_cloth_vao);
		// One draw per cloth so each can have its own colour.
		for (size_t c = 0; c < per_cloth_color.size(); ++c) {
			uint32_t start = per_cloth_start[c];
			uint32_t end   = (c + 1 < per_cloth_color.size()) ? per_cloth_start[c + 1]
			                                                  : static_cast<uint32_t>(idx.size());
			GLuint loc_col = glGetUniformLocation(m_cloth_shader->GetProgramID(), "u_color");
			glUniform4fv(loc_col, 1, &per_cloth_color[c].x);
			glDrawElements(GL_TRIANGLES, GLsizei(end - start), GL_UNSIGNED_INT,
			               (void*)(uintptr_t)(start * sizeof(uint32_t)));
		}
		glBindVertexArray(0);
		Shader::Unbind();
		glEnable(GL_CULL_FACE);
	}

	void Renderer::RenderParticles()
	{
		if (!m_physics_system || !m_particle_renderer) return;
		if (m_physics_system->GetParticleCount() == 0) return;

		if (m_particle_materials_dirty) {
			m_particle_renderer->UpdateMaterials(m_physics_system->GetHostMaterials());
			m_particle_materials_dirty = false;
		}

		// Particle-rendering responsibilities by mode:
		//   Particles : show everything as voxel spheres.
		//   Meshes    : rigid bodies handled by the mesh pipeline; fluids by SSFR; the impostor
		//               still draws cloth/rope (non-rigid, non-fluid) so those remain visible.
		//   Both      : show absolutely everything, including under-the-hood voxels of rigids.
		const bool hide_rigid = (m_render_mode == RenderMode::MeshesOnly);
		const bool hide_fluid = (m_render_mode == RenderMode::MeshesOnly);
		const bool hide_cloth = (m_render_mode == RenderMode::MeshesOnly);

		m_particle_renderer->Render(
			m_physics_system->GetParticleVBO(),
			m_physics_system->GetMaterialIdVBO(),
			m_physics_system->GetFlagsVBO(),
			m_physics_system->GetParticleCount(),
			m_camera->GetViewMatrix(), m_camera->GetProjectionMatrix(),
			m_camera->GetPosition(), glm::normalize(m_light_dir),
			m_physics_system->m_particleRadius, hide_rigid, hide_fluid, hide_cloth);
	}

	void Renderer::RenderDebug() const
	{
		// Domain box outline so the simulation bounds are always visible.
		if (m_physics_system) {
			glm::vec3 mn   = m_physics_system->m_domain.min;
			glm::vec3 mx   = m_physics_system->m_domain.max;
			glm::vec3 sz   = mx - mn;
			glm::vec3 ctr  = 0.5f * (mn + mx);
			DebugRenderer::DrawAABB(ctr, sz, glm::vec3(0.35f, 0.55f, 0.75f));
		}

		// Rope wireframe: in mesh modes, render any distance-constraint pair where *neither*
		// endpoint is a cloth particle (those are drawn as a proper triangle mesh by
		// RenderClothMeshes). This keeps the rope visible as a single coloured chain.
		if (m_physics_system &&
		    (m_render_mode == RenderMode::MeshesOnly || m_render_mode == RenderMode::Both))
		{
			auto cview = m_registry.view<DistanceConstraintComponent>();
			for (auto e : cview) {
				const auto& c = cview.get<DistanceConstraintComponent>(e);
				const auto* ta = m_registry.try_get<TransformComponent>(c.entityA);
				const auto* tb = m_registry.try_get<TransformComponent>(c.entityB);
				if (!ta || !tb) continue;
				bool a_cloth = false, b_cloth = false;
				if (auto* m = m_registry.try_get<PhysicsMaterialComponent>(c.entityA))
					a_cloth = (m->material.phase == static_cast<uint32_t>(ParticlePhase::Cloth));
				if (auto* m = m_registry.try_get<PhysicsMaterialComponent>(c.entityB))
					b_cloth = (m->material.phase == static_cast<uint32_t>(ParticlePhase::Cloth));
				if (a_cloth || b_cloth) continue;
				glm::vec3 col(0.85f, 0.75f, 0.30f);
				if (const auto* m = m_registry.try_get<PhysicsMaterialComponent>(c.entityA)) {
					col = glm::vec3(m->material.color);
				}
				DebugRenderer::DrawLine(ta->position, tb->position, col);
			}
		}

		// Picker spring visual.
		if (m_physics_system && m_physics_system->HasPickedParticle()) {
			glm::vec3 p   = m_physics_system->GetPickedParticleWorldPos();
			glm::vec3 tgt = m_physics_system->GetPickTarget();
			DebugRenderer::DrawLine(p, tgt, glm::vec3(1.0f, 0.85f, 0.20f));
			DebugRenderer::DrawAABB(p,   glm::vec3(m_physics_system->m_particleRadius * 3.0f),
									 glm::vec3(1.0f, 0.85f, 0.20f));
			DebugRenderer::DrawAABB(tgt, glm::vec3(m_physics_system->m_particleRadius * 2.0f),
									 glm::vec3(1.0f, 1.0f, 1.0f));
		}
		DebugRenderer::Flush();
	}

	void Renderer::RequestViewportFocus()
	{
		m_requestFocus = true;
	}


	void Renderer::InitOpenGLContext(const AppSpecification& app_spec)
	{
#if defined(__linux__) || defined(__gnu_linux__)
		// Force Linux Optimus/PRIME to use Dedicated GPU (NVIDIA / AMD)
		::setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 0);
		::setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 0);
		::setenv("DRI_PRIME", "1", 0);
#endif

		if (!glfwInit()) {
			Log(LogLevel::Fatal, "GLFW failed to initialise");
			exit(EXIT_FAILURE);
		}
		else
		{
			Log(LogLevel::Info, "GLFW initialised");
		}

		//using OpenGL 4.6
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		//TODO: Remove from release build?
		glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

		if(app_spec.fullscreen)
		{
			m_window.reset(glfwCreateWindow(app_spec.width, app_spec.height, app_spec.name.c_str(), glfwGetPrimaryMonitor(), nullptr));
		}
		else
		{
			m_window.reset(glfwCreateWindow(app_spec.width, app_spec.height, app_spec.name.c_str(), nullptr, nullptr));
		}
		
		if (!m_window)
		{
			Log(LogLevel::Fatal, "GLFW failed to create window");
			glfwTerminate();
			exit(EXIT_FAILURE);
		}
		Log(LogLevel::Info, "GLFW window created");
		
		//set callbacks
		glfwSetErrorCallback([](const int error, const char* description)
		{
			const std::string error_string = std::to_string(error) + ":" + description;
			Log(LogLevel::Error, error_string);
		});


		glfwMakeContextCurrent(m_window.get());

		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
		{
			std::cout << "Failed to initialize GLAD" << std::endl;
		}

		glViewport(0, 0, app_spec.width, app_spec.height);

		glEnable(GL_DEBUG_OUTPUT);

		int flags;
		glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
		if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
			Log(LogLevel::Info, "OpenGL debug context enabled");
		}

		if(app_spec.vsync)
		{
			glfwSwapInterval(1); // Enable VSync
		}
		else
		{
			glfwSwapInterval(0); // Disable VSync
		}

		Texture::InitDefaults();

	}

	void Renderer::LogRendererInfo()
	{
		Log(LogLevel::Info, std::format("Running GLFW {}", glfwGetVersionString()));
		Log(LogLevel::Info, std::format("Running OpenGL {}", reinterpret_cast<const char*>(glGetString(GL_VERSION))));
		Log(LogLevel::Info, std::format("Running GLSL {}", reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION))));
		Log(LogLevel::Info, std::format("Using GPU: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER))));
		Log(LogLevel::Info, "Renderer Initialised\n");
	}

	void Renderer::CheckFrameBufferStatus()
	{
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		switch (status)
		{
			case GL_FRAMEBUFFER_COMPLETE:
				Log(LogLevel::Info, "Framebuffer is complete.");
			break;
			case GL_FRAMEBUFFER_UNDEFINED:
				Log(LogLevel::Error, "GL_FRAMEBUFFER_UNDEFINED: The specified framebuffer is the default read or draw framebuffer, but the default framebuffer does not exist.");
			break;
			case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
				Log(LogLevel::Error, "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: One or more framebuffer attachment points are incomplete.");
			break;
			case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
				Log(LogLevel::Error, "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: The framebuffer does not have at least one image attached.");
			break;
			case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
				Log(LogLevel::Error, "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER: The value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE for one or more color attachment points.");
			break;
			case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
				Log(LogLevel::Error, "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER: The value of GL_READ_BUFFER is not GL_NONE, and the value of GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is GL_NONE for the color attachment point.");
			break;
			case GL_FRAMEBUFFER_UNSUPPORTED:
				Log(LogLevel::Error, "GL_FRAMEBUFFER_UNSUPPORTED: The combination of internal formats of the attached images violates an implementation-dependent set of restrictions.");
			break;
			case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
				Log(LogLevel::Error, "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE: The number of samples for all attachments is not the same.");
			break;
			case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
				Log(LogLevel::Error, "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS: A framebuffer attachment is layered, and a populated attachment is not layered.");
			break;
			default:
				Log(LogLevel::Error, "Unknown framebuffer error.");
		}
	}

	void Renderer::InitShadowMap()
	{
		// Generate and configure the shadow map framebuffer
		glGenFramebuffers(1, &m_shadow_map.fbo);

		// Create the depth texture
		glGenTextures(1, &m_shadow_map.texture);
		glBindTexture(GL_TEXTURE_2D, m_shadow_map.texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F,
			m_shadow_map.shadow_width, m_shadow_map.shadow_height, 0, GL_DEPTH_COMPONENT,
			GL_FLOAT, nullptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// enable GLSL sampler2DShadow-style comparisons
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

		constexpr float border_color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

		glBindFramebuffer(GL_FRAMEBUFFER, m_shadow_map.fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadow_map.texture, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			Log(LogLevel::Error, "Shadow map framebuffer is incomplete!");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void Renderer::InitFrameBuffer(const int& width, const int& height)
	{
		if (width <= 0 || height <= 0) {
			Log(LogLevel::Error, "Invalid framebuffer dimensions");
			return;
		}

		// Cleanup existing framebuffer
		if (m_frame_buffer.frame_buffer) glDeleteFramebuffers(1, &m_frame_buffer.frame_buffer);
		if (m_frame_buffer.texture) glDeleteTextures(1, &m_frame_buffer.texture);
		if (m_frame_buffer.depth_texture) glDeleteTextures(1, &m_frame_buffer.depth_texture);
		if (m_frame_buffer.depth_render_buffer) glDeleteRenderbuffers(1, &m_frame_buffer.depth_render_buffer);
		m_frame_buffer.depth_render_buffer = 0;

		// Create framebuffer
		glGenFramebuffers(1, &m_frame_buffer.frame_buffer);
		glBindFramebuffer(GL_FRAMEBUFFER, m_frame_buffer.frame_buffer);

		// HDR scene buffer (RGB16F) — bloom needs values above 1.0 to survive the threshold.
		glGenTextures(1, &m_frame_buffer.texture);
		glBindTexture(GL_TEXTURE_2D, m_frame_buffer.texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_frame_buffer.texture, 0);

		// Depth as a sampleable texture (was a renderbuffer). SSFR refraction and the foam
		// classifier read this; without a real texture we'd have to do a second forward pass
		// or copy-blit every frame. GL_DEPTH_COMPONENT32F gives us enough precision for far
		// view-space reconstruction without the GL_DEPTH24_STENCIL8 stencil baggage.
		glGenTextures(1, &m_frame_buffer.depth_texture);
		glBindTexture(GL_TEXTURE_2D, m_frame_buffer.depth_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0,
		             GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
		                       m_frame_buffer.depth_texture, 0);

		// Check framebuffer completeness
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			CheckFrameBufferStatus(); // Logs detailed error
		} else {
			//Log(LogLevel::Info, "Framebuffer initialized successfully.");
		}

		m_camera->SetAspectRatio(static_cast<float>(m_frame_buffer.render_width)/static_cast<float>(m_frame_buffer.render_height));

		glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind framebuffer
	}

	void Renderer::BindFrameBuffer() const
	{
		// Bind framebuffer for rendering
		glBindFramebuffer(GL_FRAMEBUFFER, m_frame_buffer.frame_buffer);
		glViewport(0, 0, m_frame_buffer.render_width, m_frame_buffer.render_height); // Match viewport size to framebuffer
		glClearColor(0.f, 0.f, 0.f, 1.0f); // Sky blue color
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
	}

	void Renderer::BindWindowBuffer() const
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind framebuffer
		int width{0}, height{0};
		glfwGetFramebufferSize(m_window.get(), &width, &height);
		glViewport(0, 0, width, height); // Match viewport size to framebuffer
		glClearColor(0.08f, 0.10f, 0.12f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void Renderer::RenderShadowMap()
	{
	    glBindFramebuffer(GL_FRAMEBUFFER, m_shadow_map.fbo);
	    glViewport(0, 0, m_shadow_map.shadow_width, m_shadow_map.shadow_height);
	    glClear(GL_DEPTH_BUFFER_BIT);

	    glEnable(GL_DEPTH_TEST);
	    glEnable(GL_POLYGON_OFFSET_FILL);
	    glPolygonOffset(2.0f, 4.0f);
	    glCullFace(GL_FRONT);
	    glEnable(GL_CULL_FACE);
	    glDrawBuffer(GL_NONE);

		// Compute the “scene box” we want to shadow:
		const float R = 50.0f;  // adjust to cover your scene
		glm::vec3 center = glm::vec3(0.0f);


		// Get your light’s direction (unit vector).
		glm::vec3 dir = glm::normalize(m_light_dir);

		// Place the shadow‐camera “behind” the box along -dir at distance R.
		glm::vec3 shadowCamPos = center - dir * R;

		// Build view/proj
		m_shadow_map.light_view       = glm::lookAt(shadowCamPos, center, {0,1,0});
		m_shadow_map.light_projection = glm::ortho(-R, R, R, -R, 0.1f, 2.0f*R);

	    // bind shadow shader
	    auto shadow_shader = ShaderManager::GetOrCreateShader(
	        RESOURCES_PATH "shaders/shadow.vert",
	        RESOURCES_PATH "shaders/shadow.frag"
	    );
	    shadow_shader->Bind();
	    shadow_shader->SetUniformMat4("light_view",       m_shadow_map.light_view);
	    shadow_shader->SetUniformMat4("light_projection", m_shadow_map.light_projection);

	    // --- gather all mesh+transform items ---
	    struct Item { Mesh* mesh; glm::mat4 model; };
	    std::vector<Item> items;

	    // Particles never go through the shadow pass (too expensive and visually negligible).
	    // Rigid-body visual meshes participate so they cast proper shadows.
	    for (auto e : m_registry.view<TransformComponent, MeshComponent>()) {
	        if (m_registry.any_of<ParticleComponent>(e)) continue;
	        auto &tc = m_registry.get<TransformComponent>(e);
	        auto &mc = m_registry.get<MeshComponent>(e);
	        items.push_back({ mc.mesh.get(), tc.GetMatrix() });
	    }
	    for (auto e : m_registry.view<TransformComponent, ModelComponent>()) {
	        if (m_registry.any_of<ParticleComponent>(e)) continue;
	        auto &tc  = m_registry.get<TransformComponent>(e);
	        auto &mdc = m_registry.get<ModelComponent>(e);
	        for (auto &sub : mdc.model->GetMeshes())
	            items.push_back({ sub.get(), tc.GetMatrix() });
	    }

	    if (items.empty()) {
	        glCullFace(GL_BACK);
	        glDisable(GL_CULL_FACE);
	        glDisable(GL_POLYGON_OFFSET_FILL);
	        glBindFramebuffer(GL_FRAMEBUFFER, 0);
	        return;
	    }

	    // --- sort and draw instanced per mesh ---
	    std::sort(items.begin(), items.end(),
	              [](auto const &a, auto const &b){ return a.mesh < b.mesh; });

	    size_t idx = 0;
	    while (idx < items.size()) {
	        Mesh* mesh = items[idx].mesh;

	        // collect all models for this mesh
	        std::vector<glm::mat4> models;
	        size_t j = idx;
	        for (; j < items.size() && items[j].mesh == mesh; ++j)
	            models.push_back(items[j].model);

	        // upload per-instance buffer
	        glBindBuffer(GL_ARRAY_BUFFER, mesh->instanceVBO);
	        glBufferData(GL_ARRAY_BUFFER,
	                     models.size() * sizeof(glm::mat4),
	                     models.data(),
	                     GL_DYNAMIC_DRAW);
	        glBindBuffer(GL_ARRAY_BUFFER, 0);

	        // single instanced draw
	        glBindVertexArray(mesh->VAO);
	        glDrawElementsInstanced(
	            GL_TRIANGLES,
	            mesh->indexCount,
	            GL_UNSIGNED_INT,
	            nullptr,
	            static_cast<GLsizei>(models.size())
	        );
	        glBindVertexArray(0);

	        idx = j;
	    }

	    Shader::Unbind();
	    glCullFace(GL_BACK);
	    glDisable(GL_CULL_FACE);
	    glDisable(GL_POLYGON_OFFSET_FILL);
	    glBindFramebuffer(GL_FRAMEBUFFER, 0);

	    // restore viewport to window
	    int w, h;
	    glfwGetFramebufferSize(m_window.get(), &w, &h);
	    glViewport(0, 0, w, h);
	}

	void Renderer::RenderFullScreenQuad() const
	{
		glDisable(GL_DEPTH_TEST);

		// Use the gradient shader
		auto gradientShader = ShaderManager::GetOrCreateShader(
			RESOURCES_PATH "shaders/gradient.vert",
			RESOURCES_PATH "shaders/gradient.frag"
		);
		gradientShader->Bind();

		// upload inverse matrices
		glm::mat4 invProj = glm::inverse(m_camera->GetProjectionMatrix());
		glm::mat4 invView = glm::inverse(m_camera->GetViewMatrix());
		gradientShader->SetUniformMat4("inverseProjection", invProj);
		gradientShader->SetUniformMat4("invView", invView);

		// upload sun dir & colors
		gradientShader->SetUniformVec3("light_dir", glm::normalize(m_light_dir));
		// Linear-space sky colours — darker than typical sRGB values so the post pipeline (ACES
		// + sRGB encode) can lift them to a natural-looking blue rather than white.
		gradientShader->SetUniformVec3("topColor",     glm::vec3(0.10f, 0.18f, 0.32f));
		gradientShader->SetUniformVec3("bottomColor",  glm::vec3(0.40f, 0.55f, 0.70f));
		gradientShader->SetUniform1f("mieG", 0.8f);
		gradientShader->SetUniform1f("u_time", static_cast<float>(glfwGetTime()));

		glBindVertexArray(m_screen_quad.get()->vao);
		glDrawArrays(GL_TRIANGLES, 0, 6); // Draw the quad as two triangles
		glBindVertexArray(0);

		Shader::Unbind();

		glEnable(GL_DEPTH_TEST);
	}

	void Renderer::RenderScene() const
	{
		glm::mat4 lightSpace = m_shadow_map.light_projection * m_shadow_map.light_view;
		const bool skip_particles = (m_render_mode == RenderMode::ParticlesOnly);

		for (auto e : m_registry.view<TransformComponent, MeshComponent>()) {
			if (skip_particles && m_registry.any_of<ParticleComponent>(e)) continue;
			auto &tc = m_registry.get<TransformComponent>(e);
			auto &mc = m_registry.get<MeshComponent>(e);
			auto &mat= m_registry.get<MaterialComponent>(e);

			mat.material->Apply();

			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, m_shadow_map.texture);
			mat.material->shader->SetUniformMat4("model", tc.GetMatrix());
			mat.material->shader->SetUniformMat4("light_space_matrix", lightSpace);
			mat.material->shader->SetUniform1i("should_shade", 1);
			mat.material->shader->SetUniform1i("shadow_map", 4);

			mc.mesh->Draw();
		}

		for (auto e : m_registry.view<TransformComponent, ModelComponent>()) {
			if (skip_particles && m_registry.any_of<ParticleComponent>(e)) continue;
			auto &tc = m_registry.get<TransformComponent>(e);
			auto &mc = m_registry.get<ModelComponent>(e);
			auto &mat= m_registry.get<MaterialComponent>(e);

			mat.material->Apply();

			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, m_shadow_map.texture);
			mat.material->shader->SetUniformMat4("model", tc.GetMatrix());
			mat.material->shader->SetUniformMat4("light_space_matrix", lightSpace);
			mat.material->shader->SetUniform1i("should_shade", 1);
			mat.material->shader->SetUniform1i("shadow_map", 4);

			mc.model->Draw();
		}

	}

	void Renderer::RenderSceneBatched() const {
		glm::mat4 lightSpace = m_shadow_map.light_projection * m_shadow_map.light_view;

		struct Item { Material* mat; Mesh* mesh; glm::mat4 model; };
		std::vector<Item> items;

		// Skip per-entity sphere meshes whenever the impostor renderer (or SSFR) is the
		// authoritative pipeline for particles. Only "Both" mode draws everything twice —
		// it's the explicit debug view.
		const bool skip_particles      = (m_render_mode != RenderMode::Both);
		// Rigid-body visual meshes draw whenever we're in mesh-aware modes (Meshes / Both),
		// but not in Particles-only (we want to see the raw voxels there).
		const bool skip_rigid_visuals  = (m_render_mode == RenderMode::ParticlesOnly);

		auto include = [&](entt::entity e) {
			if (skip_particles && m_registry.any_of<ParticleComponent>(e)) return false;
			if (skip_rigid_visuals && m_registry.any_of<RigidBodyVisualComponent>(e)) return false;
			return true;
		};

		// Gather MeshComponents
		{
			auto view = m_registry.view<TransformComponent, MeshComponent, MaterialComponent>();
			items.reserve(std::distance(view.begin(), view.end()));
			for (auto e : view) {
				if (!include(e)) continue;
				auto& tc  = m_registry.get<TransformComponent>(e);
				auto& mc  = m_registry.get<MeshComponent>(e);
				auto& mat = m_registry.get<MaterialComponent>(e);
				items.push_back({ mat.material.get(),
								  mc.mesh.get(),
								  tc.GetMatrix() });
			}
		}

		// Gather ModelComponents (each model may have multiple sub-meshes)
		{
			auto view = m_registry.view<TransformComponent, ModelComponent, MaterialComponent>();
			for (auto e : view) {
				if (!include(e)) continue;
				auto& tc   = m_registry.get<TransformComponent>(e);
				auto& mdc  = m_registry.get<ModelComponent>(e);
				auto& mat  = m_registry.get<MaterialComponent>(e);
				for (auto& submesh : mdc.model->GetMeshes()) {
					items.push_back({ mat.material.get(),
									  submesh.get(),
									  tc.GetMatrix() });
				}
			}
		}

		if (items.empty()) {
			// nothing to draw
			return;
		}

		// Sort by material, then mesh
		std::sort(items.begin(), items.end(), [](auto const &a, auto const &b) {
			if (a.mat  != b.mat)  return a.mat  < b.mat;
			return   a.mesh < b.mesh;
		});


		size_t idx = 0;
		while (idx < items.size()) {
			auto mat  = items[idx].mat;
			auto mesh = items[idx].mesh;

			// collect per-instance matrices
			std::vector<glm::mat4> models;
			size_t j = idx;
			for (; j < items.size() && items[j].mat == mat && items[j].mesh == mesh; ++j)
				models.push_back(items[j].model);
			GLsizei instanceCount = GLsizei(models.size());

			// upload instance‐models
			glBindBuffer(GL_ARRAY_BUFFER, mesh->instanceVBO);
			glBufferData(GL_ARRAY_BUFFER,
						 instanceCount * sizeof(glm::mat4),
						 models.data(),
						 GL_DYNAMIC_DRAW);

			// set up material + PBR maps
			mat->Apply();

			// set per‐draw uniforms
			auto s = mat->shader.get();
			s->SetUniformMat4("light_space_matrix", lightSpace);
			s->SetUniform1i("should_shade",        1);

			// bind shadow map
			glActiveTexture(GL_TEXTURE5);
			glBindTexture(GL_TEXTURE_2D, m_shadow_map.texture);
			s->SetUniform1i("shadow_map", 5);

			// draw instanced
			glBindVertexArray(mesh->VAO);
			glDrawElementsInstanced(
				GL_TRIANGLES,
				mesh->indexCount,
				GL_UNSIGNED_INT,
				nullptr,
				instanceCount
			);
			glBindVertexArray(0);

			idx = j;
		}

		Shader::Unbind();
	}

	GLFWwindow* Renderer::GetWindow() const
	{
		return m_window.get();
	}

	Camera* Renderer::GetCamera() const
	{
		return m_camera.get();
	}

	void Renderer::UpdateRenderData()
	{
		//if(m_render_data == m_old_render_data) return;

		m_old_render_data = m_render_data;

		// Update view and projection matrices from the camera
		m_render_data.view = m_camera->GetViewMatrix();          // View matrix from the camera
		m_render_data.projection = m_camera->GetProjectionMatrix(); // Projection matrix from the camera

		// Update the camera's position
		m_render_data.view_pos = m_camera->GetPosition();

		// Padding explicitly set to 0 (required for std140 uniform alignment)
		m_render_data.padding1 = 0.0f;

		// Update the directional‐light direction and color
		m_render_data.light_dir   = glm::normalize(m_light_dir);
		m_render_data.padding2    = 0.0f;             // explicit zero for std140 padding
		m_render_data.light_color = m_light_color;
		m_render_data.padding3    = 0.0f;

		// Update wireframe mode (bool is treated as 4-byte int in std140)
		m_render_data.wireframe = m_wireframe_mode;

		m_render_data.padding4[0]  = m_render_data.padding4[1] = m_render_data.padding4[2] = 0.0f;

		glBindBuffer(GL_UNIFORM_BUFFER, m_uboRenderData);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(RenderData), &m_render_data);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	void Renderer::ResizeFrameBuffer(float width, float height)
	{
		int newWidth = static_cast<int>(width);
		int newHeight = static_cast<int>(height);

		if (newWidth > 0 && newHeight > 0 &&
			(newWidth != m_frame_buffer.render_width || newHeight != m_frame_buffer.render_height))
		{
			m_frame_buffer.render_width = newWidth;
			m_frame_buffer.render_height = newHeight;
			InitFrameBuffer(m_frame_buffer.render_width, m_frame_buffer.render_height);
			if (m_post_process)   m_post_process->Resize(newWidth, newHeight);
			if (m_fluid_renderer) m_fluid_renderer->Resize(newWidth, newHeight);
		}
	}

	void Renderer::SetLightDir(const glm::vec3 &dir)
	{
		m_light_dir = dir;
		m_render_data.light_dir = dir;
	}
}
