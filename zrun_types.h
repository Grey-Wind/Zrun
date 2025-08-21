#ifndef ZRUN_TYPES_H
#define ZRUN_TYPES_H

#include <string>
#include <functional>

namespace Zrun {

enum class ShellType {
    CMD,
    PowerShell,
    Bash,
    Sh
};

enum class AsyncState {
    Running,
    Completed,
    Failed,
    TimedOut,
    Cancelled
};

struct CommandResult {
    int exitCode = 0;
    std::string output;
    std::string error;
    long long executionTime = 0;
    bool timedOut = false;

    CommandResult() = default;
    CommandResult(int code, std::string out, std::string err, long long time, bool timeout)
        : exitCode(code), output(std::move(out)), error(std::move(err)),
        executionTime(time), timedOut(timeout) {}
};

using OutputCallback = std::function<void(const std::string& output, bool isError)>;

} // namespace Zrun

#endif // ZRUN_TYPES_H
