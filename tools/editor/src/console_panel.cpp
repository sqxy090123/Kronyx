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

    void add_entry(LogLevel level, const char *message, const char *file, int line) {
        LogEntry *entry = (LogEntry *)malloc(sizeof(LogEntry));
        if (!entry) return;
        entry->level = level;
        entry->message = message ? strdup(message) : strdup("");
        entry->file = file ? strdup(file) : strdup("");
        entry->line = line;
        entry->timestamp = time(NULL);
        entry->next = nullptr;

        if (tail) {
            tail->next = entry;
        } else {
            head = entry;
        }
        tail = entry;
        entry_count++;
    }

    void draw() {
        if (ImGui::BeginChild("Console", ImVec2(0, 0), true)) {
            /* 绘制过滤选项 */
            ImGui::Checkbox("Errors", &show_errors);
            ImGui::SameLine();
            ImGui::Checkbox("Warnings", &show_warnings);
            ImGui::SameLine();
            ImGui::Checkbox("Info", &show_infos);
            ImGui::SameLine();
            ImGui::Checkbox("Debug", &show_debugs);
            if (show_clear_button) {
                ImGui::SameLine();
                if (ImGui::Button("Clear")) {
                    clear();
                }
            }
            ImGui::Separator();

            /* 绘制日志条目 */
            if (ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), true)) {
                for (LogEntry *e = head; e != nullptr; e = e->next) {
                    if (e->level == LogLevel::Error && !show_errors) continue;
                    if (e->level == LogLevel::Warning && !show_warnings) continue;
                    if (e->level == LogLevel::Info && !show_infos) continue;
                    if (e->level == LogLevel::Debug && !show_debugs) continue;

                    ImVec4 color;
                    switch (e->level) {
                        case LogLevel::Error:   color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); break;
                        case LogLevel::Warning: color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); break;
                        case LogLevel::Info:    color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); break;
                        default:                color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); break;
                    }
                    ImGui::TextColored(color, "%s", e->message);
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
    }

    void toggle(bool *minimized) {
        KY_UNUSED(minimized);
        /* 预留：最小化切换 */
    }

    void clear() {
        LogEntry *e = head;
        while (e) {
            LogEntry *next = e->next;
            free((void *)e->message);
            free((void *)e->file);
            free(e);
            e = next;
        }
        head = nullptr;
        tail = nullptr;
        entry_count = 0;
    }

    LogEntry *head;
    LogEntry *tail;
    size_t entry_count;
    size_t entry_capacity;
    bool show_errors;
    bool show_warnings;
    bool show_infos;
    bool show_debugs;
    bool show_clear_button;
};

/* 控制台面板虚表实现 */
static const kyEditorPanelVtbl console_panel_vtbl = {
    .name = "Console",
    .icon = ">",
    .init = [](kyEditorPanel *panel, void *user) -> void {
        KY_UNUSED(user);
        ConsolePanel *impl = new ConsolePanel();
        impl->init();
        panel->impl = impl;
    },
    .destroy = [](kyEditorPanel *panel) -> void {
        if (panel && panel->impl) {
            ConsolePanel *cp = static_cast<ConsolePanel*>(panel->impl);
            cp->destroy();
            delete cp;
            panel->impl = nullptr;
        }
    },
    .update = [](kyEditorPanel *panel, float dt) -> void {
        if (panel && panel->impl) {
            ConsolePanel *cp = static_cast<ConsolePanel*>(panel->impl);
            cp->update(dt);
        }
    },
    .draw = [](kyEditorPanel *panel) -> void {
        if (panel && panel->impl) {
            ConsolePanel *cp = static_cast<ConsolePanel*>(panel->impl);
            cp->draw();
        }
    },
    .toggle = [](kyEditorPanel *panel, bool *minimized) -> void {
        if (panel && panel->impl) {
            ConsolePanel *cp = static_cast<ConsolePanel*>(panel->impl);
            cp->toggle(minimized);
        }
    },
    .config = nullptr,
    .reorder = nullptr,
    .reserved = {0}
};

/* 函数实现移至 editor.c */

KY_API void ky_editor_console_info(kyEditorPanel *console, const char *msg, const char *file, int line) {
    if (console && console->impl) {
        ConsolePanel *cp = static_cast<ConsolePanel*>(console->impl);
        cp->add_entry(LogLevel::Info, msg, file, line);
    }
}

KY_API void ky_editor_console_warning(kyEditorPanel *console, const char *msg, const char *file, int line) {
    if (console && console->impl) {
        ConsolePanel *cp = static_cast<ConsolePanel*>(console->impl);
        cp->add_entry(LogLevel::Warning, msg, file, line);
    }
}

KY_API void ky_editor_console_error(kyEditorPanel *console, const char *msg, const char *file, int line) {
    if (console && console->impl) {
        ConsolePanel *cp = static_cast<ConsolePanel*>(console->impl);
        cp->add_entry(LogLevel::Error, msg, file, line);
    }
}
