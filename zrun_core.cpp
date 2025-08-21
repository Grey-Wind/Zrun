#include "zrun_core.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#endif

namespace Zrun {

std::atomic<int> CoreImpl::s_nextAsyncId(1);

struct CoreImpl::AsyncCommand {
    int id;
    std::string command;
    ShellType shellType;
    int timeoutMs;
    OutputCallback outputCallback;
    std::thread thread;
    std::atomic<AsyncState> state{AsyncState::Running};
    CommandResult result;
    std::mutex mutex;
    std::condition_variable cv;
    bool cancelled{false};

    AsyncCommand(int id, std::string cmd, ShellType type, int timeout, OutputCallback cb)
        : id(id), command(std::move(cmd)), shellType(type), timeoutMs(timeout),
        outputCallback(std::move(cb)) {}

    ~AsyncCommand() {
        if (thread.joinable()) {
            if (state == AsyncState::Running) {
                cancelled = true;
                // 给线程一点时间正常退出
                if (thread.joinable()) {
                    thread.detach();
                }
            } else {
                thread.join();
            }
        }
    }
};

CoreImpl::CoreImpl() = default;

CoreImpl::~CoreImpl() {
    // 清理所有异步命令
    std::lock_guard<std::mutex> lock(m_asyncMutex);
    for (auto& pair : m_asyncCommands) {
        if (pair.second->state == AsyncState::Running) {
            pair.second->cancelled = true;
            // 尝试正常终止
            terminateAsync(pair.first);
        }
    }
    m_asyncCommands.clear();
}

CommandResult CoreImpl::executeSync(const std::string& command,
                                    ShellType shellType,
                                    int timeoutMs) {
#ifdef _WIN32
    return executeSyncWindows(command, shellType, timeoutMs);
#else
    return executeSyncUnix(command, shellType, timeoutMs);
#endif
}

#ifdef _WIN32
CommandResult CoreImpl::executeSyncWindows(const std::string& command,
                                           ShellType shellType,
                                           int timeoutMs) {
    CommandResult result;
    auto startTime = std::chrono::steady_clock::now();

    std::string fullCommand = buildShellCommand(command, shellType);

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE hStdOutRd = nullptr, hStdOutWr = nullptr;
    HANDLE hStdErrRd = nullptr, hStdErrWr = nullptr;

    // 创建管道
    if (!CreatePipe(&hStdOutRd, &hStdOutWr, &sa, 0) ||
        !CreatePipe(&hStdErrRd, &hStdErrWr, &sa, 0)) {
        result.exitCode = -1;
        result.error = "Failed to create pipes";
        return result;
    }

    // 确保读句柄不被继承
    SetHandleInformation(hStdOutRd, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdErrRd, HANDLE_FLAG_INHERIT, 0);

    PROCESS_INFORMATION pi;
    STARTUPINFOA si;
    ZeroMemory(&pi, sizeof(pi));
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hStdOutWr;
    si.hStdError = hStdErrWr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags |= STARTF_USESTDHANDLES;

    // 准备环境变量
    std::string envBlock;
    if (!m_environment.empty()) {
        // 获取当前环境
        LPCH currentEnv = GetEnvironmentStrings();
        if (currentEnv) {
            // 复制当前环境
            for (LPTSTR var = (LPTSTR)currentEnv; *var; var += lstrlen(var) + 1) {
                envBlock.append(var, lstrlen(var));
                envBlock.push_back('\0');
            }
            FreeEnvironmentStrings(currentEnv);
        }

        // 添加自定义环境变量
        for (const auto& pair : m_environment) {
            std::string envVar = pair.first + "=" + pair.second;
            envBlock.append(envVar);
            envBlock.push_back('\0');
        }
        envBlock.push_back('\0');
    }

    // 创建进程
    DWORD flags = CREATE_NO_WINDOW;
    BOOL success = CreateProcessA(
        nullptr,
        const_cast<LPSTR>(fullCommand.c_str()),
        nullptr,
        nullptr,
        TRUE,
        flags,
        m_environment.empty() ? nullptr : &envBlock[0],
        m_workingDirectory.empty() ? nullptr : m_workingDirectory.c_str(),
        &si,
        &pi
        );

    // 关闭不需要的写句柄
    CloseHandle(hStdOutWr);
    CloseHandle(hStdErrWr);
    hStdOutWr = hStdErrWr = nullptr;

    if (!success) {
        result.exitCode = -1;
        result.error = "Failed to create process: " + std::to_string(GetLastError());
        CloseHandle(hStdOutRd);
        CloseHandle(hStdErrRd);
        return result;
    }

    // 等待进程完成或超时
    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        result.timedOut = true;
    } else if (waitResult == WAIT_OBJECT_0) {
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        result.exitCode = static_cast<int>(exitCode);
    }

    // 读取输出
    const int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    DWORD bytesRead;

    while (true) {
        if (!ReadFile(hStdOutRd, buffer, BUFFER_SIZE - 1, &bytesRead, nullptr) || bytesRead == 0) {
            break;
        }
        buffer[bytesRead] = '\0';
        result.output.append(buffer, bytesRead);
    }

    while (true) {
        if (!ReadFile(hStdErrRd, buffer, BUFFER_SIZE - 1, &bytesRead, nullptr) || bytesRead == 0) {
            break;
        }
        buffer[bytesRead] = '\0';
        result.error.append(buffer, bytesRead);
    }

    // 清理资源
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdOutRd);
    CloseHandle(hStdErrRd);

    auto endTime = std::chrono::steady_clock::now();
    result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                               endTime - startTime).count();

    return result;
}
#else
CommandResult CoreImpl::executeSyncUnix(const std::string& command,
                                        ShellType shellType,
                                        int timeoutMs) {
    CommandResult result;
    auto startTime = std::chrono::steady_clock::now();

    std::string fullCommand = buildShellCommand(command, shellType);

    int stdoutPipe[2] = {-1, -1};
    int stderrPipe[2] = {-1, -1};

    if (pipe(stdoutPipe) == -1 || pipe(stderrPipe) == -1) {
        result.exitCode = -1;
        result.error = "Failed to create pipe: " + std::string(strerror(errno));
        return result;
    }

    pid_t pid = fork();
    if (pid == -1) {
        result.exitCode = -1;
        result.error = "Fork failed: " + std::string(strerror(errno));
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        close(stderrPipe[0]);
        close(stderrPipe[1]);
        return result;
    }

    if (pid == 0) { // 子进程
        // 关闭读端
        close(stdoutPipe[0]);
        close(stderrPipe[0]);

        // 重定向标准输出和错误
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);

        // 关闭写端（已经重定向）
        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        // 设置工作目录
        if (!m_workingDirectory.empty() &&
            chdir(m_workingDirectory.c_str()) == -1) {
            exit(127);
        }

        // 设置环境变量
        if (!m_environment.empty()) {
            for (const auto& pair : m_environment) {
                setenv(pair.first.c_str(), pair.second.c_str(), 1);
            }
        }

        // 执行命令
        execl("/bin/sh", "sh", "-c", fullCommand.c_str(), (char*)nullptr);
        exit(127); // execl失败
    }

    // 父进程
    // 关闭写端
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    // 设置非阻塞
    fcntl(stdoutPipe[0], F_SETFL, O_NONBLOCK);
    fcntl(stderrPipe[0], F_SETFL, O_NONBLOCK);

    auto readFromPipe = [](int fd, std::string& output) {
        char buffer[4096];
        ssize_t bytesRead;
        while ((bytesRead = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            output.append(buffer, bytesRead);
        }
        return bytesRead;
    };

    int status;
    bool processDone = false;
    auto timeoutTime = startTime + std::chrono::milliseconds(timeoutMs);

    while (!processDone) {
        // 检查超时
        if (std::chrono::steady_clock::now() > timeoutTime) {
            kill(pid, SIGTERM);
            result.timedOut = true;
            break;
        }

        // 检查进程状态
        pid_t waitResult = waitpid(pid, &status, WNOHANG);
        if (waitResult == pid) {
            processDone = true;
            if (WIFEXITED(status)) {
                result.exitCode = WEXITSTATUS(status);
            } else {
                result.exitCode = -1;
            }
        } else if (waitResult == -1 && errno != ECHILD) {
            result.exitCode = -1;
            result.error = "waitpid failed: " + std::string(strerror(errno));
            break;
        }

        // 读取可用输出
        readFromPipe(stdoutPipe[0], result.output);
        readFromPipe(stderrPipe[0], result.error);

        // 短暂休眠
        usleep(10000); // 10ms
    }

    // 读取剩余输出
    readFromPipe(stdoutPipe[0], result.output);
    readFromPipe(stderrPipe[0], result.error);

    // 关闭管道
    close(stdoutPipe[0]);
    close(stderrPipe[0]);

    // 确保进程结束
    if (!processDone) {
        waitpid(pid, &status, 0);
    }

    auto endTime = std::chrono::steady_clock::now();
    result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                               endTime - startTime).count();

    return result;
}
#endif

std::string CoreImpl::buildShellCommand(const std::string& command, ShellType shellType) {
    std::string fullCommand;

    switch (shellType) {
    case ShellType::PowerShell:
        fullCommand = "powershell -NoProfile -ExecutionPolicy ";
        fullCommand += m_executionPolicy.empty() ? "Bypass" : m_executionPolicy;
        fullCommand += " -Command \"";
        // 转义引号
        for (char c : command) {
            if (c == '\"') fullCommand += "\\\"";
            else fullCommand += c;
        }
        fullCommand += "\"";
        break;

    case ShellType::CMD:
        fullCommand = "cmd.exe /C \"";
        for (char c : command) {
            if (c == '\"') fullCommand += "\\\"";
            else fullCommand += c;
        }
        fullCommand += "\"";
        break;

    case ShellType::Bash:
        fullCommand = "bash -c \"";
        for (char c : command) {
            if (c == '\"') fullCommand += "\\\"";
            else if (c == '$') fullCommand += "\\$";
            else if (c == '`') fullCommand += "\\`";
            else fullCommand += c;
        }
        fullCommand += "\"";
        break;

    case ShellType::Sh:
        fullCommand = "sh -c \"";
        for (char c : command) {
            if (c == '\"') fullCommand += "\\\"";
            else fullCommand += c;
        }
        fullCommand += "\"";
        break;
    }

    return fullCommand;
}

int CoreImpl::executeAsync(const std::string& command,
                           ShellType shellType,
                           int timeoutMs,
                           OutputCallback outputCallback) {
    int asyncId = nextAsyncId();

    auto asyncCmd = std::make_shared<AsyncCommand>(
        asyncId, command, shellType, timeoutMs, std::move(outputCallback)
        );

    {
        std::lock_guard<std::mutex> lock(m_asyncMutex);
        m_asyncCommands[asyncId] = asyncCmd;
    }

    // 启动线程执行命令
    asyncCmd->thread = std::thread(&CoreImpl::asyncExecutionThread, this, asyncCmd);

    return asyncId;
}

void CoreImpl::asyncExecutionThread(std::shared_ptr<AsyncCommand> cmd) {
    CommandResult result = executeSync(cmd->command, cmd->shellType, cmd->timeoutMs);

    std::lock_guard<std::mutex> lock(cmd->mutex);
    if (cmd->cancelled) {
        cmd->state = AsyncState::Cancelled;
    } else {
        cmd->result = std::move(result);
        cmd->state = cmd->result.timedOut ? AsyncState::TimedOut :
                         (cmd->result.exitCode == 0 ? AsyncState::Completed : AsyncState::Failed);
    }
    cmd->cv.notify_all();

    // 如果设置了回调，调用它
    if (cmd->outputCallback && !cmd->cancelled) {
        if (!cmd->result.output.empty()) {
            cmd->outputCallback(cmd->result.output, false);
        }
        if (!cmd->result.error.empty()) {
            cmd->outputCallback(cmd->result.error, true);
        }
    }
}

AsyncState CoreImpl::getAsyncStatus(int asyncId) {
    std::lock_guard<std::mutex> lock(m_asyncMutex);
    auto it = m_asyncCommands.find(asyncId);
    if (it == m_asyncCommands.end()) {
        return AsyncState::Failed;
    }
    return it->second->state;
}

bool CoreImpl::getAsyncResult(int asyncId, CommandResult& result) {
    std::lock_guard<std::mutex> lock(m_asyncMutex);
    auto it = m_asyncCommands.find(asyncId);
    if (it == m_asyncCommands.end()) {
        return false;
    }

    std::unique_lock<std::mutex> cmdLock(it->second->mutex);
    if (it->second->state == AsyncState::Running) {
        // 等待完成
        it->second->cv.wait(cmdLock, [&]() {
            return it->second->state != AsyncState::Running;
        });
    }

    result = it->second->result;
    return true;
}

bool CoreImpl::terminateAsync(int asyncId) {
    std::lock_guard<std::mutex> lock(m_asyncMutex);
    auto it = m_asyncCommands.find(asyncId);
    if (it == m_asyncCommands.end()) {
        return false;
    }

    it->second->cancelled = true;
    it->second->state = AsyncState::Cancelled;
    it->second->cv.notify_all();

    return true;
}

void CoreImpl::setWorkingDirectory(const std::string& directory) {
    m_workingDirectory = directory;
}

void CoreImpl::setEnvironment(const std::string& key, const std::string& value) {
    m_environment[key] = value;
}

void CoreImpl::setEnvironment(const std::map<std::string, std::string>& environment) {
    m_environment = environment;
}

void CoreImpl::setExecutionPolicy(const std::string& policy) {
    m_executionPolicy = policy;
}

void CoreImpl::clearEnvironment() {
    m_environment.clear();
}

int CoreImpl::nextAsyncId() {
    return s_nextAsyncId++;
}

} // namespace Zrun
