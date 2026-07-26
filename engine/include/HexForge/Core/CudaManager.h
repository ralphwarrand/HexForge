#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <glad/glad.h>

// Forward declare CUDA types to avoid including cuda_runtime.h in a public header
typedef struct cudaGraphicsResource* cudaGraphicsResource_t;

namespace Hex
{

    class CudaManager {
    public:
        CudaManager();
        ~CudaManager();

        void Init();
        void Shutdown();

        void RegisterGLBuffer(GLuint vbo);
        void UnregisterGLBuffers();

        void MapGLBuffers();
        void UnmapGLBuffers();

        void* GetMappedPointer(GLuint vbo);
        cudaGraphicsResource_t GetRegisteredResource(GLuint vbo);

    private:
        void FindCudaDevice();

        struct CudaDeviceProperties {
            int id = -1;
            char name[256]{};
            int major = 0;
            int minor = 0;
            size_t totalGlobalMem = 0;
        };

        CudaDeviceProperties m_device_properties;
        std::unordered_map<GLuint, cudaGraphicsResource_t> m_graphics_resources_map;
    };

}
