#include "zrun.hpp"
#include "zrun_core.h"
#include <memory>

namespace Zrun {

class ZRun::Impl {
public:
    Zrun::CoreImpl core;
};

ZRun::ZRun() : m_impl(std::make_unique<Impl>()) {}

ZRun::~ZRun() = default;

CommandResult ZRun::executeSync(const std::string& command,
                                ShellType shellType,
                                int timeoutMs) {
    return m_impl->core.executeSync(command, shellType, timeoutMs);
}

int ZRun::executeAsync(const std::string& command,
                       ShellType shellType,
                       int timeoutMs,
                       OutputCallback callback) {
    return m_impl->core.executeAsync(command, shellType, timeoutMs, callback);
}

AsyncState ZRun::getAsyncStatus(int asyncId) {
    return m_impl->core.getAsyncStatus(asyncId);
}

CommandResult ZRun::getAsyncResult(int asyncId) {
    CommandResult result;
    m_impl->core.getAsyncResult(asyncId, result);
    return result;
}

bool ZRun::terminateAsync(int asyncId) {
    return m_impl->core.terminateAsync(asyncId);
}

void ZRun::setWorkingDirectory(const std::string& directory) {
    m_impl->core.setWorkingDirectory(directory);
}

void ZRun::setEnvironment(const std::string& key, const std::string& value) {
    m_impl->core.setEnvironment(key, value);
}

void ZRun::setEnvironment(const std::map<std::string, std::string>& environment) {
    m_impl->core.setEnvironment(environment);
}

void ZRun::setExecutionPolicy(const std::string& policy) {
    m_impl->core.setExecutionPolicy(policy);
}

void ZRun::clearEnvironment() {
    m_impl->core.clearEnvironment();
}

} // namespace Zrun
