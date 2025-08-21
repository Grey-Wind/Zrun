#ifndef ZRUN_CPP_H
#define ZRUN_CPP_H

#include "zrun_types.h"
#include <memory>
#include <map>

namespace Zrun {

class ZRun {
public:
    ZRun();
    ~ZRun();

    // 禁止拷贝和赋值
    ZRun(const ZRun&) = delete;
    ZRun& operator=(const ZRun&) = delete;

    // 同步执行命令
    CommandResult executeSync(const std::string& command,
                              ShellType shellType = ShellType::PowerShell,
                              int timeoutMs = 30000);

    // 异步执行命令
    int executeAsync(const std::string& command,
                     ShellType shellType = ShellType::PowerShell,
                     int timeoutMs = 30000,
                     OutputCallback callback = nullptr);

    // 获取异步命令状态
    AsyncState getAsyncStatus(int asyncId);

    // 获取异步命令结果
    CommandResult getAsyncResult(int asyncId);

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
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Zrun

#endif // ZRUN_CPP_H
