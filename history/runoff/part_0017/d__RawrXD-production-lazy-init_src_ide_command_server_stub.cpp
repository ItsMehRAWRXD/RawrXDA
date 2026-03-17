// IDECommandServer stub implementation for GUI linking
// Provides minimal functional stubs to resolve linker errors

#include <string>
#include <mutex>
#include <QObject>
#include <QString>
#include <QDebug>

class IDECommandServer : public QObject {
    Q_OBJECT

private:
    bool running_ = false;
    std::string address_;
    std::mutex mutex_;

public:
    explicit IDECommandServer(QObject* parent = nullptr, QObject* window = nullptr)
        : QObject(parent) {
        Q_UNUSED(window);
    }

    bool startServer(const QString& address) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            qDebug() << "IDECommandServer already running";
            return false;
        }
        address_ = address.toStdString();
        running_ = true;
        qDebug() << "IDECommandServer started on" << address;
        return true;
    }

    void stopServer() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        running_ = false;
        qDebug() << "IDECommandServer stopped";
    }

    bool isRunning() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

    std::string getAddress() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return address_;
    }
};

#include "ide_command_server_stub.moc"
