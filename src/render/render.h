#include "kronyx/render.h"

typedef struct kyPipelineImpl {
    kyRenderDevice *rd;
    kyShader *shader;
    kyVertexLayout layout;
    kyPrimitiveTopology topology;
    int depth_test;
    int depth_write;
    int cull_mode;
    int blend_on;
    kyVec4 blend_color;
    void *impl;
} kyPipelineImpl;

typedef struct kyRenderDeviceGL kyRenderDeviceGL;

struct kyRenderDevice {
    kyRendererBackend backend;
    void *impl;
    kyAllocator alloc;
};
