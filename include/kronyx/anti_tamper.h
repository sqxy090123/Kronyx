/*
 * KyAntiTamper - 跨平台反篡改与完整性验证
 *
 * 两个组件：
 *   1. ky_antitamper (静态库)    -- 嵌入游戏 exe，负责加载验证库
 *   2. KyAntiTamper.dll/.so/.dylib -- 独立验证库，执行多轮 HMAC+TOTP 挑战响应
 *
 * 流程：
 *   游戏 exe 启动 → 加载验证库 → 运行多轮 HMAC-SHA256 + TOTP 挑战响应
 *   → 验证验证库自身完整性 → 预分配游戏内存池
 *
 * 开发者密钥模式（环境变量 KY_ANTITEMPER_DEV_KEY）：
 *   "0" 或未设置       : 标准模式（无开发者密钥）
 *   "release:<hex64>"  : 生产密钥
 *   "dev:<hex64>"      : 开发密钥（跳过部分检查）
 *   "<hex64>"          : 直接写入 32 字节密钥
 */

#ifndef KY_ANTITEMPER_H
#define KY_ANTITEMPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* ---- 返回值 -------------------------------------------------------------- */

typedef enum {
    KY_TAMPER_OK                    =  0,   /* 验证通过                 */
    KY_TAMPER_FAIL_LICENSE          = -1,   /* 许可证/密钥无效           */
    KY_TAMPER_FAIL_INTEGRITY        = -2,   /* 验证库完整性校验失败       */
    KY_TAMPER_FAIL_HMAC             = -3,   /* HMAC 校验不匹配           */
    KY_TAMPER_FAIL_TOTP             = -4,   /* TOTP 校验不匹配           */
    KY_TAMPER_FAIL_RANDOM           = -5,   /* Salt 生成失败             */
    KY_TAMPER_FAIL_TIMEOUT          = -6,   /* 验证超时                  */
    KY_TAMPER_FAIL_MISSING_LIB      = -7,   /* 验证库未找到              */
    KY_TAMPER_FAIL_NO_SYMBOL        = -8,   /* 验证库缺少导出函数        */
    KY_TAMPER_FAIL_NOT_VERIFIED     = -9,   /* 尚未调用 init             */
    KY_TAMPER_FAIL_ALREADY_DONE      = -10,  /* 已初始化                  */
} KyTamperResult;

typedef enum {
    KY_TAMPER_MODE_ERROR     = 0,   /* 硬失败：中止启动            */
    KY_TAMPER_MODE_WARNING = 1,   /* 软失败：显示警告后继续       */
} KyTamperMode;

/* ---- 回调函数（由游戏 exe 提供）------------------------------------------ */

typedef void (*KyTamperLogFn)(int level, const char *msg);
/* level: 0=错误, 1=警告, 2=信息, 3=调试 */

typedef int  (*KyTamperDialogFn)(int type, const char *title,
                                  const char *message);
/* type: 0=错误, 1=警告. 返回 0=继续, 1=中止 */

typedef void*(*KyTamperAllocFn)(size_t size);
typedef void (*KyTamperFreeFn)(void *ptr);

/* ---- 平台导出宏 ---------------------------------------------------------- */

#ifdef _WIN32
  #ifdef KY_ANTITEMPER_BUILDING_LIB
    #define KY_ANTITEMPER_API __declspec(dllexport)
  #else
    #define KY_ANTITEMPER_API __declspec(dllimport)
  #endif
#else
  #define KY_ANTITEMPER_API __attribute__((visibility("default")))
#endif

/* ---- 验证库导出函数（供游戏 exe 动态加载）------------------------------- */

/*
 * 执行反篡改验证的核心函数。
 * 由 KyAntiTamper.dll/.so/.dylib 导出，游戏 exe 通过 dlopen/dlsym 调用。
 */
KY_ANTITEMPER_API int ky_tamper_verify(
        const uint8_t *salt, size_t salt_len,
        const uint8_t *hmac_in,
        const uint8_t *totp_in,
        uint8_t *hmac_out,
        uint8_t *totp_out,
        size_t out_len,
        uint8_t *salt_out);

KY_ANTITEMPER_API const char* ky_tamper_version(void);

/* ---- 公共 API（游戏 exe 调用）------------------------------------------- */

/* 在调用 Init 前设置回调。可选（未设置则使用默认行为）。 */
KY_ANTITEMPER_API void  ky_tamper_set_log_cb      (KyTamperLogFn fn);
KY_ANTITEMPER_API void  ky_tamper_set_dialog_cb   (KyTamperDialogFn fn);
KY_ANTITEMPER_API void  ky_tamper_set_alloc_cb    (KyTamperAllocFn fn);
KY_ANTITEMPER_API void  ky_tamper_set_free_cb     (KyTamperFreeFn fn);

/*
 * 初始化反篡改验证。从同目录加载验证库，运行多轮 HMAC+TOTP 挑战响应。
 *
 * mode:          KY_TAMPER_MODE_ERROR 或 KY_TAMPER_MODE_WARNING
 * prealloc_bytes: 游戏内存池大小（0 = 不预分配）
 *
 * 返回 KY_TAMPER_OK 表示验证成功。
 */
KY_ANTITEMPER_API KyTamperResult ky_tamper_init(KyTamperMode mode,
                                                 size_t prealloc_bytes);

/* 释放预分配的游戏内存池。在关闭时调用。 */
KY_ANTITEMPER_API void ky_tamper_shutdown(void);

/* 检查验证是否已完成（无论结果）。 */
KY_ANTITEMPER_API int  ky_tamper_is_verified(void);

/* 获取最后一次验证结果。 */
KY_ANTITEMPER_API KyTamperResult ky_tamper_last_result(void);

#ifdef __cplusplus
}
#endif
#endif  /* KY_ANTITEMPER_H */
