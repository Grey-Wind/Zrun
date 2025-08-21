#ifndef ZRUNQT_H
#define ZRUNQT_H

#include <QObject>
#include <QString>
#include <QMap>

class ZRunQt : public QObject
{
    Q_OBJECT

public:
    enum ShellType {
        CMD,
        PowerShell,
        Bash
    };
    Q_ENUM(ShellType)

    enum AsyncState {
        Running,
        Completed,
        Failed,
        TimedOut,
        Cancelled
    };
    Q_ENUM(AsyncState)

    struct CommandResult {
        int exitCode = 0;
        QString output;
        QString error;
        qint64 executionTime = 0;
        bool timedOut = false;

        CommandResult() = default;
        CommandResult(int code, const QString& out, const QString& err, qint64 time, bool timeout)
            : exitCode(code), output(out), error(err), executionTime(time), timedOut(timeout) {}
    };

    explicit ZRunQt(QObject *parent = nullptr);
    ~ZRunQt();

    // 同步执行命令
    CommandResult executeSync(const QString &command,
                              ShellType shellType = PowerShell,
                              int timeoutMs = 30000);

    // 异步执行命令
    int executeAsync(const QString &command,
                     ShellType shellType = PowerShell,
                     int timeoutMs = 30000);

    // 获取异步命令状态
    AsyncState getAsyncStatus(int asyncId);

    // 获取异步命令结果
    CommandResult getAsyncResult(int asyncId);

    // 终止异步命令
    bool terminateAsync(int asyncId);

    // 设置工作目录
    void setWorkingDirectory(const QString &directory);

    // 设置环境变量
    void setEnvironment(const QString &key, const QString &value);

    // 设置执行策略 (PowerShell)
    void setExecutionPolicy(const QString &policy);

signals:
    void asyncOutputReady(int asyncId, const QString &output, bool isError);
    void asyncFinished(int asyncId, const ZRunQt::CommandResult &result);

private slots:
    void onAsyncOutput(int asyncId, const QString &output, bool isError);
    void onAsyncFinished(int asyncId, const CommandResult &result);

private:
    class Impl;
    Impl* m_impl;
};

// 只在头文件中声明一次元类型
Q_DECLARE_METATYPE(ZRunQt::CommandResult)

#endif // ZRUNQT_H
