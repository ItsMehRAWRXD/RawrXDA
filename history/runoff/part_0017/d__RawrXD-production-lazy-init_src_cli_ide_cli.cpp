// ============================================================================
// IDE CLI Client - QLocalSocket JSON Command Sender
// ============================================================================
// Connects to IDECommandServer and sends JSON commands.
// ============================================================================

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTimer>
#include <QEventLoop>
#include <QTextStream>

namespace {
QJsonObject buildParams(const QString& command, const QCommandLineParser& parser) {
    QJsonObject params;
    if (command == "load-model") {
        params["path"] = parser.value("path");
    } else if (command == "open-file") {
        params["path"] = parser.value("path");
    } else if (command == "toggle-dock") {
        params["name"] = parser.value("name");
    } else if (command == "send-chat") {
        params["message"] = parser.value("message");
    } else if (command == "list-files") {
        params["directory"] = parser.value("directory");
    }
    return params;
}

bool sendRequest(const QString& serverName, const QJsonObject& request, QJsonObject& response, int timeoutMs) {
    QLocalSocket socket;
    socket.connectToServer(serverName);
    if (!socket.waitForConnected(timeoutMs)) {
        return false;
    }

    QJsonDocument doc(request);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);
    payload.append('\n');

    socket.write(payload);
    socket.flush();

    if (!socket.waitForReadyRead(timeoutMs)) {
        return false;
    }

    QByteArray data = socket.readAll();
    int newline = data.indexOf('\n');
    if (newline >= 0) {
        data = data.left(newline);
    }

    QJsonParseError err;
    QJsonDocument respDoc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !respDoc.isObject()) {
        return false;
    }

    response = respDoc.object();
    return true;
}
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("RawrXD-IDE-CLI");
    app.setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("RawrXD IDE Command Client");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption serverOpt({"s", "server"}, "IDE server name", "name", "RawrXD_IDE_Server");
    QCommandLineOption timeoutOpt({"t", "timeout"}, "Timeout (ms)", "ms", "5000");
    QCommandLineOption pathOpt("path", "Path for load-model/open-file", "path");
    QCommandLineOption nameOpt("name", "Dock name for toggle-dock", "name");
    QCommandLineOption messageOpt("message", "Message for send-chat", "message");
    QCommandLineOption directoryOpt("directory", "Directory for list-files", "directory", ".");

    parser.addOption(serverOpt);
    parser.addOption(timeoutOpt);
    parser.addOption(pathOpt);
    parser.addOption(nameOpt);
    parser.addOption(messageOpt);
    parser.addOption(directoryOpt);

    parser.addPositionalArgument("command", "Command to send (ping, load-model, open-file, toggle-dock, send-chat, get-status, list-files)");
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    QTextStream out(stdout);
    QTextStream err(stderr);

    if (args.isEmpty()) {
        err << "Missing command. Use --help for usage." << Qt::endl;
        return 1;
    }

    const QString command = args.first();
    const int timeoutMs = parser.value(timeoutOpt).toInt();
    const QString serverName = parser.value(serverOpt);

    QJsonObject request;
    request["command"] = command;
    request["params"] = buildParams(command, parser);

    QJsonObject response;
    if (!sendRequest(serverName, request, response, timeoutMs)) {
        err << "Failed to send command to server: " << serverName << Qt::endl;
        return 1;
    }

    QJsonDocument respDoc(response);
    out << respDoc.toJson(QJsonDocument::Indented) << Qt::endl;
    return 0;
}
