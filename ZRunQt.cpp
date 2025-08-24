#include "ZRunQt.h"
#include "zrun_core.h"
#include <QThread>
#include <QMetaType>

// 静态编译定义
#if defined(ZRUN_STATIC)
#define ZRUNQT_API
#else
#define ZRUNQT_API Q_DECL_IMPORT
#endif

// 只在实现文件中注册元类型，不重复声明
class ZRunQt::Impl {
public:
    Zrun::CoreImpl core;
};

ZRunQt::ZRunQt(QObject *parent)
    : QObject(parent), m_impl(new Impl()) {
    qRegisterMetaType<ZRunQt::CommandResult>();
    qRegisterMetaType<ZRunQt::AsyncState>();
}

ZRunQt::~ZRunQt() {
    delete m_impl;
}

ZRunQt::CommandResult ZRunQt::executeSync(const QString &command,
                                          ShellType shellType,
                                          int timeoutMs) {
    Zrun::ShellType type;
    switch (shellType) {
    case CMD: type = Zrun::ShellType::CMD; break;
    case PowerShell: type = Zrun::ShellType::PowerShell; break;
    case Bash: type = Zrun::ShellType::Bash; break;
    default: type = Zrun::ShellType::PowerShell;
    }

    auto result = m_impl->core.executeSync(command.toStdString(), type, timeoutMs);

    CommandResult qtResult;
    qtResult.exitCode = result.exitCode;
    qtResult.output = QString::fromStdString(result.output);
    qtResult.error = QString::fromStdString(result.error);
    qtResult.executionTime = result.executionTime;
    qtResult.timedOut = result.timedOut;

    return qtResult;
}

int ZRunQt::executeAsync(const QString &command,
                         ShellType shellType,
                         int timeoutMs) {
    Zrun::ShellType type;
    switch (shellType) {
    case CMD: type = Zrun::ShellType::CMD; break;
    case PowerShell: type = Zrun::ShellType::PowerShell; break;
    case Bash: type = Zrun::ShellType::Bash; break;
    default: type = Zrun::ShellType::PowerShell;
    }

    auto outputCallback = [this](const std::string& output, bool isError) {
        QMetaObject::invokeMethod(this, "onAsyncOutput", Qt::QueuedConnection,
                                  Q_ARG(int, -1),
                                  Q_ARG(QString, QString::fromStdString(output)),
                                  Q_ARG(bool, isError));
    };

    int asyncId = m_impl->core.executeAsync(
        command.toStdString(), type, timeoutMs, outputCallback
        );

    return asyncId;
}

ZRunQt::AsyncState ZRunQt::getAsyncStatus(int asyncId) {
    auto state = m_impl->core.getAsyncStatus(asyncId);
    switch (state) {
    case Zrun::AsyncState::Running: return Running;
    case Zrun::AsyncState::Completed: return Completed;
    case Zrun::AsyncState::Failed: return Failed;
    case Zrun::AsyncState::TimedOut: return TimedOut;
    case Zrun::AsyncState::Cancelled: return Cancelled;
    default: return Failed;
    }
}

ZRunQt::CommandResult ZRunQt::getAsyncResult(int asyncId) {
    Zrun::CommandResult result;
    if (m_impl->core.getAsyncResult(asyncId, result)) {
        CommandResult qtResult;
        qtResult.exitCode = result.exitCode;
        qtResult.output = QString::fromStdString(result.output);
        qtResult.error = QString::fromStdString(result.error);
        qtResult.executionTime = result.executionTime;
        qtResult.timedOut = result.timedOut;
        return qtResult;
    }

    return CommandResult();
}

bool ZRunQt::terminateAsync(int asyncId) {
    return m_impl->core.terminateAsync(asyncId);
}

void ZRunQt::setWorkingDirectory(const QString &directory) {
    m_impl->core.setWorkingDirectory(directory.toStdString());
}

void ZRunQt::setEnvironment(const QString &key, const QString &value) {
    m_impl->core.setEnvironment(key.toStdString(), value.toStdString());
}

void ZRunQt::setExecutionPolicy(const QString &policy) {
    m_impl->core.setExecutionPolicy(policy.toStdString());
}

void ZRunQt::onAsyncOutput(int asyncId, const QString &output, bool isError) {
    emit asyncOutputReady(asyncId, output, isError);
}

void ZRunQt::onAsyncFinished(int asyncId, const CommandResult &result) {
    emit asyncFinished(asyncId, result);
}
