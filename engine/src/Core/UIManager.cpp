// Hex
#include "HexForge/pch.h"
#include "HexForge/Core/UIManager.h"
#include "HexForge/Renderer/Renderer.h"
#include "HexForge/Renderer/Camera.h"
#include "HexForge/Core/Profiler.h"

namespace Hex
{

    UIManager::UIManager(GLFWwindow* window, const std::shared_ptr<Console>& console,
                         PhysicsSystem& physicsSystem, Renderer& renderer)
        : m_window(window),
          m_console(console),
          m_physicsSystem(physicsSystem),
          m_renderer(renderer) //
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        SetStyle();

        ImGui_ImplGlfw_InitForOpenGL(m_window, false);
        ImGui_ImplOpenGL3_Init("#version 420");

        io.IniFilename = "imgui_layout.ini";
    }

    UIManager::~UIManager()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void UIManager::BeginFrame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();


        ImGui::DockSpaceOverViewport(ImGuiDockNodeFlags_PassthruCentralNode, ImGui::GetMainViewport());
    }

    void UIManager::EndFrame()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_context);
        }
    }

    void UIManager::RenderUI(float deltaTime)
    {
        ShowMenuBar();
        
        if (m_showPhysicsControls) ShowPhysicsControls();
        if (m_showMetrics) ShowMetrics(deltaTime);
        if (m_showSceneInfo) ShowSceneInfo();
        if (m_showLightingTool) ShowLightingTool();

        if (m_console) m_console->Render();
        
        ShowViewport(); // This should be called to render the scene viewport
    }

    void UIManager::ShowMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Exit")) {
                    glfwSetWindowShouldClose(m_window, true);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Physics Controls", nullptr, &m_showPhysicsControls);
                ImGui::MenuItem("Rendering Metrics", nullptr, &m_showMetrics);
                ImGui::MenuItem("Scene Info", nullptr, &m_showSceneInfo);
                ImGui::MenuItem("Lighting Tool", nullptr, &m_showLightingTool);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }

    void UIManager::ShowPhysicsControls()
    {
        if (ImGui::Begin("Physics Controls", &m_showPhysicsControls))
        {
            ImGui::Text("Simulation Parameters");
            ImGui::DragFloat3("Gravity", &m_physicsSystem.m_gravity.x, 0.1f);
            ImGui::SliderInt("Substeps", &m_physicsSystem.m_substeps, 1, 16);
            ImGui::SliderInt("Iters / Substep", &m_physicsSystem.m_solverIterations, 1, 8);
            ImGui::DragFloat("Fluid Radius", &m_physicsSystem.m_fluidInteractionRadius, 0.01f, 0.05f, 5.0f);
            ImGui::DragFloat("Particle Radius", &m_physicsSystem.m_particleRadius, 0.005f, 0.01f, 1.0f);
            ImGui::DragFloat("Global Damping", &m_physicsSystem.m_globalDamping, 0.0005f, 0.5f, 1.0f, "%.4f");
            ImGui::DragFloat("Max Speed (m/s)", &m_physicsSystem.m_maxParticleSpeed, 0.5f, 0.0f, 200.0f);

            ImGui::Separator();
            ImGui::Text("Cloth / Rope");
            ImGui::DragFloat("Distance Compliance", &m_physicsSystem.m_distanceCompliance, 1e-8f, 0.0f, 1e-3f, "%.2e");

            ImGui::Separator();
            ImGui::Text("Picker");
            ImGui::DragFloat("Pick Stiffness", &m_physicsSystem.m_pickStiffness, 50.0f, 1.0f, 50000.0f);
            ImGui::DragFloat("Pick Damping",   &m_physicsSystem.m_pickDamping,   1.0f, 0.0f, 500.0f);

            ImGui::Separator();
            ImGui::Text("PBF Fluid Tuning");
            ImGui::DragFloat("Artif. Pressure k", &m_physicsSystem.m_pbfPressureK, 0.00005f, 0.0f, 0.01f, "%.5f");
            ImGui::DragFloat("Delta q / h",       &m_physicsSystem.m_pbfDeltaQRatio, 0.005f, 0.05f, 0.4f);
            ImGui::DragFloat("Boundary Push",     &m_physicsSystem.m_boundaryPushStrength, 0.005f, 0.0f, 1.0f);
            ImGui::DragFloat("Vorticity Eps",     &m_physicsSystem.m_vorticityEpsilon, 0.0001f, 0.0f, 0.01f, "%.4f");

            ImGui::Separator();
            ImGui::Text("Chebyshev Acceleration");
            ImGui::Checkbox("Enabled##cheby", &m_physicsSystem.m_chebyshevEnabled);
            ImGui::DragFloat("Rho", &m_physicsSystem.m_chebyshevRho, 0.001f, 0.5f, 0.999f);

            ImGui::Separator();
            ImGui::Text("Render Mode  (F to cycle)");
            const char* mode_names[] = { "Particles", "Meshes", "Both" };
            int mode = static_cast<int>(m_renderer.m_render_mode);
            if (ImGui::Combo("##mode", &mode, mode_names, IM_ARRAYSIZE(mode_names))) {
                m_renderer.SetRenderMode(static_cast<RenderMode>(mode));
            }

            if (auto* pp = m_renderer.GetPostProcess()) {
                ImGui::Separator();
                ImGui::Text("Post-process");
                ImGui::DragFloat("Exposure",        &pp->m_exposure,        0.05f, 0.1f, 5.0f);
                ImGui::DragFloat("Bloom Threshold", &pp->m_bloom_threshold, 0.05f, 0.0f, 4.0f);
                ImGui::DragFloat("Bloom Intensity", &pp->m_bloom_intensity, 0.05f, 0.0f, 4.0f);
            }

            ImGui::Separator();
            ImGui::Text("Stats");
            ImGui::Text("Particles:    %u", m_physicsSystem.GetParticleCount());
            ImGui::Text("Materials:    %u", m_physicsSystem.GetMaterialCount());
            ImGui::Text("Constraints:  %u", m_physicsSystem.GetConstraintCount());
            ImGui::Text("Rigid bodies: %u", m_physicsSystem.GetRigidBodyCount());

            ImGui::Separator();
            ImGui::Text("Domain Box");
            ImGui::DragFloat3("Min", &m_physicsSystem.m_domain.min.x, 0.1f);
            ImGui::DragFloat3("Max", &m_physicsSystem.m_domain.max.x, 0.1f);
        }
        ImGui::End();
    }
    
    // Implement other UI panel methods as needed
    void UIManager::ShowMetrics(float deltaTime)
    {
        if (ImGui::Begin("Metrics", &m_showMetrics))
        {
            // --- Basic Info ---
            ImGui::Text("FPS: %.1f", 1.0f / deltaTime);
            ImGui::Text("Frame Time: %.3f ms", deltaTime * 1000.0f);

            ImGui::Separator();

            // --- Frame Time Plot ---
            // Add current frame time to history
            m_frameTimeHistory.push_back(deltaTime * 1000.0f);
            if (m_frameTimeHistory.size() > 100) // Keep the history to a manageable size
            {
                m_frameTimeHistory.erase(m_frameTimeHistory.begin());
            }

            ImGui::PlotLines("Frame Time (ms)", m_frameTimeHistory.data(), m_frameTimeHistory.size(), 0,
                             nullptr, 0.0f, 33.3f, ImVec2(0, 80)); // scale from 0 to 33ms (30fps)

            ImGui::Separator();

            // --- Hierarchical Breakdown ---
            ImGui::Text("Frame Breakdown:");

            const ProfileResult& frameData = Profiler::Get().GetFrameData();
            // Start rendering the tree from the root's children
            for (const auto& child : frameData.Children)
            {
                RenderProfileNode(child);
            }
        }
        ImGui::End();
    }

    void UIManager::ShowSceneInfo()
    {
        if (ImGui::Begin("Scene Information", &m_showSceneInfo))
        {
            ImGui::Text("Camera Position:");
            ImGui::Text("%.2f, %.2f, %.2f",
                        m_renderer.GetCamera()->GetPosition().x,
                        m_renderer.GetCamera()->GetPosition().y,
                        m_renderer.GetCamera()->GetPosition().z);
            ImGui::Separator();


            // Display Camera View Matrix
            ImGui::Text("Camera View:");
            glm::mat4 viewMatrix =  m_renderer.GetCamera()->GetViewMatrix();
            for (int i = 0; i < 4; ++i) {
                ImGui::Text("%.2f, %.2f, %.2f, %.2f",
                            viewMatrix[i][0], viewMatrix[i][1], viewMatrix[i][2], viewMatrix[i][3]);
            }
            ImGui::Separator();

            // Display Camera Projection Matrix
            ImGui::Text("Camera Proj:");
            glm::mat4 projMatrix =  m_renderer.GetCamera()->GetProjectionMatrix();
            for (int i = 0; i < 4; ++i) {
                ImGui::Text("%.2f, %.2f, %.2f, %.2f",
                            projMatrix[i][0], projMatrix[i][1], projMatrix[i][2], projMatrix[i][3]);
            }

            // Display Primitives Information
            if (ImGui::CollapsingHeader("Primitives Information"))
            {
            }
        }
        ImGui::End();
    }

    void UIManager::ShowLightingTool()
    {
        if (ImGui::Begin("Lighting Tool", &m_showLightingTool)) // Allow closing
        {
            constexpr int active_lights_count = 1; // TODO: update to reflect actual light count
            constexpr int selected_light_index = 1; // TODO: update to reflect actual selected light index

            // Display static lighting information
            ImGui::Text("Active Lights: %d", active_lights_count);
            ImGui::Text("Selected Light: %d", selected_light_index);
            ImGui::Text("Light Direction:");

            // Add interactive controls for editing the light position
            if (ImGui::DragFloat3("LightDirection", &m_light_dir.x, 0.1f, -1.0f, 1.0f))
            {
                m_renderer.SetLightDir(m_light_dir);
            }

            ImGui::Separator();

            // Shadow Mapping
            if (ImGui::CollapsingHeader("Shadow Mapping"))
            {
                static float shadow_zoom = 1.0f; // Zoom factor
                static glm::vec2 shadow_pan(0.0f, 0.0f); // Pan offsets

                ImGui::Text("Shadow Map");

                // Add controls for zoom and pan
                ImGui::SliderFloat("Zoom", &shadow_zoom, 0.1f, 5.0f, "Zoom: %.2f");
                ImGui::DragFloat2("Pan", &shadow_pan.x, 0.01f, -1.0f, 1.0f, "Pan: %.2f");

                // Calculate the UV range for zoom
                float uv_range = 0.5f / shadow_zoom;
                glm::vec2 uv_center = glm::vec2(0.5f) + shadow_pan * uv_range;

                // Clamp the UV center to avoid going out of bounds
                uv_center.x = glm::clamp(uv_center.x, uv_range, 1.0f - uv_range);
                uv_center.y = glm::clamp(uv_center.y, uv_range, 1.0f - uv_range);

                ImVec2 uv_min(uv_center.x - uv_range, uv_center.y - uv_range);
                ImVec2 uv_max(uv_center.x + uv_range, uv_center.y + uv_range);

                // Display the shadow map with the calculated UVs
                ImVec2 image_size(300, 300); // Fixed display size
                ImGui::Image((void*)(intptr_t)m_renderer.GetShadowMapTexture(), image_size, uv_min, uv_max);
            }


        }
        ImGui::End();
    }

    void UIManager::ShowViewport()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse);

        m_isViewportHovered = ImGui::IsWindowHovered();

        // Capture the viewport's screen-space rect so Application::GetMouseRay can convert
        // raw cursor coords (which are window-relative, not viewport-relative) into NDC.
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        ImVec2 viewportScreenPos = ImGui::GetCursorScreenPos();
        m_viewportScreenPos  = { viewportScreenPos.x, viewportScreenPos.y };
        m_viewportScreenSize = { viewportPanelSize.x, viewportPanelSize.y };

        uint32_t textureID = m_renderer.GetFrameBufferTexture();

        ImGui::Image(
            (void*)(intptr_t)textureID,
            viewportPanelSize,
            ImVec2(0, 1),
            ImVec2(1, 0)
        );

        if (m_viewportSize.x != viewportPanelSize.x || m_viewportSize.y != viewportPanelSize.y)
        {
            m_viewportSize = { viewportPanelSize.x, viewportPanelSize.y };
            m_renderer.ResizeFrameBuffer(viewportPanelSize.x, viewportPanelSize.y);
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void UIManager::RenderProfileNode(const ProfileResult& node)
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once); // Keep nodes open by default initially

        // If the node has children, create a collapsible tree node
        if (!node.Children.empty())
        {
            // Format the label to show name and time
            char label[128];
            sprintf(label, "%s (%.3f ms)", node.Name, node.TimeMs);

            if (ImGui::TreeNode(label))
            {
                for (const auto& child : node.Children)
                {
                    RenderProfileNode(child); // Recurse for children
                }
                ImGui::TreePop();
            }
        }
        else // If it's a leaf node, just display it as text
        {
            ImGui::Text("%s: %.3f ms", node.Name, node.TimeMs);
        }
    }


    void UIManager::SetStyle()
    {
       ImGuiStyle& style = ImGui::GetStyle();
       ImVec4* colors = style.Colors;

       // Base Colors
       colors[ImGuiCol_Text]                  = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
       colors[ImGuiCol_TextDisabled]          = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
       colors[ImGuiCol_WindowBg]              = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
       colors[ImGuiCol_ChildBg]               = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
       colors[ImGuiCol_PopupBg]               = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
       colors[ImGuiCol_Border]                = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
       colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
       colors[ImGuiCol_FrameBg]               = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
       colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
       colors[ImGuiCol_FrameBgActive]         = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
       colors[ImGuiCol_TitleBg]               = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
       colors[ImGuiCol_TitleBgActive]         = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
       colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
       colors[ImGuiCol_MenuBarBg]             = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
       colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
       colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
       colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
       colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
       colors[ImGuiCol_CheckMark]             = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
       colors[ImGuiCol_SliderGrab]            = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
       colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
       colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
       colors[ImGuiCol_ButtonHovered]         = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
       colors[ImGuiCol_ButtonActive]          = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
       colors[ImGuiCol_Header]                = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
       colors[ImGuiCol_HeaderHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
       colors[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
       colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
       colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
       colors[ImGuiCol_SeparatorActive]       = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
       colors[ImGuiCol_ResizeGrip]            = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
       colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
       colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
       colors[ImGuiCol_Tab]                   = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
       colors[ImGuiCol_TabHovered]            = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
       colors[ImGuiCol_TabActive]             = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
       colors[ImGuiCol_TabUnfocused]          = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
       colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.15f, 0.20f, 0.25f, 1.00f);
       colors[ImGuiCol_DockingPreview]        = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
       colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

       // Customize ImGui style
       style.WindowRounding    = 5.3f;
       style.FrameRounding     = 2.3f;
       style.ScrollbarRounding = 1.5f;
       style.GrabRounding      = 2.3f;
       style.WindowBorderSize  = 1.0f;
       style.FrameBorderSize   = 1.0f;
       style.ItemSpacing       = ImVec2(10, 8);
    }
}

