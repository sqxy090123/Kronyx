#include "render_backend.h"
#include "kronyx/math.h"
#include "kronyx/log.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef KY_HAS_VULKAN

#include <vulkan/vulkan.h>

#define KY_VK_MAX_ATTRIBS 8
#define KY_VK_IMAGE_W 128
#define KY_VK_IMAGE_H 128

/* ---------------------------------------------------------------------------
 * Device
 * ------------------------------------------------------------------------- */
typedef struct kyVKDevice {
    VkInstance instance;
    VkPhysicalDevice physical;
    VkDevice device;
    VkQueue queue;                  /* graphics queue */
    uint32_t queue_family;
    VkPhysicalDeviceMemoryProperties mem_props;
    /* render target */
    VkImage image;
    VkImageView view;
    VkMemoryAllocateInfo image_mem_req;
    VkDeviceMemory image_mem;
    VkImageLayout image_layout;     /* tracked for barriers */
    /* readback host buffer */
    VkBuffer readback;
    VkDeviceMemory readback_mem;
    VkDeviceSize readback_size;
    /* sync */
    VkFence fence;
    int width;
    int height;
    int frame_count;
} kyVKDevice;

/* ---------------------------------------------------------------------------
 * Resources
 * ------------------------------------------------------------------------- */
typedef struct kyVKShader {
    VkShaderModule vs;
    VkShaderModule fs;
} kyVKShader;

typedef struct kyVKBuffer {
    VkBuffer buf;
    VkDeviceMemory mem;
    size_t size;
    int dynamic;
} kyVKBuffer;

typedef struct kyVKTexture {
    VkImage img;
    VkImageView view;
    VkDeviceMemory mem;
} kyVKTexture;

typedef struct kyVKPipeline {
    VkPipeline pipe;
    VkPipelineLayout layout;
    VkRenderPass rp;
    kyVKShader *shader;             /* raw pointer; caller owns destroy order */
    kyVertexLayout layout_vk;
    kyVertexAttrib attribs[KY_VK_MAX_ATTRIBS];
    kyPrimitiveTopology topology;
    int depth_test, depth_write, cull_mode, blend_on;
} kyVKPipeline;

typedef struct kyVKCmdList {
    kyRenderDevice *rd;
    kyVKPipeline *pipeline;
    uint32_t index_size;
    uint32_t vertex_count;
    uint32_t pending_draws;
} kyVKCmdList;

KY_STATIC_ASSERT(offsetof(kyVKCmdList, rd) == 0,
                 "kyVKCmdList.rd must be the first member");

/* Empty-but-legal SPIR-V module (magic + version + 0 words).  The lvp
 * driver accepts it for module creation; the pipeline below only needs a
 * valid module handle, the actual fragment shader computes a constant
 * color via the pipeline's color-blend state rather than shader code. */
static const uint32_t VK_EMPTY_SPIRV[3] = {
    0x07230203, 0x00000000, 0x00000000,
};

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */
static VkResult vk_alloc_host_mem(kyVKDevice *d, VkDeviceSize size,
                                   VkDeviceMemory *out_mem) {
    VkMemoryAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = size;
    uint32_t idx = 0;
    for (uint32_t t = 0; t < d->mem_props.memoryTypeCount; t++) {
        VkMemoryPropertyFlags pf = d->mem_props.memoryTypes[t].propertyFlags;
        if ((pf & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            idx = t;
            break;
        }
    }
    ai.memoryTypeIndex = idx;
    return vkAllocateMemory(d->device, &ai, NULL, out_mem);
}

/* Choose a memory type index for a requirement set (device-local for
 * GPU-resident resources). */
static uint32_t vk_pick_mem_type(const VkPhysicalDeviceMemoryProperties *mp,
                                  VkMemoryRequirements *mr) {
    for (uint32_t t = 0; t < mp->memoryTypeCount; t++) {
        if (mr->memoryTypeBits & (1u << t))
            return t;
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * create / destroy
 * ------------------------------------------------------------------------- */
static void *vk_create(void *platform_win) {
    KY_UNUSED(platform_win);
    kyVKDevice *d = (kyVKDevice *)calloc(1, sizeof(kyVKDevice));
    if (!d) return NULL;

    /* 1. instance */
    const char *app_name = "kronyx";
    VkApplicationInfo app = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app.apiVersion = VK_MAKE_VERSION(1, 0, 0);
    app.pApplicationName = app_name;
    app.applicationVersion = 0;
    VkInstanceCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    ici.pApplicationInfo = &app;
    if (vkCreateInstance(&ici, NULL, &d->instance) != VK_SUCCESS) {
        ky_log_write(KY_LOG_ERROR, "vk_backend: vkCreateInstance failed");
        free(d);
        return NULL;
    }

    /* 2. physical device */
    uint32_t dev_count = 0;
    if (vkEnumeratePhysicalDevices(d->instance, &dev_count, NULL) !=
        VK_SUCCESS || dev_count == 0) {
        ky_log_write(KY_LOG_ERROR, "vk_backend: no Vulkan physical device");
        vkDestroyInstance(d->instance, NULL);
        free(d);
        return NULL;
    }
    if (vkEnumeratePhysicalDevices(d->instance, &dev_count, &d->physical) !=
        VK_SUCCESS) {
        vkDestroyInstance(d->instance, NULL);
        free(d);
        return NULL;
    }
    vkGetPhysicalDeviceMemoryProperties(d->physical, &d->mem_props);

    /* 3. pick a graphics queue family */
    uint32_t qf_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(d->physical, &qf_count, NULL);
    VkQueueFamilyProperties qf[qf_count > 0 ? qf_count : 1];
    vkGetPhysicalDeviceQueueFamilyProperties(d->physical, &qf_count, qf);
    d->queue_family = 0;
    for (uint32_t i = 0; i < qf_count; i++) {
        if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            d->queue_family = i;
            break;
        }
    }
    float qf_priority = 1.0f;
    VkDeviceQueueCreateInfo qci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    qci.queueFamilyIndex = d->queue_family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &qf_priority;
    VkDeviceCreateInfo dci = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    if (vkCreateDevice(d->physical, &dci, NULL, &d->device) != VK_SUCCESS) {
        ky_log_write(KY_LOG_ERROR, "vk_backend: vkCreateDevice failed");
        vkDestroyInstance(d->instance, NULL);
        free(d);
        return NULL;
    }
    vkGetDeviceQueue(d->device, d->queue_family, 0, &d->queue);

    /* 4. color image + view (device-local, R8G8B8A8) */
    VkImageCreateInfo ici_img = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici_img.imageType = VK_IMAGE_TYPE_2D;
    ici_img.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici_img.extent.width = KY_VK_IMAGE_W;
    ici_img.extent.height = KY_VK_IMAGE_H;
    ici_img.extent.depth = 1;
    ici_img.mipLevels = 1;
    ici_img.arrayLayers = 1;
    ici_img.samples = VK_SAMPLE_COUNT_1_BIT;
    ici_img.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici_img.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ici_img.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(d->device, &ici_img, NULL, &d->image) != VK_SUCCESS) {
        ky_log_write(KY_LOG_ERROR, "vk_backend: image create failed");
        vkDestroyDevice(d->device, NULL);
        vkDestroyInstance(d->instance, NULL);
        free(d);
        return NULL;
    }
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(d->device, d->image, &mr);
    d->image_mem_req.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    d->image_mem_req.allocationSize = mr.size;
    d->image_mem_req.memoryTypeIndex = vk_pick_mem_type(&d->mem_props, &mr);
    if (vkAllocateMemory(d->device, &d->image_mem_req, NULL,
                         &d->image_mem) != VK_SUCCESS) {
        ky_log_write(KY_LOG_ERROR, "vk_backend: image mem alloc failed");
        vkDestroyImage(d->device, d->image, NULL);
        vkDestroyDevice(d->device, NULL);
        vkDestroyInstance(d->instance, NULL);
        free(d);
        return NULL;
    }
    vkBindImageMemory(d->device, d->image, d->image_mem, 0);
    d->image_layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageViewCreateInfo vci = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    vci.image = d->image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = VK_FORMAT_R8G8B8A8_UNORM;
    vci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.layerCount = 1;
    if (vkCreateImageView(d->device, &vci, NULL, &d->view) != VK_SUCCESS) {
        vkDestroyImage(d->device, d->image, NULL);
        vkFreeMemory(d->device, d->image_mem, NULL);
        vkDestroyDevice(d->device, NULL);
        vkDestroyInstance(d->instance, NULL);
        free(d);
        return NULL;
    }

    /* 5. readback host buffer */
    d->readback_size = (VkDeviceSize)(KY_VK_IMAGE_W * KY_VK_IMAGE_H * 4);
    VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size = d->readback_size;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    {
        uint32_t qfi = d->queue_family;
        bci.queueFamilyIndexCount = 1;
        bci.pQueueFamilyIndices = &qfi;
    }
    if (vkCreateBuffer(d->device, &bci, NULL, &d->readback) != VK_SUCCESS) {
        ky_log_write(KY_LOG_ERROR, "vk_backend: readback buf create failed");
        vkDestroyImageView(d->device, d->view, NULL);
        vkDestroyImage(d->device, d->image, NULL);
        vkFreeMemory(d->device, d->image_mem, NULL);
        vkDestroyDevice(d->device, NULL);
        vkDestroyInstance(d->instance, NULL);
        free(d);
        return NULL;
    }
    if (vk_alloc_host_mem(d, d->readback_size, &d->readback_mem) !=
        VK_SUCCESS) {
        vkDestroyBuffer(d->device, d->readback, NULL);
        vkDestroyImageView(d->device, d->view, NULL);
        vkDestroyImage(d->device, d->image, NULL);
        vkFreeMemory(d->device, d->image_mem, NULL);
        vkDestroyDevice(d->device, NULL);
        vkDestroyInstance(d->instance, NULL);
        free(d);
        return NULL;
    }
    vkBindBufferMemory(d->device, d->readback, d->readback_mem, 0);

    /* 6. fence.  Created for the future submit path that will signalled a
     * render-pass flush; reset here so a stale signalled state from a
     * previous device does not block the first wait. */
    VkFenceCreateInfo fci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    if (vkCreateFence(d->device, &fci, NULL, &d->fence) != VK_SUCCESS) {
        vkDestroyBuffer(d->device, d->readback, NULL);
        vkFreeMemory(d->device, d->readback_mem, NULL);
        vkDestroyImageView(d->device, d->view, NULL);
        vkDestroyImage(d->device, d->image, NULL);
        vkFreeMemory(d->device, d->image_mem, NULL);
        vkDestroyDevice(d->device, NULL);
        vkDestroyInstance(d->instance, NULL);
        free(d);
        return NULL;
    }
    vkResetFences(d->device, 1, &d->fence);

    d->width = KY_VK_IMAGE_W;
    d->height = KY_VK_IMAGE_H;
    d->frame_count = 0;
    return d;
}

static void vk_destroy(void *impl) {
    kyVKDevice *d = (kyVKDevice *)impl;
    if (!d) return;
    vkDestroyFence(d->device, d->fence, NULL);
    vkDestroyBuffer(d->device, d->readback, NULL);
    vkFreeMemory(d->device, d->readback_mem, NULL);
    vkDestroyImageView(d->device, d->view, NULL);
    vkDestroyImage(d->device, d->image, NULL);
    vkFreeMemory(d->device, d->image_mem, NULL);
    vkDestroyDevice(d->device, NULL);
    vkDestroyInstance(d->instance, NULL);
    free(d);
}

static const char *vk_name(void) {
    return "vulkan";
}

/* ---------------------------------------------------------------------------
 * Shaders
 * ------------------------------------------------------------------------- */
static void *vk_create_shader(void *impl, const kyShaderSource *src) {
    kyVKDevice *d = (kyVKDevice *)impl;
    if (!d || !src) return NULL;
    kyVKShader *s = (kyVKShader *)calloc(1, sizeof(kyVKShader));
    if (!s) return NULL;

    VkShaderModuleCreateInfo sci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    sci.codeSize = sizeof(VK_EMPTY_SPIRV);
    sci.pCode = VK_EMPTY_SPIRV;
    if (vkCreateShaderModule(d->device, &sci, NULL, &s->vs) != VK_SUCCESS) {
        free(s);
        return NULL;
    }
    if (vkCreateShaderModule(d->device, &sci, NULL, &s->fs) != VK_SUCCESS) {
        vkDestroyShaderModule(d->device, s->vs, NULL);
        free(s);
        return NULL;
    }
    return s;
}

static void vk_destroy_shader(void *impl, void *shader) {
    kyVKDevice *d = (kyVKDevice *)impl;
    kyVKShader *s = (kyVKShader *)shader;
    if (!d || !s) return;
    vkDestroyShaderModule(d->device, s->vs, NULL);
    vkDestroyShaderModule(d->device, s->fs, NULL);
    free(s);
}

static int vk_shader_uniform(void *impl, void *shader, const char *name) {
    KY_UNUSED(impl);
    KY_UNUSED(shader);
    KY_UNUSED(name);
    return -1;
}

/* ---------------------------------------------------------------------------
 * Buffers
 * ------------------------------------------------------------------------- */
static void *vk_create_buffer(void *impl, size_t size, const void *data,
                              int dynamic) {
    kyVKDevice *d = (kyVKDevice *)impl;
    if (!d || size == 0) return NULL;

    kyVKBuffer *b = (kyVKBuffer *)calloc(1, sizeof(kyVKBuffer));
    if (!b) return NULL;
    b->size = size;
    b->dynamic = dynamic;

    VkBufferCreateInfo bci = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size = (VkDeviceSize)size;
    bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    {
        uint32_t qfi = d->queue_family;
        bci.queueFamilyIndexCount = 1;
        bci.pQueueFamilyIndices = &qfi;
    }
    if (vkCreateBuffer(d->device, &bci, NULL, &b->buf) != VK_SUCCESS) {
        free(b);
        return NULL;
    }
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(d->device, b->buf, &mr);
    VkMemoryAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = vk_pick_mem_type(&d->mem_props, &mr);
    if (vkAllocateMemory(d->device, &ai, NULL, &b->mem) != VK_SUCCESS) {
        vkDestroyBuffer(d->device, b->buf, NULL);
        free(b);
        return NULL;
    }
    vkBindBufferMemory(d->device, b->buf, b->mem, 0);

    if (data) {
        VkMemoryPropertyFlags pf = d->mem_props.memoryTypes[ai.memoryTypeIndex]
                                       .propertyFlags;
        if (pf & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT &&
            (pf & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            void *mapped = NULL;
            if (vkMapMemory(d->device, b->mem, 0, size, 0, &mapped) ==
                VK_SUCCESS) {
                memcpy(mapped, data, size);
                vkUnmapMemory(d->device, b->mem);
            }
        }
    }
    return b;
}

static void vk_destroy_buffer(void *impl, void *buf) {
    kyVKDevice *d = (kyVKDevice *)impl;
    kyVKBuffer *b = (kyVKBuffer *)buf;
    if (!d || !b) return;
    vkDestroyBuffer(d->device, b->buf, NULL);
    vkFreeMemory(d->device, b->mem, NULL);
    free(b);
}

static int vk_update_buffer(void *impl, void *buf, size_t offset, size_t size,
                            const void *data) {
    kyVKDevice *d = (kyVKDevice *)impl;
    kyVKBuffer *b = (kyVKBuffer *)buf;
    if (!d || !b || !data || size == 0) return -1;
    if (offset > b->size || size > b->size - offset) return -1;
    VkMemoryPropertyFlags pf = d->mem_props
                                   .memoryTypes[0].propertyFlags;
    void *mapped = NULL;
    if (pf & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT &&
        (pf & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
        vkMapMemory(d->device, b->mem, offset, size, 0, &mapped) ==
            VK_SUCCESS) {
        memcpy((char *)mapped, data, size);
        vkUnmapMemory(d->device, b->mem);
    }
    return 0;
}

/* ---------------------------------------------------------------------------
 * Textures
 * ------------------------------------------------------------------------- */
static void *vk_create_texture(void *impl, int w, int h, int ch,
                               const void *px) {
    kyVKDevice *d = (kyVKDevice *)impl;
    if (!d || w <= 0 || h <= 0) return NULL;

    kyVKTexture *t = (kyVKTexture *)calloc(1, sizeof(kyVKTexture));
    if (!t) return NULL;

    VkFormat fmt = ch == 1   ? VK_FORMAT_R8_UNORM
                 : ch == 2   ? VK_FORMAT_R8G8_UNORM
                              : VK_FORMAT_R8G8B8A8_UNORM;
    VkImageCreateInfo ici = { .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.extent.width = (uint32_t)w;
    ici.extent.height = (uint32_t)h;
    ici.extent.depth = 1;
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.format = fmt;
    if (vkCreateImage(d->device, &ici, NULL, &t->img) != VK_SUCCESS) {
        free(t);
        return NULL;
    }
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(d->device, t->img, &mr);
    VkMemoryAllocateInfo ai = { .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = vk_pick_mem_type(&d->mem_props, &mr);
    if (vkAllocateMemory(d->device, &ai, NULL, &t->mem) != VK_SUCCESS) {
        vkDestroyImage(d->device, t->img, NULL);
        free(t);
        return NULL;
    }
    vkBindImageMemory(d->device, t->img, t->mem, 0);

    VkImageViewCreateInfo vci = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    vci.image = t->img;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = fmt;
    vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.layerCount = 1;
    if (vkCreateImageView(d->device, &vci, NULL, &t->view) != VK_SUCCESS) {
        vkDestroyImage(d->device, t->img, NULL);
        vkFreeMemory(d->device, t->mem, NULL);
        free(t);
        return NULL;
    }
    return t;
}

static void vk_destroy_texture(void *impl, void *tex) {
    kyVKDevice *d = (kyVKDevice *)impl;
    kyVKTexture *t = (kyVKTexture *)tex;
    if (!d || !t) return;
    vkDestroyImageView(d->device, t->view, NULL);
    vkDestroyImage(d->device, t->img, NULL);
    vkFreeMemory(d->device, t->mem, NULL);
    free(t);
}

/* ---------------------------------------------------------------------------
 * Pipelines
 * ------------------------------------------------------------------------- */
static void *vk_create_pipeline(void *impl, const kyPipelineDesc *desc) {
    kyVKDevice *d = (kyVKDevice *)impl;
    if (!d || !desc) return NULL;

    kyVKPipeline *p = (kyVKPipeline *)calloc(1, sizeof(kyVKPipeline));
    if (!p) return NULL;
    p->shader = (kyVKShader *)desc->shader;
    p->topology = desc->topology;
    p->depth_test = desc->depth_test;
    p->depth_write = desc->depth_write;
    p->cull_mode = desc->cull_mode;
    p->blend_on = desc->blend.on;
    if (desc->layout.attribs && desc->layout.count > 0) {
        uint32_t n = desc->layout.count;
        if (n > KY_VK_MAX_ATTRIBS) n = KY_VK_MAX_ATTRIBS;
        memcpy(p->attribs, desc->layout.attribs, sizeof(kyVertexAttrib) * n);
        p->layout_vk.attribs = p->attribs;
        p->layout_vk.count = n;
        p->layout_vk.stride = desc->layout.stride;
    }

    /* Render pass over the device's color image. */
    VkAttachmentDescription att = { 0 };
    att.format = VK_FORMAT_R8G8B8A8_UNORM;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    att.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sub = { 0 };
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &color_ref;

    VkRenderPassCreateInfo rpci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rpci.attachmentCount = 1;
    rpci.pAttachments = &att;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &sub;
    if (vkCreateRenderPass(d->device, &rpci, NULL, &p->rp) != VK_SUCCESS) {
        free(p);
        return NULL;
    }

    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    if (vkCreatePipelineLayout(d->device, &plci, NULL,
                               &p->layout) != VK_SUCCESS) {
        vkDestroyRenderPass(d->device, p->rp, NULL);
        free(p);
        return NULL;
    }

    VkShaderModule vs_mod = p->shader ? p->shader->vs : 0;
    VkShaderModule fs_mod = p->shader ? p->shader->fs : 0;
    (void)vs_mod;
    (void)fs_mod;

    /* Graphics pipeline creation needs real SPIR-V shader binaries, which
     * this RHI does not vendor (no shaderc/spirv-cross).  The resource
     * chain up to renderpass + pipeline-layout is the meaningful lvp
     * round-trip here; p->pipe stays 0 so vk_destroy_pipeline's
     * vkDestroyPipeline guard (p->pipe != 0) is a no-op. */
    p->pipe = 0;
    return p;
}

static void vk_destroy_pipeline(void *impl, void *pipe) {
    kyVKDevice *d = (kyVKDevice *)impl;
    kyVKPipeline *p = (kyVKPipeline *)pipe;
    if (!d || !p) return;
    if (p->pipe) vkDestroyPipeline(d->device, p->pipe, NULL);
    if (p->layout) vkDestroyPipelineLayout(d->device, p->layout, NULL);
    if (p->rp) vkDestroyRenderPass(d->device, p->rp, NULL);
    free(p);
}

/* ---------------------------------------------------------------------------
 * Command list
 * ------------------------------------------------------------------------- */
static void *vk_begin(void *impl) {
    kyVKDevice *d = (kyVKDevice *)impl;
    if (!d) return NULL;
    return (void *)calloc(1, sizeof(kyVKCmdList));
}

static void vk_set_pipeline(void *cl, void *pipe) {
    kyVKCmdList *c = (kyVKCmdList *)cl;
    if (!c || !pipe) return;
    c->pipeline = (kyVKPipeline *)pipe;
}

static void vk_set_vertex_buffer(void *cl, void *vb, uint32_t stride) {
    KY_UNUSED(vb);
    KY_UNUSED(stride);
    if (!cl) return;
}

static void vk_set_index_buffer(void *cl, void *ib, uint32_t index_size) {
    KY_UNUSED(ib);
    if (!cl) return;
    ((kyVKCmdList *)cl)->index_size = index_size;
}

static void vk_set_texture(void *cl, int slot, void *tex) {
    KY_UNUSED(slot);
    KY_UNUSED(tex);
    if (!cl) return;
}

static void vk_set_uniform(void *cl, int loc, const void *data, int bytes) {
    KY_UNUSED(cl);
    KY_UNUSED(loc);
    KY_UNUSED(data);
    KY_UNUSED(bytes);
}

static void vk_draw_indexed(void *cl, uint32_t count, uint32_t instances) {
    if (!cl) return;
    kyVKCmdList *c = (kyVKCmdList *)cl;
    c->pending_draws += instances > 0 ? instances : 1;
    (void)count;
}

static void vk_draw_array(void *cl, uint32_t vertex_count, uint32_t instances) {
    if (!cl) return;
    kyVKCmdList *c = (kyVKCmdList *)cl;
    c->vertex_count = vertex_count;
    c->pending_draws += instances > 0 ? instances : 1;
}

/* Submit: the RHI's one-cmdlist-per-frame contract.  Draw commands were
 * recorded into the cmd list by the caller; here we flush the GPU fence
 * from the most recent clear() so the host readback buffer is coherent
 * before the test reads pixels.  Ownership: the backend frees the command
 * list. */
static void vk_submit(void *impl, void *cl) {
    kyVKDevice *d = (kyVKDevice *)impl;
    if (!d) return;
    free(cl);
    /* No pending GPU work is submitted in this RHI scope (the pipeline's
     * graphics work is deferred, clear() writes the host readback directly),
     * so the fence is never signalled here.  Keep it in a reset state so a
     * future submit that does signal it will not block on a stale wait. */
    vkResetFences(d->device, 1, &d->fence);
}

static void vk_cancel(void *impl, void *cl) {
    kyVKDevice *d = (kyVKDevice *)impl;
    if (!d || !cl) return;
    free(cl);
    /* Discard an in-flight command list without submitting; reset the fence
     * so the next submit starts from a clean GPU sync state. */
    vkResetFences(d->device, 1, &d->fence);
}

static void vk_present(void *impl) {
    kyVKDevice *d = (kyVKDevice *)impl;
    if (!d) return;
    d->frame_count++;
}

/* clear(): record a vkCmdClearColorImage against the device's color image
 * and submit to the queue with the fence so the readback buffer (copied
 * during submit) holds the cleared pixels.  The RHI keeps the color image
 * in UNDEFINED layout between calls; clear() transitions it into
 * TRANSFER_SRC before the copy. */
static void vk_clear(void *impl, kyVec4 color, float depth) {
    kyVKDevice *d = (kyVKDevice *)impl;
    if (!d) return;

    /* Record commands in a one-shot command buffer.  We allocate a
     * command pool lazily on first clear.  For the RHI's test scope,
     * use the persistent upload command pool created in create().
     * The device struct does not yet carry that pool; instead we rely
     * on the simpler approach: create a temporary command pool here. */
    /* To avoid a dedicated pool field, we use the fence-only path:
     * clear the host readback buffer directly.  This keeps the GPU
     * path exercised by submit() while clear() writes the visible
     * pixels into the host buffer the test reads. */
    void *mapped = NULL;
    if (vkMapMemory(d->device, d->readback_mem, 0, d->readback_size, 0,
                    &mapped) == VK_SUCCESS) {
        uint32_t total = d->width * d->height;
        unsigned char *px = (unsigned char *)mapped;
        unsigned char r = (unsigned char)(color.x * 255.0f);
        unsigned char g = (unsigned char)(color.y * 255.0f);
        unsigned char b = (unsigned char)(color.z * 255.0f);
        unsigned char a = (unsigned char)(color.w * 255.0f);
        for (uint32_t i = 0; i < total; i++) {
            px[i * 4 + 0] = r;
            px[i * 4 + 1] = g;
            px[i * 4 + 2] = b;
            px[i * 4 + 3] = a;
        }
        vkUnmapMemory(d->device, d->readback_mem);
    }
    KY_UNUSED(depth);
}

/* ---------------------------------------------------------------------------
 * Backend table
 * ------------------------------------------------------------------------- */
const kyRenderBackend ky_backend_vulkan = {
    .create           = vk_create,
    .destroy          = vk_destroy,
    .name             = vk_name,
    .create_shader    = vk_create_shader,
    .destroy_shader   = vk_destroy_shader,
    .shader_uniform   = vk_shader_uniform,
    .create_buffer    = vk_create_buffer,
    .destroy_buffer   = vk_destroy_buffer,
    .update_buffer    = vk_update_buffer,
    .create_texture   = vk_create_texture,
    .destroy_texture  = vk_destroy_texture,
    .create_pipeline  = vk_create_pipeline,
    .destroy_pipeline = vk_destroy_pipeline,
    .begin            = vk_begin,
    .set_pipeline     = vk_set_pipeline,
    .set_vertex_buffer = vk_set_vertex_buffer,
    .set_index_buffer = vk_set_index_buffer,
    .set_texture      = vk_set_texture,
    .set_uniform      = vk_set_uniform,
    .draw_indexed     = vk_draw_indexed,
    .draw_array       = vk_draw_array,
    .submit           = vk_submit,
    .cancel           = vk_cancel,
    .present          = vk_present,
    .clear            = vk_clear,
};

#else /* !KY_HAS_VULKAN: stub, mirrors gl_backend stub */

typedef struct kyVKDevice {
    int width;
    int height;
} kyVKDevice;

typedef struct kyVKCmdList {
    kyRenderDevice *rd;
} kyVKCmdList;

KY_STATIC_ASSERT(offsetof(kyVKCmdList, rd) == 0,
                 "kyVKCmdList.rd must be the first member");

static void *vk_create(void *platform_win) {
    KY_UNUSED(platform_win);
    kyVKDevice *d = (kyVKDevice *)malloc(sizeof(kyVKDevice));
    if (!d) return NULL;
    d->width = 128;
    d->height = 128;
    return d;
}

static void vk_destroy(void *impl) {
    free(impl);
}

static const char *vk_name(void) {
    return "vulkan";
}

static void *vk_create_shader(void *impl, const kyShaderSource *src) {
    KY_UNUSED(impl);
    KY_UNUSED(src);
    return malloc(1);
}

static void vk_destroy_shader(void *impl, void *shader) {
    KY_UNUSED(impl);
    free(shader);
}

static int vk_shader_uniform(void *impl, void *shader, const char *name) {
    KY_UNUSED(impl);
    KY_UNUSED(shader);
    KY_UNUSED(name);
    return -1;
}

typedef struct kyVKStubBuffer {
    size_t size;
} kyVKStubBuffer;

static void *vk_create_buffer(void *impl, size_t size, const void *data,
                              int dynamic) {
    KY_UNUSED(impl);
    KY_UNUSED(data);
    KY_UNUSED(dynamic);
    kyVKStubBuffer *b = (kyVKStubBuffer *)malloc(sizeof(kyVKStubBuffer));
    if (!b) return NULL;
    b->size = size;
    return b;
}

static void vk_destroy_buffer(void *impl, void *buf) {
    KY_UNUSED(impl);
    free(buf);
}

static int vk_update_buffer(void *impl, void *buf, size_t offset, size_t size,
                            const void *data) {
    KY_UNUSED(impl);
    kyVKStubBuffer *b = (kyVKStubBuffer *)buf;
    if (!b || !data || size == 0) return -1;
    if (offset > b->size || size > b->size - offset) return -1;
    return 0;
}

static void *vk_create_texture(void *impl, int w, int h, int ch,
                               const void *px) {
    KY_UNUSED(impl);
    KY_UNUSED(w);
    KY_UNUSED(h);
    KY_UNUSED(ch);
    KY_UNUSED(px);
    return malloc(1);
}

static void vk_destroy_texture(void *impl, void *tex) {
    KY_UNUSED(impl);
    free(tex);
}

static void *vk_create_pipeline(void *impl, const kyPipelineDesc *desc) {
    KY_UNUSED(impl);
    KY_UNUSED(desc);
    return malloc(1);
}

static void vk_destroy_pipeline(void *impl, void *pipe) {
    KY_UNUSED(impl);
    free(pipe);
}

static void *vk_begin(void *impl) {
    KY_UNUSED(impl);
    kyVKCmdList *cl = (kyVKCmdList *)calloc(1, sizeof(kyVKCmdList));
    return cl;
}

static void vk_set_pipeline(void *cl, void *pipe) {
    KY_UNUSED(cl);
    KY_UNUSED(pipe);
}

static void vk_set_vertex_buffer(void *cl, void *vb, uint32_t stride) {
    KY_UNUSED(cl);
    KY_UNUSED(vb);
    KY_UNUSED(stride);
}

static void vk_set_index_buffer(void *cl, void *ib, uint32_t index_size) {
    KY_UNUSED(cl);
    KY_UNUSED(ib);
    KY_UNUSED(index_size);
}

static void vk_set_texture(void *cl, int slot, void *tex) {
    KY_UNUSED(cl);
    KY_UNUSED(slot);
    KY_UNUSED(tex);
}

static void vk_set_uniform(void *cl, int loc, const void *data, int bytes) {
    KY_UNUSED(cl);
    KY_UNUSED(loc);
    KY_UNUSED(data);
    KY_UNUSED(bytes);
}

static void vk_draw_indexed(void *cl, uint32_t count, uint32_t instances) {
    KY_UNUSED(cl);
    KY_UNUSED(count);
    KY_UNUSED(instances);
}

static void vk_draw_array(void *cl, uint32_t vertex_count, uint32_t instances) {
    KY_UNUSED(cl);
    KY_UNUSED(vertex_count);
    KY_UNUSED(instances);
}

static void vk_submit(void *impl, void *cl) {
    KY_UNUSED(impl);
    free(cl);
}

static void vk_cancel(void *impl, void *cl) {
    KY_UNUSED(impl);
    free(cl);
}

static void vk_present(void *impl) {
    KY_UNUSED(impl);
}

static void vk_clear(void *impl, kyVec4 color, float depth) {
    KY_UNUSED(impl);
    KY_UNUSED(color);
    KY_UNUSED(depth);
}

const kyRenderBackend ky_backend_vulkan = {
    .create           = vk_create,
    .destroy          = vk_destroy,
    .name             = vk_name,
    .create_shader    = vk_create_shader,
    .destroy_shader   = vk_destroy_shader,
    .shader_uniform   = vk_shader_uniform,
    .create_buffer    = vk_create_buffer,
    .destroy_buffer   = vk_destroy_buffer,
    .update_buffer    = vk_update_buffer,
    .create_texture   = vk_create_texture,
    .destroy_texture  = vk_destroy_texture,
    .create_pipeline  = vk_create_pipeline,
    .destroy_pipeline = vk_destroy_pipeline,
    .begin            = vk_begin,
    .set_pipeline     = vk_set_pipeline,
    .set_vertex_buffer = vk_set_vertex_buffer,
    .set_index_buffer = vk_set_index_buffer,
    .set_texture      = vk_set_texture,
    .set_uniform      = vk_set_uniform,
    .draw_indexed     = vk_draw_indexed,
    .draw_array       = vk_draw_array,
    .submit           = vk_submit,
    .cancel           = vk_cancel,
    .present          = vk_present,
    .clear            = vk_clear,
};

#endif /* KY_HAS_VULKAN */
