#include "MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QString>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainWindow w;
    w.resize(1000, 800);

    // Komut satırı argümanı ile PDF açma: ilk argüman dosya yolu olarak yorumlanır
    QString selectedPdf;
    const QStringList args = QCoreApplication::arguments();
    if (args.size() > 1) {
        QString candidate = args.at(1);
        QFileInfo fi(candidate);
        if (fi.exists() && fi.isFile())
            selectedPdf = fi.absoluteFilePath();
    }

    // Argüman yoksa açılışta license.pdf: öncelik çalışma dizini; yoksa proje kökü ihtimali
    if (selectedPdf.isEmpty()) {
        QString defaultPdf = QDir::current().filePath(QStringLiteral("license.pdf"));
        if (!QFileInfo::exists(defaultPdf)) {
            // build klasöründen çalışırken proje kökü bir üst dizindir
            const QString candidate = QDir(QCoreApplication::applicationDirPath())
                                          .filePath(QStringLiteral("../license.pdf"));
            if (QFileInfo::exists(candidate))
                defaultPdf = candidate;
        }
        selectedPdf = defaultPdf;
    }

    w.openPdf(selectedPdf);

    w.show();
    return app.exec();
}
