// Hex
#include "HexForge/pch.h"
#include "HexForge/Core/Application.h"
#include "HexForge/Core/Logger.h"
#include "HexForge/Core/Console.h"
#include "HexForge/Core/UIManager.h"
#include "HexForge/Renderer/Renderer.h"
#include "HexForge/Gameplay/EntityManager.h"
#include "HexForge/Gameplay/InputManager.h"
#include "HexForge/Physics/PhysicsSystem.h"
#include "HexForge/Renderer/Camera.h"
#include "HexForge/Core/Profiler.h"
#include "HexForge/Core/CudaManager.h"
#include "HexForge/Renderer/DebugRenderer.h"

namespace Hex
{
	Application::Application(const AppSpecification& application_spec, const SceneBuilder& scene_builder)
	: m_sceneBuilder(std::move(scene_builder)) {
		Init(application_spec);
	}

	Application::~Application()
	{
		DebugRenderer::Shutdown();
	}

	std::unique_ptr<InputManager>& Application::GetInputManager()
	{
		return m_input_manager;
	}

	void Application::Close()
	{
		m_running = false;
	}

	void Application::SetGameplayMode(bool enabled)
	{
		m_gameplayMode = enabled;
		ImGuiIO& io = ImGui::GetIO(); // Get ImGui's IO object

		if (m_gameplayMode)
		{
			// --- ENTER Gameplay Mode ---
			// 1. Tell GLFW to hide and lock the cursor. This enables raw mouse motion.
			glfwSetInputMode(m_renderer->GetWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

			// 2. IMPORTANT: Tell ImGui to stop processing mouse input.
			// This prevents ImGui from trying to set the cursor icon, which solves the conflict.
			io.ConfigFlags |= ImGuiConfigFlags_NoMouse;

			// 3. Reset the camera's internal mouse state to prevent a sudden jump.
			m_renderer->GetCamera()->ResetMouse();

			m_renderer->RequestViewportFocus();
		}
		else
		{
			// --- ENTER UI Mode ---
			// 1. Tell GLFW to show and unlock the cursor.
			glfwSetInputMode(m_renderer->GetWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

			// 2. IMPORTANT: Tell ImGui to resume processing mouse input.
			// The '& ~' operation is a bitwise NOT that removes the flag.
			io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
		}
	}

	bool Application::IsInGameplayMode() const
	{
		return m_gameplayMode;
	}

	void Application::ProcessMousePicking()
	{
		auto [ray_origin, ray_dir] = GetMouseRay();

		// Left-click: pick a particle. While held, drag it via a critically-damped spring.
		// Release: drop. The "target depth" stays at the particle's pick-time distance so the
		// mouse moves on a camera-facing plane (no Y bias).
		const bool lmb_down  = m_input_manager->IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
		const bool lmb_press = m_input_manager->IsMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT);

		static float s_pick_depth = 5.0f; // distance from camera to picked particle at grab time

		if (lmb_press) {
			// Snap to the nearest particle within ~0.5 m of the ray.
			uint32_t hit = m_physics_system->PickParticleByRay(ray_origin, ray_dir, 0.5f);
			if (hit != 0xFFFFFFFFu) {
				glm::vec3 wp = m_physics_system->GetPickedParticleWorldPos();
				s_pick_depth = glm::length(wp - ray_origin);
			}
		}

		if (m_physics_system->HasPickedParticle() && lmb_down) {
			// Drag target: point along the cursor ray at the original pick depth.
			glm::vec3 target = ray_origin + glm::normalize(ray_dir) * s_pick_depth;
			m_physics_system->SetPickTarget(target);
		}
		if (!lmb_down) {
			m_physics_system->ReleasePick();
		}
	}

	std::pair<glm::vec3, glm::vec3> Application::GetMouseRay()
	{
		// We must compare cursor and viewport-rect in the SAME coordinate frame. ImGui's
		// multi-viewport mode places ImGui windows in OS-screen coordinates, while GLFW's
		// cursor pos is in GLFW-window content-area coordinates — those differ as soon as
		// the user docks the viewport into a separate OS window. Take both from ImGui so we
		// always agree.
		ImVec2 mouse_im = ImGui::GetMousePos();
		glm::vec2 cursorPos(mouse_im.x, mouse_im.y);

		glm::vec2 vp_origin = m_ui_manager ? m_ui_manager->GetViewportScreenPos()
		                                   : glm::vec2(0.0f);
		glm::vec2 vp_size   = m_ui_manager ? m_ui_manager->GetViewportScreenSize()
		                                   : glm::vec2(float(m_specification.width),
		                                               float(m_specification.height));
		if (vp_size.x <= 1.0f || vp_size.y <= 1.0f) {
			vp_origin = glm::vec2(0.0f);
			vp_size   = glm::vec2(float(m_specification.width), float(m_specification.height));
		}

		glm::vec2 local = cursorPos - vp_origin;
		float ndcX = (2.0f * local.x) / vp_size.x - 1.0f;
		float ndcY = 1.0f - (2.0f * local.y) / vp_size.y;
		float ndcZ = -1.0f;
		glm::vec4 ray_ndc_start = glm::vec4(ndcX, ndcY, ndcZ, 1.0f);

		// Get the inverse projection and view matrices
		glm::mat4 proj = m_renderer->GetCamera()->GetProjectionMatrix();
		glm::mat4 view = m_renderer->GetCamera()->GetViewMatrix();
		glm::mat4 invProj = glm::inverse(proj);
		glm::mat4 invView = glm::inverse(view);

		// Unproject the NDC point to get a point on the near plane in world space
		glm::vec4 ray_world_start_h = invView * invProj * ray_ndc_start;
		ray_world_start_h /= ray_world_start_h.w; // Perspective divide
		glm::vec3 ray_world_start = glm::vec3(ray_world_start_h);

		// The ray's origin is the camera's position
		glm::vec3 ray_origin = m_renderer->GetCamera()->GetPosition(); // Assumes you have a GetPosition() method

		// The ray's direction is from the camera towards the point on the near plane
		glm::vec3 ray_dir = glm::normalize(ray_world_start - ray_origin);

		return {ray_origin, ray_dir};
	}

	void Application::Init(const AppSpecification& application_spec)
	{
		m_specification = application_spec;

		// 1. Console is created
		m_console = std::make_shared<Console>();

		// 2. World Systems are created
		m_entity_manager = std::make_unique<EntityManager>();

		// 3. Input Manager is created
		m_input_manager = std::make_unique<InputManager>();

		// 4. Renderer creates the GLFW window and OpenGL context
		m_renderer = std::make_unique<Renderer>(m_entity_manager->GetRegistry() ,application_spec, m_console);

		// 5. Cuda Manager is created and initialized (requires OpenGL context)
		m_cuda_manager = std::make_unique<CudaManager>();
		m_cuda_manager->Init();

		// 6. Physics System is created (requires CudaManager)
		m_physics_system = std::make_unique<PhysicsSystem>(*m_cuda_manager);

		DebugRenderer::Init();

		// 7. This tells GLFW to associate our 'Application' instance ('this') with the window.
		glfwSetWindowUserPointer(m_renderer->GetWindow(), this);

		// 8. InputManager sets the MASTER callbacks for the window
		Hex::InputManager::Init(m_renderer->GetWindow());

		// 9. UIManager initializes ImGui, which will now use the forwarded events
		m_ui_manager = std::make_unique<UIManager>(m_renderer->GetWindow(), m_console, *m_physics_system, *m_renderer);

		// 10. We build the scene
		m_sceneBuilder(*m_entity_manager, *m_physics_system);

		// 11. Initialize the physics system's GPU resources
		m_physics_system->Init(*m_entity_manager);

		// 12. Hand the renderer a non-owning ref so it can read the particle VBO + materials.
		m_renderer->SetPhysicsSystem(m_physics_system.get());

		m_running = true;
	}

	void Application::Run()
	{
	    float delta_time = 0.0f;
	    float last_frame = 0.0f;

	    while (m_running && !glfwWindowShouldClose(m_renderer->GetWindow()))
	    {
	    	DebugRenderer::BeginFrame();

	    	Profiler::Get().BeginFrame();

	    	// Poll for any new window/input events FIRST
	    	{
	    		PROFILE_SCOPE("Poll Events");
	    		glfwPollEvents();
			}

	        // Calculate delta time
	        const float current_frame = static_cast<float>(glfwGetTime());
	        delta_time = current_frame - last_frame;
	        last_frame = current_frame;

	        // Start the ImGui frame
	        m_ui_manager->BeginFrame();
	    	{
	    		PROFILE_SCOPE("UI Rendering");
				m_ui_manager->RenderUI(delta_time);
	    	}

	        // Get ImGui IO state AFTER rendering the UI
	        ImGuiIO& io = ImGui::GetIO();

	        // --- GLOBAL HOTKEYS ---
	        // These should work no matter what ImGui window is focused.
	        if (m_input_manager->IsKeyJustPressed(GLFW_KEY_ESCAPE))
	        {
	            m_running = false;
	        }

	        if (m_input_manager->IsKeyJustPressed(GLFW_KEY_TAB))
	        {
	            SetGameplayMode(!m_gameplayMode); // Toggle the mode
	        }

	        // Dedicated hotkeys for each render mode — no more "what mode am I in" guessing.
	        //   1 = Particles only      (cheap, voxel spheres)
	        //   2 = Meshes / SSFR water (the pretty mode)
	        //   3 = Both (debug overlay)
	        if (m_input_manager->IsKeyJustPressed(GLFW_KEY_1))
	            m_renderer->SetRenderMode(RenderMode::ParticlesOnly);
	        if (m_input_manager->IsKeyJustPressed(GLFW_KEY_2))
	            m_renderer->SetRenderMode(RenderMode::MeshesOnly);
	        if (m_input_manager->IsKeyJustPressed(GLFW_KEY_3))
	            m_renderer->SetRenderMode(RenderMode::Both);
	        // F still cycles for muscle-memory.
	        if (m_input_manager->IsKeyJustPressed(GLFW_KEY_F))
	        {
	            int next = (static_cast<int>(m_renderer->m_render_mode) + 1) % 3;
	            m_renderer->SetRenderMode(static_cast<RenderMode>(next));
	        }

	        // --- CONTEXT-SENSITIVE INPUT ---
	        if (m_gameplayMode)
	        {
	            // In gameplay mode, the game has full input control.
	            // We don't need to check ImGui's capture flags here because the cursor is disabled.
	            m_renderer->GetCamera()->Update(delta_time, *m_input_manager);
	        }
	        else // We are in UI Mode
	        {
	            // In UI mode, we only process game actions (like picking) if ImGui isn't
	            // using the mouse for its own widgets and the mouse is over the viewport.
	            if (m_ui_manager->IsViewportHovered())
	            {
	            	ProcessMousePicking();
	            }
	        }

	        // --- WORLD AND RENDER UPDATES ---
		    {
			    PROFILE_SCOPE("Physics Tick");
	    		m_physics_system->Tick(*m_entity_manager, delta_time, current_frame);
		    }

	        // Render 3D world to framebuffer
		    {
			    PROFILE_SCOPE("World Rendering");
	    		m_renderer->RenderWorld(delta_time);
		    }

	        // Finalize and render the UI
		    {
			    PROFILE_SCOPE("UI EndFrame");
	    		m_ui_manager->EndFrame();
		    }

	        // Swap buffers
		    {
			    PROFILE_SCOPE("Swap Buffers");
	    		glfwSwapBuffers(m_renderer->GetWindow());
		    }

	        // This snapshots the input state for the NEXT frame.
	        m_input_manager->Update();

	    	Profiler::Get().EndFrame();
	    }
	}
}
