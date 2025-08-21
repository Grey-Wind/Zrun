#ifndef ZRUN_CORE_H
#define ZRUN_CORE_H

#include "zrun_types.h"
#include <mutex>
#include <memory>
#include <map>

namespace Zrun {

class CoreImpl {
public:
    CoreImpl();
    ~CoreImpl();

    // 禁止拷贝和赋值
    CoreImpl(const CoreImpl&) = delete;
    CoreImpl& operator=(const CoreImpl&) = delete;

    // 同步执行命令
    CommandResult executeSync(const std::string& command,
                              ShellType shellType = ShellType::PowerShell,
                              int timeoutMs = 30000);

    // 异步执行命令
    int executeAsync(const std::string& command,
                     ShellType shellType = ShellType::PowerShell,
                     int timeoutMs = 30000,
                     OutputCallback outputCallback = nullptr);

    // 检查异步命令状态
    AsyncState getAsyncStatus(int asyncId);

    // 获取异步命令结果
    bool getAsyncResult(int asyncId, CommandResult& result);

    // 终止异步命令
    bool terminateAsync(int asyncId);

    // 设置工作目录
    void setWorkingDirectory(const std::string& directory);

    // 设置环境变量
    void setEnvironment(const std::string& key, const std::string& value);
    void setEnvironment(const std::map<std::string, std::string>& environment);

    // 设置执行策略 (PowerShell)
    void setExecutionPolicy(const std::string& policy);

    // 清除所有环境变量设置
    void clearEnvironment();

private:
    struct AsyncCommand;

    std::string buildShellCommand(const std::string& command, ShellType shellType);
    void asyncExecutionThread(std::shared_ptr<AsyncCommand> cmd);
    static int nextAsyncId();

    // 平台特定的实现
    CommandResult executeSyncWindows(const std::string& command, ShellType shellType, int timeoutMs);
    CommandResult executeSyncUnix(const std::string& command, ShellType shellType, int timeoutMs);

    std::string m_workingDirectory;
    std::map<std::string, std::string> m_environment;
    std::string m_executionPolicy;

    std::map<int, std::shared_ptr<AsyncCommand>> m_asyncCommands;
    std::mutex m_asyncMutex;
    static std::atomic<int> s_nextAsyncId;
};

} // namespace Zrun

#endif // ZRUN_CORE_H
