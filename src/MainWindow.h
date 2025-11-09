#pragma once

#include <QMainWindow>

class QLineEdit;
class QPdfDocument;
class SelectablePdfView;
class QPdfSearchModel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void openPdf(const QString& filePath);

private:
    void setupUi();
    void setupShortcuts();

    QPdfDocument* m_doc {nullptr};
    SelectablePdfView* m_view {nullptr};
    QLineEdit* m_searchEdit {nullptr};
    QPdfSearchModel* m_searchModel {nullptr};
};
