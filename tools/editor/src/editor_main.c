#include <GLFW/glfw3.h>
#include <kronyx/editor.h>
#include <kronyx/render.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================
 * Editor 主窗口和 RHI 集成
 * ============================================ */

typedef struct EditorApp {
    GLFWwindow *window;
    kyRenderDevice *render_device;
    kyShader *grid_shader;
    kyPipeline *grid_pipeline;
    void *command_list;
    
    /* 面板 */
    kyEditorPanel *viewport;
    kyEditorPanel *hierarchy;
    kyEditorPanel *properties;
    kyEditorPanel *console;
    
    /* 渲染资源 */
    float *grid_vertices;
    uint16_t *grid_indices;
    kyBuffer *grid_vertex_buffer;
    kyBuffer *grid_index_buffer;
    
    /* 状态 */
    bool running;
    float last_time;
    int frame_count;
    float fps;
    int grid_index_count;
} EditorApp;

static EditorApp *g_app = NULL;

/* ============================================
 * 网格着色器源码
 * ============================================ */
static const char *grid_vs_source = 
"#version 330 core\n"
"layout(location = 0) in vec3 a_position;\n"
"uniform mat4 u_mvp;\n"
"void main() {\n"
"    gl_Position = u_mvp * vec4(a_position, 1.0);\n"
"}\n";

static const char *grid_fs_source = 
"#version 330 core\n"
"out vec4 FragColor;\n"
"uniform vec4 u_color;\n"
"void main() {\n"
"    FragColor = u_color;\n"
"}\n";

/* ============================================
 * 辅助函数
 * ============================================ */
static void init_grid_geometry(EditorApp *app) {
    int grid_size = 20;
    float spacing = 1.0f;
    int line_count = grid_size * 2 + 1;
    int vertex_count = line_count * 2;
    int index_count = line_count * 2 - 2;
    
    app->grid_vertices = (float *)malloc(vertex_count * 3 * sizeof(float));
    app->grid_indices = (uint16_t *)malloc(index_count * sizeof(uint16_t));
    
    /* 生成网格顶点 */
    float offset = (float)grid_size * spacing / 2.0f;
    int idx = 0;
    
    for (int i = 0; i < line_count; i++) {
        float pos = (float)i * spacing - offset;
        
        /* 垂直线 */
        app->grid_vertices[idx++] = pos;
        app->grid_vertices[idx++] = 0.0f;
        app->grid_vertices[idx++] = -offset;
        
        app->grid_vertices[idx++] = pos;
        app->grid_vertices[idx++] = 0.0f;
        app->grid_vertices[idx++] = offset;
        
        /* 索引 */
        if (i > 0 && i < line_count - 1) {
            int base = (i - 1) * 2;
            app->grid_indices[base] = base;
            app->grid_indices[base + 1] = base + 1;
        }
    }
    
    app->grid_index_count = index_count;
}

static void create_grid_pipeline(EditorApp *app) {
    kyVertexLayout layout = {
        .attribs = (kyVertexAttrib[]){
            {.location = 0, .offset = 0, .size = 3, .normalized = 0}
        },
        .count = 1,
        .stride = sizeof(float) * 3
    };
    
    kyPipelineDesc desc = {
        .shader = app->grid_shader,
        .layout = layout,
        .topology = KY_LINES,
        .depth_test = 1,
        .depth_write = 1,
        .cull_mode = 0,
        .blend = {.on = 0, .color = {0, 0, 0, 0}}
    };
    
    app->grid_pipeline = ky_rd_create_pipeline(app->render_device, &desc);
}

/* ============================================
 * 初始化函数
 * ============================================ */
static bool init_editor(EditorApp *app) {
    /* 初始化 GLFW */
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return false;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    
    /* 创建窗口 */
    app->window = glfwCreateWindow(1280, 720, "Kronyx Editor", NULL, NULL);
    if (!app->window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(app->window);
    glfwSwapInterval(1);
    
    /* 初始化 RHI */
    app->render_device = ky_rd_create(KY_RENDERER_GL, app->window);
    if (!app->render_device) {
        fprintf(stderr, "Failed to create render device\n");
        glfwDestroyWindow(app->window);
        glfwTerminate();
        return false;
    }
    
    printf("Render backend: %s\n", ky_rd_backend_name(app->render_device));
    
    /* 创建网格着色器 */
    kyShaderSource shader_src = {
        .vs = grid_vs_source,
        .fs = grid_fs_source,
        .cs = NULL,
        .entry = "main"
    };
    
    app->grid_shader = ky_rd_create_shader(app->render_device, &shader_src);
    if (!app->grid_shader) {
        fprintf(stderr, "Failed to create grid shader\n");
        ky_rd_destroy(app->render_device);
        glfwDestroyWindow(app->window);
        glfwTerminate();
        return false;
    }
    
    /* 创建网格几何体 */
    init_grid_geometry(app);
    
    /* 创建网格缓冲区 */
    app->grid_vertex_buffer = ky_rd_create_buffer(app->render_device,
        app->grid_index_count * 3 * sizeof(float), app->grid_vertices, 1);
    app->grid_index_buffer = ky_rd_create_buffer(app->render_device,
        app->grid_index_count * sizeof(uint16_t), app->grid_indices, 0);
    
    /* 创建渲染管线 */
    create_grid_pipeline(app);
    
    /* 创建面板 */
    app->viewport = ky_editor_panel_create_viewport();
    app->hierarchy = ky_editor_panel_create_hierarchy();
    app->properties = ky_editor_panel_create_properties();
    app->console = ky_editor_panel_create_console();
    
    if (!app->viewport || !app->hierarchy || !app->properties || !app->console) {
        fprintf(stderr, "Failed to create editor panels\n");
        return false;
    }
    
    /* 初始化面板 */
    app->viewport->vtbl->init(app->viewport, app);
    app->hierarchy->vtbl->init(app->hierarchy, app);
    app->properties->vtbl->init(app->properties, app);
    app->console->vtbl->init(app->console, app);
    
    app->running = true;
    app->last_time = 0.0f;
    app->frame_count = 0;
    app->fps = 0.0f;
    
    printf("Editor initialized successfully\n");
    return true;
}

/* ============================================
 * 清理函数
 * ============================================ */
static void cleanup_editor(EditorApp *app) {
    if (!app) return;
    
    /* 清理面板 */
    if (app->viewport) {
        app->viewport->vtbl->destroy(app->viewport);
        app->viewport = NULL;
    }
    if (app->hierarchy) {
        app->hierarchy->vtbl->destroy(app->hierarchy);
        app->hierarchy = NULL;
    }
    if (app->properties) {
        app->properties->vtbl->destroy(app->properties);
        app->properties = NULL;
    }
    if (app->console) {
        app->console->vtbl->destroy(app->console);
        app->console = NULL;
    }
    
    /* 清理渲染资源 */
    if (app->grid_shader) {
        ky_rd_destroy_shader(app->render_device, app->grid_shader);
        app->grid_shader = NULL;
    }
    if (app->grid_pipeline) {
        ky_rd_destroy_pipeline(app->render_device, app->grid_pipeline);
        app->grid_pipeline = NULL;
    }
    if (app->grid_vertex_buffer) {
        ky_rd_destroy_buffer(app->render_device, app->grid_vertex_buffer);
        app->grid_vertex_buffer = NULL;
    }
    if (app->grid_index_buffer) {
        ky_rd_destroy_buffer(app->render_device, app->grid_index_buffer);
        app->grid_index_buffer = NULL;
    }
    if (app->grid_vertices) {
        free(app->grid_vertices);
        app->grid_vertices = NULL;
    }
    if (app->grid_indices) {
        free(app->grid_indices);
        app->grid_indices = NULL;
    }
    
    /* 清理 RHI */
    if (app->render_device) {
        ky_rd_destroy(app->render_device);
        app->render_device = NULL;
    }
    
    /* 清理窗口 */
    if (app->window) {
        glfwDestroyWindow(app->window);
        app->window = NULL;
    }
    
    glfwTerminate();
    
    printf("Editor cleaned up\n");
}

/* ============================================
 * 渲染场景
 * ============================================ */
static void render_scene(EditorApp *app) {
    /* 清除屏幕 */
    kyVec4 clear_color = {0.125f, 0.125f, 0.125f, 1.0f};
    ky_rd_clear(app->render_device, clear_color, 1.0f);
    
    /* 开始渲染命令 */
    app->command_list = ky_rd_begin(app->render_device);
    
    /* 设置网格管线 */
    ky_cmd_set_pipeline(app->command_list, app->grid_pipeline);
    
    /* 设置顶点缓冲 */
    ky_cmd_set_vertex_buffer(app->command_list, app->grid_vertex_buffer, 
        sizeof(float) * 3);
    
    /* 设置索引缓冲 */
    ky_cmd_set_index_buffer(app->command_list, app->grid_index_buffer, 
        sizeof(uint16_t));
    
    /* 绘制网格 */
    ky_cmd_draw_indexed(app->command_list, app->grid_index_count, 1);
    
    /* 提交渲染命令 */
    ky_rd_submit(app->render_device, app->command_list);
    
    /* 呈现 */
    ky_rd_present(app->render_device);
}

/* ============================================
 * 主循环
 * ============================================ */
static void run_editor(EditorApp *app) {
    double last_time = glfwGetTime();
    double frame_time = 0.0;
    
    printf("Starting editor loop...\n");
    
    while (app->running) {
        double current_time = glfwGetTime();
        float dt = (float)(current_time - app->last_time);
        app->last_time = current_time;
        
        /* 检查事件 */
        if (glfwWindowShouldClose(app->window)) {
            app->running = false;
            break;
        }
        
        /* 处理输入 */
        glfwPollEvents();
        
        /* 更新面板 */
        app->viewport->vtbl->update(app->viewport, dt);
        app->hierarchy->vtbl->update(app->hierarchy, dt);
        app->properties->vtbl->update(app->properties, dt);
        app->console->vtbl->update(app->console, dt);
        
        /* 渲染场景 */
        render_scene(app);
        
        /* 交换缓冲区 */
        glfwSwapBuffers(app->window);
        
        /* 计算 FPS */
        app->frame_count++;
        frame_time += dt;
        if (frame_time >= 1.0) {
            app->fps = (float)app->frame_count / frame_time;
            app->frame_count = 0;
            frame_time = 0.0;
            
            /* 更新窗口标题 */
            char title[128];
            snprintf(title, sizeof(title), 
                "Kronyx Editor - %.1f FPS", app->fps);
            glfwSetWindowTitle(app->window, title);
        }
    }
    
    printf("Editor loop ended (%.1f FPS average)\n", app->fps);
}

/* ============================================
 * 入口函数
 * ============================================ */
int main(int argc, char *argv[]) {
    KY_UNUSED(argc);
    KY_UNUSED(argv);
    
    printf("========================================\n");
    printf("Kronyx Editor - RHI Integration\n");
    printf("========================================\n");
    
    g_app = (EditorApp *)calloc(1, sizeof(EditorApp));
    if (!g_app) {
        fprintf(stderr, "Failed to allocate editor app\n");
        return 1;
    }
    
    /* 初始化 Editor */
    if (!init_editor(g_app)) {
        fprintf(stderr, "Failed to initialize editor\n");
        free(g_app);
        return 1;
    }
    
    /* 运行主循环 */
    run_editor(g_app);
    
    /* 清理 */
    cleanup_editor(g_app);
    free(g_app);
    
    return 0;
}
