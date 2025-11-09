#pragma once

#include <QMainWindow>
#include <QString>

class QLineEdit;
class QPdfDocument;
class SelectablePdfView;
class QPdfSearchModel;
class QLabel;
class QAction;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void openPdf(const QString& filePath);

private:
    void setupUi();
    void setupShortcuts();
    void updateSearchStatus();
    void jumpToSearchResult(int idx);

    QPdfDocument* m_doc {nullptr};
    SelectablePdfView* m_view {nullptr};
    QLineEdit* m_searchEdit {nullptr};
    QPdfSearchModel* m_searchModel {nullptr};
    QString m_currentFilePath;
    QLabel* m_searchStatus {nullptr};
    QAction* m_actFindPrev {nullptr};
    QAction* m_actFindNext {nullptr};
};
