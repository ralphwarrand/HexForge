#include "HexForge/pch.h"
#include "HexForge/Core/CudaManager.h"
#include "HexForge/Core/Logger.h"

#include <cstring>

#if HEX_ENABLE_CUDA
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>
#endif

namespace Hex
{

    CudaManager::CudaManager() = default;

    CudaManager::~CudaManager() {
        Shutdown();
    }

    void CudaManager::Init() {
        FindCudaDevice();
    }

    void CudaManager::Shutdown() {
#if HEX_ENABLE_CUDA
        if (m_device_properties.id != -1) {

            UnregisterGLBuffers();

            cudaDeviceReset();
            Log(LogLevel::Info, "CUDA device has been reset.");
        }
#endif
    }

    void CudaManager::RegisterGLBuffer(GLuint vbo) {
        if (vbo == 0) return;

#if HEX_ENABLE_CUDA
        cudaGraphicsResource_t resource = nullptr;
        cudaError_t err = cudaGraphicsGLRegisterBuffer(&resource, vbo, cudaGraphicsRegisterFlagsWriteDiscard);
        if (err != cudaSuccess) {
            Log(LogLevel::Error, std::format("CUDA Error: cudaGraphicsGLRegisterBuffer failed for VBO {} with code {}. {}", vbo, static_cast<int>(err), cudaGetErrorString(err)));
            return;
        }
        m_graphics_resources_map[vbo] = resource;
        Log(LogLevel::Info, std::format("Successfully registered VBO {} with CUDA.", vbo));
#else
        (void)vbo;
#endif
    }

    void CudaManager::UnregisterGLBuffers() {
#if HEX_ENABLE_CUDA
        for (auto const& [vbo, resource] : m_graphics_resources_map) {
            if (resource) {
                cudaGraphicsUnregisterResource(resource);
            }
        }
        m_graphics_resources_map.clear();
        Log(LogLevel::Info, "Unregistered all CUDA graphics resources.");
#endif
    }

    void CudaManager::MapGLBuffers() {
#if HEX_ENABLE_CUDA
        for (auto const& [vbo, resource] : m_graphics_resources_map) {
            if (resource) {
                cudaGraphicsResource_t non_const_resource = resource;
                cudaError_t error = cudaGraphicsMapResources(1, &non_const_resource, 0);
                if (error != cudaSuccess) {
                    Log(LogLevel::Error, std::format("CUDA Error: cudaGraphicsMapResources failed for VBO {} with code {}. {}", vbo, static_cast<int>(error), cudaGetErrorString(error)));
                }
            }
        }
#endif
    }

    void CudaManager::UnmapGLBuffers() {
#if HEX_ENABLE_CUDA
        for (auto const& [vbo, resource] : m_graphics_resources_map) {
            if (resource) {
                cudaGraphicsResource_t non_const_resource = resource;
                cudaError_t error = cudaGraphicsUnmapResources(1, &non_const_resource, 0);
                if (error != cudaSuccess) {
                    Log(LogLevel::Error, std::format("CUDA Error: cudaGraphicsUnmapResources failed for VBO {} with code {}. {}", vbo, static_cast<int>(error), cudaGetErrorString(error)));
                }
            }
        }
#endif
    }

    void* CudaManager::GetMappedPointer(GLuint vbo) {
        if (m_graphics_resources_map.find(vbo) == m_graphics_resources_map.end()) {
            return nullptr;
        }

        void* d_ptr = nullptr;
#if HEX_ENABLE_CUDA
        size_t num_bytes = 0;
        cudaGraphicsResourceGetMappedPointer(&d_ptr, &num_bytes, m_graphics_resources_map[vbo]);
#else
        (void)vbo;
#endif
        return d_ptr;
    }

    void CudaManager::FindCudaDevice() {
#if HEX_ENABLE_CUDA
        Log(LogLevel::Info, "Initializing CUDA and checking versions...");

        int driverVersion = 0, runtimeVersion = 0;
        cudaError_t driverErr = cudaDriverGetVersion(&driverVersion);
        cudaError_t runtimeErr = cudaRuntimeGetVersion(&runtimeVersion);

        if (driverErr != cudaSuccess) {
            Log(LogLevel::Fatal, std::format("CUDA Error: cudaDriverGetVersion failed with code {}. {}", static_cast<int>(driverErr), cudaGetErrorString(driverErr)));
            return;
        }
        if (runtimeErr != cudaSuccess) {
            Log(LogLevel::Fatal, std::format("CUDA Error: cudaRuntimeGetVersion failed with code {}. {}", static_cast<int>(runtimeErr), cudaGetErrorString(runtimeErr)));
            return;
        }

        Log(LogLevel::Info, std::format("CUDA Driver Version: {}.{}", driverVersion / 1000, (driverVersion % 1000) / 10));
        Log(LogLevel::Info, std::format("CUDA Runtime Version: {}.{}", runtimeVersion / 1000, (runtimeVersion % 1000) / 10));

        if (driverVersion == 0) {
            Log(LogLevel::Fatal, "CUDA Driver API reported version 0. This usually means no NVIDIA driver is installed or it is not running.");
            return;
        }

        if (driverVersion < runtimeVersion) {
            Log(LogLevel::Fatal, std::format("Unsupported Configuration: CUDA Driver version ({}) is older than the CUDA Runtime version ({}). Please update your NVIDIA display driver to the latest version.", driverVersion, runtimeVersion));
            return;
        }

        Log(LogLevel::Info, "Finding CUDA device for OpenGL Interop...");

        unsigned int device_count = 0;
        int devices[1]; // We only need one device.
        cudaError_t error = cudaGLGetDevices(&device_count, devices, 1, cudaGLDeviceListAll);

        if (error != cudaSuccess) {
            Log(LogLevel::Fatal, std::format("CUDA Interop Error: cudaGLGetDevices failed with code {}. {}. This usually means the NVIDIA driver is not running or is in a bad state. Please ensure the application is running on the NVIDIA GPU via the NVIDIA Control Panel.", static_cast<int>(error), cudaGetErrorString(error)));
            return;
        }

        if (device_count == 0) {
            Log(LogLevel::Fatal, "No CUDA-enabled devices were found that are compatible with the current OpenGL context. Ensure your monitor is connected to the NVIDIA GPU and that the application is set to run on it.");
            return;
        }

        int cuda_device = devices[0];
        cudaSetDevice(cuda_device);

        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, cuda_device);

        m_device_properties.id = cuda_device;
        std::strncpy(m_device_properties.name, prop.name, sizeof(m_device_properties.name) - 1);
        m_device_properties.name[sizeof(m_device_properties.name) - 1] = '\0';
        m_device_properties.major = prop.major;
        m_device_properties.minor = prop.minor;
        m_device_properties.totalGlobalMem = prop.totalGlobalMem;

        Log(LogLevel::Info, std::format("Successfully initialized CUDA on device {}: {}", cuda_device, m_device_properties.name));
        Log(LogLevel::Info, std::format("Compute Capability: {}.{}", m_device_properties.major, m_device_properties.minor));
        Log(LogLevel::Info, std::format("Total Global Memory: {} MB", m_device_properties.totalGlobalMem / (1024 * 1024)));
#else
        Log(LogLevel::Info, "CUDA acceleration is disabled or unavailable on this build.");
#endif
    }

}
