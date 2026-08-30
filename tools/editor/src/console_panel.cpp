#include <imgui.h>
#include "kronyx/editor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ============================================
 * 控制台面板实现（C++）
 * ============================================ */
enum class LogLevel {
    Error = 0,
    Warning = 1,
    Info = 2,
    Debug = 3
};

struct LogEntry {
    LogLevel level;
    const char *message;
    const char *file;
    int line;
    time_t timestamp;
    LogEntry *next;
};

class ConsolePanel {
public:
    ConsolePanel() : head(nullptr), tail(nullptr), entry_capacity(256) {
        entry_count = 0;
        show_errors = true;
        show_warnings = true;
        show_infos = true;
        show_debugs = false;
        show_clear_button = true;
    }

    ~ConsolePanel() {
        clear();
    }

    void init() {
        /* 初始化逻辑 */
    }

    void destroy() {
        /* 清理逻辑 */
    }

    void update(float dt) {
        KY_UNUSED(dt);
        /* 更新逻辑 */
    }

    void draw() {
        if (ImGui::BeginChild("Console", ImVec2(0, 0), true)) {
            /* 绘制过滤选项 */
