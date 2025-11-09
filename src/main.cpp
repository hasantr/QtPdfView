#include "MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QString>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow w;
    w.resize(1000, 800);

    // Açılışta license.pdf: öncelik çalışma dizini; yoksa proje kökü ihtimali
    QString defaultPdf = QDir::current().filePath(QStringLiteral("license.pdf"));
    if (!QFileInfo::exists(defaultPdf)) {
        // build klasöründen çalışırken proje kökü bir üst dizindir
        const QString candidate = QDir(QCoreApplication::applicationDirPath())
                                      .filePath(QStringLiteral("../license.pdf"));
        if (QFileInfo::exists(candidate))
            defaultPdf = candidate;
    }
    w.openPdf(defaultPdf);

    w.show();
    return app.exec();
}
