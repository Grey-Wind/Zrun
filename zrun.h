#ifndef ZRUN_H
#define ZRUN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// 平台特定的导出宏
#if defined(_WIN32)
#ifdef ZRUN_BUILD_DLL
#define ZRUN_API __declspec(dllexport)
#else
#define ZRUN_API __declspec(dllimport)
#endif
#else
#define ZRUN_API __attribute__((visibility("default")))
#endif

typedef enum {
    ZRUN_SHELL_CMD = 0,
    ZRUN_SHELL_POWERSHELL = 1,
    ZRUN_SHELL_BASH = 2,
    ZRUN_SHELL_SH = 3
} zrun_shell_type;

typedef enum {
    ZRUN_ASYNC_RUNNING = 0,
    ZRUN_ASYNC_COMPLETED = 1,
    ZRUN_ASYNC_FAILED = 2,
    ZRUN_ASYNC_TIMED_OUT = 3,
    ZRUN_ASYNC_CANCELLED = 4
} zrun_async_state;

typedef struct {
    int exit_code;
    char* output;
    char* error;
    int64_t execution_time;
    int timed_out;
} zrun_command_result;

typedef void (*zrun_output_callback)(const char* output, int is_error, void* user_data);

// 创建和销毁实例
ZRUN_API void* zrun_create(void);
ZRUN_API void zrun_destroy(void* instance);

// 同步执行命令
ZRUN_API zrun_command_result zrun_execute_sync(void* instance, const char* command,
                                               zrun_shell_type shell_type, int timeout_ms);

// 异步执行命令
ZRUN_API int zrun_execute_async(void* instance, const char* command,
                                zrun_shell_type shell_type, int timeout_ms,
                                zrun_output_callback callback, void* user_data);

// 异步命令管理
ZRUN_API zrun_async_state zrun_get_async_status(void* instance, int async_id);
ZRUN_API int zrun_get_async_result(void* instance, int async_id, zrun_command_result* result);
ZRUN_API int zrun_terminate_async(void* instance, int async_id);

// 配置
ZRUN_API void zrun_set_working_directory(void* instance, const char* directory);
ZRUN_API void zrun_set_environment(void* instance, const char* key, const char* value);
ZRUN_API void zrun_set_execution_policy(void* instance, const char* policy);
ZRUN_API void zrun_clear_environment(void* instance);

// 资源清理
ZRUN_API void zrun_free_result(zrun_command_result result);

#ifdef __cplusplus
}
#endif

#endif // ZRUN_H
