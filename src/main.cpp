#include "MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QLocalServer>
#include <QLocalSocket>
#include <QDataStream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Command line arguments:
    // args[1] = PDF file (to be displayed)
    // args[2] = Original file (optional - for title and "Open" button)
    QString selectedPdf;
    QString originalFile;
    const QStringList args = QCoreApplication::arguments();
    if (args.size() > 1) {
        QFileInfo fi(args.at(1));
        if (fi.exists() && fi.isFile())
            selectedPdf = fi.absoluteFilePath();
    }
    if (args.size() > 2) {
        QFileInfo fi(args.at(2));
        if (fi.exists() && fi.isFile())
            originalFile = fi.absoluteFilePath();
    }

    // Use license.pdf if no argument provided (quick check)
    if (selectedPdf.isEmpty()) {
        const QString defaultPdf = QDir::current().filePath(QStringLiteral("license.pdf"));
        if (QFileInfo::exists(defaultPdf)) {
            selectedPdf = defaultPdf;
        } else {
            // Check project root when running from build directory
            const QString fallback = QDir(QCoreApplication::applicationDirPath())
                                         .filePath(QStringLiteral("../license.pdf"));
            if (QFileInfo::exists(fallback))
                selectedPdf = fallback;
            else
                selectedPdf = defaultPdf; // Use path even if not exists (error shown in MainWindow)
        }
    }

    // Single instance: try to connect to existing instance, forward request and exit if successful
    const QString serverName = QStringLiteral("QtPdfView_SingleInstance");
    {
        QLocalSocket probe;
        probe.connectToServer(serverName, QIODevice::WriteOnly);
        if (probe.waitForConnected(150)) {
            // Forward file path to existing instance (or just activate if empty)
            QByteArray payload;
            QDataStream out(&payload, QIODevice::WriteOnly);
            out.setVersion(QDataStream::Qt_6_2);
            out << selectedPdf; // can be empty -> just bring to front
            probe.write(payload);
            probe.flush();
            probe.waitForBytesWritten(150);
            return 0;
        }
    }

    // Start server (remove old remnant if exists)
    QLocalServer::removeServer(serverName);
    QLocalServer* server = new QLocalServer(&app);
    server->listen(serverName);

    MainWindow w;
    w.resize(1000, 800);

    w.openPdf(selectedPdf);
    if (!originalFile.isEmpty())
        w.setOriginalFile(originalFile);
    w.show();

    // Listen for "open file" / "activate" requests from second instances
    QObject::connect(server, &QLocalServer::newConnection, &app, [&w, server](){
        while (QLocalSocket* client = server->nextPendingConnection()) {
            QObject::connect(client, &QLocalSocket::readyRead, client, [client, &w]{
                QDataStream in(client);
                in.setVersion(QDataStream::Qt_6_2);
                QString path;
                in >> path;
                if (!path.isEmpty())
                    w.openPdf(path);
                w.raiseAndActivate();
                client->disconnectFromServer();
            });
            QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
        }
    });

    return app.exec();
}
