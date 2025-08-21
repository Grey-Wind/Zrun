#include "zrun.h"
#include "zrun_core.h"
#include <string>

// 确保在编译 DLL 时正确导出函数
#if defined(_WIN32) && defined(ZRUN_BUILD_DLL)
#define ZRUN_API __declspec(dllexport)
#else
#define ZRUN_API
#endif

// C接口包装器
struct ZRunInstance {
    Zrun::CoreImpl impl;
};

// 辅助函数：将C字符串转换为std::string
static std::string toStdString(const char* str) {
    return str ? std::string(str) : std::string();
}

// 辅助函数：将std::string转换为C字符串（需要调用者释放）
static char* toCString(const std::string& str) {
    char* cstr = new char[str.size() + 1];
    std::strcpy(cstr, str.c_str());
    return cstr;
}

// 辅助函数：将CommandResult转换为zrun_command_result
static zrun_command_result toCResult(const Zrun::CommandResult& result) {
    zrun_command_result cresult;
    cresult.exit_code = result.exitCode;
    cresult.output = toCString(result.output);
    cresult.error = toCString(result.error);
    cresult.execution_time = result.executionTime;
    cresult.timed_out = result.timedOut ? 1 : 0;
    return cresult;
}

// 辅助函数：将zrun_shell_type转换为Zrun::ShellType
static Zrun::ShellType toCppShellType(zrun_shell_type shell_type) {
    switch (shell_type) {
    case ZRUN_SHELL_CMD: return Zrun::ShellType::CMD;
    case ZRUN_SHELL_POWERSHELL: return Zrun::ShellType::PowerShell;
    case ZRUN_SHELL_BASH: return Zrun::ShellType::Bash;
    case ZRUN_SHELL_SH: return Zrun::ShellType::Sh;
    default: return Zrun::ShellType::PowerShell;
    }
}

// 辅助函数：将Zrun::AsyncState转换为zrun_async_state
static zrun_async_state toCAsyncState(Zrun::AsyncState state) {
    switch (state) {
    case Zrun::AsyncState::Running: return ZRUN_ASYNC_RUNNING;
    case Zrun::AsyncState::Completed: return ZRUN_ASYNC_COMPLETED;
    case Zrun::AsyncState::Failed: return ZRUN_ASYNC_FAILED;
    case Zrun::AsyncState::TimedOut: return ZRUN_ASYNC_TIMED_OUT;
    case Zrun::AsyncState::Cancelled: return ZRUN_ASYNC_CANCELLED;
    default: return ZRUN_ASYNC_FAILED;
    }
}

extern "C" {

ZRUN_API void* zrun_create(void) {
    try {
        return new ZRunInstance();
    } catch (...) {
        return nullptr;
    }
}

ZRUN_API void zrun_destroy(void* instance) {
    if (instance) {
        delete static_cast<ZRunInstance*>(instance);
    }
}

ZRUN_API zrun_command_result zrun_execute_sync(void* instance, const char* command,
                                               zrun_shell_type shell_type, int timeout_ms) {
    if (!instance || !command) {
        zrun_command_result result;
        result.exit_code = -1;
        result.output = toCString("");
        result.error = toCString("Invalid arguments");
        result.execution_time = 0;
        result.timed_out = 0;
        return result;
    }

    try {
        ZRunInstance* zrun = static_cast<ZRunInstance*>(instance);
        auto result = zrun->impl.executeSync(toStdString(command), toCppShellType(shell_type), timeout_ms);
        return toCResult(result);
    } catch (const std::exception& e) {
        zrun_command_result result;
        result.exit_code = -1;
        result.output = toCString("");
        result.error = toCString(std::string("Exception: ") + e.what());
        result.execution_time = 0;
        result.timed_out = 0;
        return result;
    } catch (...) {
        zrun_command_result result;
        result.exit_code = -1;
        result.output = toCString("");
        result.error = toCString("Unknown exception");
        result.execution_time = 0;
        result.timed_out = 0;
        return result;
    }
}

ZRUN_API int zrun_execute_async(void* instance, const char* command,
                                zrun_shell_type shell_type, int timeout_ms,
                                zrun_output_callback callback, void* user_data) {
    if (!instance || !command) {
        return -1;
    }

    try {
        ZRunInstance* zrun = static_cast<ZRunInstance*>(instance);

        Zrun::OutputCallback outputCallback;
        if (callback) {
            outputCallback = [callback, user_data](const std::string& output, bool isError) {
                callback(output.c_str(), isError ? 1 : 0, user_data);
            };
        }

        return zrun->impl.executeAsync(
            toStdString(command),
            toCppShellType(shell_type),
            timeout_ms,
            outputCallback
            );
    } catch (...) {
        return -1;
    }
}

ZRUN_API zrun_async_state zrun_get_async_status(void* instance, int async_id) {
    if (!instance) {
        return ZRUN_ASYNC_FAILED;
    }

    try {
        ZRunInstance* zrun = static_cast<ZRunInstance*>(instance);
        auto state = zrun->impl.getAsyncStatus(async_id);
        return toCAsyncState(state);
    } catch (...) {
        return ZRUN_ASYNC_FAILED;
    }
}

ZRUN_API int zrun_get_async_result(void* instance, int async_id, zrun_command_result* result) {
    if (!instance || !result) {
        return 0;
    }

    try {
        ZRunInstance* zrun = static_cast<ZRunInstance*>(instance);
        Zrun::CommandResult cppResult;

        if (zrun->impl.getAsyncResult(async_id, cppResult)) {
            *result = toCResult(cppResult);
            return 1;
        }

        return 0;
    } catch (...) {
        return 0;
    }
}

ZRUN_API int zrun_terminate_async(void* instance, int async_id) {
    if (!instance) {
        return 0;
    }

    try {
        ZRunInstance* zrun = static_cast<ZRunInstance*>(instance);
        return zrun->impl.terminateAsync(async_id) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

ZRUN_API void zrun_set_working_directory(void* instance, const char* directory) {
    if (instance && directory) {
        ZRunInstance* zrun = static_cast<ZRunInstance*>(instance);
        zrun->impl.setWorkingDirectory(toStdString(directory));
    }
}

ZRUN_API void zrun_set_environment(void* instance, const char* key, const char* value) {
    if (instance && key && value) {
        ZRunInstance* zrun = static_cast<ZRunInstance*>(instance);
        zrun->impl.setEnvironment(toStdString(key), toStdString(value));
    }
}

ZRUN_API void zrun_set_execution_policy(void* instance, const char* policy) {
    if (instance && policy) {
        ZRunInstance* zrun = static_cast<ZRunInstance*>(instance);
        zrun->impl.setExecutionPolicy(toStdString(policy));
    }
}

ZRUN_API void zrun_clear_environment(void* instance) {
    if (instance) {
        ZRunInstance* zrun = static_cast<ZRunInstance*>(instance);
        zrun->impl.clearEnvironment();
    }
}

ZRUN_API void zrun_free_result(zrun_command_result result) {
    delete[] result.output;
    delete[] result.error;
}

} // extern "C"
