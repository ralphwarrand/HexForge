#include "HexForge/pch.h"
#include "HexForge/Core/UIManager.h"
#include "HexForge/Renderer/Renderer.h" // For framebuffer access if needed

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
            ImGui::SliderInt("Solver Iterations", &m_physicsSystem.m_solverIterations, 1, 100);

            ImGui::Separator();
            ImGui::Text("Wind Parameters");
            ImGui::DragFloat3("Wind Direction", &m_physicsSystem.m_windDirection.x, 0.01f, -1.0f, 1.0f);
            ImGui::SliderFloat("Wind Strength", &m_physicsSystem.m_windStrength, 0.0f, 100.0f);
            ImGui::SliderFloat("Wind Frequency", &m_physicsSystem.m_windFrequency, 0.0f, 10.0f);
            ImGui::SliderFloat("Turbulence", &m_physicsSystem.m_turbulence, 0.0f, 20.0f);
        }
        ImGui::End();
    }
    
    // Implement other UI panel methods as needed
    void UIManager::ShowMetrics(float deltaTime)
    {
        if (ImGui::Begin("Rendering Metrics", &m_showMetrics))
        {
            ImGui::Text("FPS: %.1f", 1.0f / deltaTime);
            ImGui::Text("Frame-time: %.6f ms", deltaTime * 1000.0f);
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

        // Set the window padding to zero for a seamless look
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });

        // Begin the Viewport window
        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse);

        // Get the current size of the viewport's content area
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

        // Get the texture ID from your renderer's framebuffer
        // We will add the GetFrameBufferTexture() function in the next step
        uint32_t textureID = m_renderer.GetFrameBufferTexture();

        // Draw the texture as an image, flipping the Y-axis for OpenGL
        // OpenGL's texture origin is bottom-left, ImGui's is top-left
        ImGui::Image(
            (void*)(intptr_t)textureID,   // The texture ID
            viewportPanelSize,           // Size of the image
            ImVec2(0, 1),                // UV coordinates for the top-left corner
            ImVec2(1, 0)                 // UV coordinates for the bottom-right corner
        );

        // Check if the viewport size has changed
        if (m_viewportSize.x != viewportPanelSize.x || m_viewportSize.y != viewportPanelSize.y)
        {
            // If it has, update our stored size and tell the renderer to resize its framebuffer
            m_viewportSize = { viewportPanelSize.x, viewportPanelSize.y };
            m_renderer.ResizeFrameBuffer(viewportPanelSize.x, viewportPanelSize.y);
        }

        ImGui::End();
        ImGui::PopStyleVar();
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

