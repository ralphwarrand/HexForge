#pragma once
#include <chrono>
#include <vector>

namespace Hex
{
    // A single node in our profiling tree
    struct ProfileResult
    {
        const char* Name;
        float TimeMs = 0.0f;
        std::vector<ProfileResult> Children;
    };

    // Singleton class to manage profiling data
    class Profiler
    {
    public:
        static Profiler& Get()
        {
            static Profiler instance;
            return instance;
        }

        void BeginFrame()
        {
            // Start a new frame by clearing the "next" buffer
            m_profileDataNext = ProfileResult{ "Frame" };
            m_nodeStack.clear();
            m_nodeStack.push_back(&m_profileDataNext);
        }

        void EndFrame()
        {
            // When the frame is done, move the collected data to the "current" buffer
            // This avoids data changing while ImGui is trying to render it.
            m_profileDataCurrent = m_profileDataNext;
        }

        void PushScope(const char* name)
        {
            ProfileResult newNode{ name };
            m_nodeStack.back()->Children.push_back(newNode);
            m_nodeStack.push_back(&m_nodeStack.back()->Children.back());
        }

        void PopScope(float timeMs)
        {
            m_nodeStack.back()->TimeMs = timeMs;
            m_nodeStack.pop_back();
        }

        const ProfileResult& GetFrameData() const { return m_profileDataCurrent; }

    private:
        Profiler() = default;
        ~Profiler() = default;
        Profiler(const Profiler&) = delete;
        Profiler& operator=(const Profiler&) = delete;

        ProfileResult m_profileDataCurrent{ "Frame" };
        ProfileResult m_profileDataNext{ "Frame" };
        std::vector<ProfileResult*> m_nodeStack;
    };

    // RAII Timer class
    class ProfilingTimer
    {
    public:
        ProfilingTimer(const char* name)
            : m_name(name), m_startTime(std::chrono::high_resolution_clock::now())
        {
            Profiler::Get().PushScope(m_name);
        }

        ~ProfilingTimer()
        {
            auto endTime = std::chrono::high_resolution_clock::now();
            long long start = std::chrono::time_point_cast<std::chrono::microseconds>(m_startTime).time_since_epoch().count();
            long long end = std::chrono::time_point_cast<std::chrono::microseconds>(endTime).time_since_epoch().count();
            float durationMs = (end - start) * 0.001f;
            Profiler::Get().PopScope(durationMs);
        }

    private:
        const char* m_name;
        std::chrono::time_point<std::chrono::high_resolution_clock> m_startTime;
    };
}

// Helper macro to make profiling a scope easy
#define PROFILE_SCOPE(name) ProfilingTimer timer##__LINE__(name)