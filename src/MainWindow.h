#pragma once

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QPointer>
#include <QTimer>

#include "MiniMapWidget.h"

class QLineEdit;
class QPdfDocument;
class SelectablePdfView;
class QPdfSearchModel;
class QLabel;
class QAction;
class QListWidget;
class QDockWidget;
class SecretSearchPanel;
class QScrollBar;
class QDragEnterEvent;
class QDropEvent;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    void triggerAdvancedMinimapSearch(const QString& terms);

    void openPdf(const QString& filePath);
    void setOriginalFile(const QString& originalPath);
    void raiseAndActivate();

protected:
    void resizeEvent(QResizeEvent* ev) override;
    void dragEnterEvent(QDragEnterEvent* ev) override;
    void dropEvent(QDropEvent* ev) override;

private:
    void setupUi();
    void setupShortcuts();
    void updateSearchStatus();
    void jumpToSearchResult(int idx);
    void updatePageCountLabel();
    void setupThumbnailPanel();
    void setupSecretSearchPanel();
    void updateThumbnails();
    void updateCurrentPageHighlight();
    void updateSecretPageMetrics();
    void updateNormalSearchMinimap(const QString& term);
    void clearMinimapMarkers(const QString& message = QString());
    void runSecretSearch(const QString& terms);
    bool computePageOffsets(QVector<qreal>& offsets, qreal& totalHeight) const;
    int collectMarkersForTerms(const QStringList& terms,
                               QVector<MiniMapMarker>& markers,
                               QVector<int>& counts);
    void updateViewportOverlay();
    void positionFloatingMinimap();
    void adjustToolBarStyle();

    QPdfDocument* m_doc {nullptr};
    SelectablePdfView* m_view {nullptr};
    QLineEdit* m_searchEdit {nullptr};
    QPdfSearchModel* m_searchModel {nullptr};
    QString m_currentFilePath;
    QString m_originalFilePath;
    QAction* m_openOriginalAct {nullptr};
    QLabel* m_searchStatus {nullptr};
    QAction* m_actFindPrev {nullptr};
    QAction* m_actFindNext {nullptr};
    QLabel* m_pageCountLabel {nullptr};
    QListWidget* m_thumbnailList {nullptr};
    QDockWidget* m_thumbnailDock {nullptr};
    QAction* m_toggleThumbnails {nullptr};
    SecretSearchPanel* m_secretPanel {nullptr};
    QTimer* m_searchDebounce {nullptr};
    QVector<qreal> m_secretPageHeights;
    QToolBar* m_toolbar {nullptr};
    enum class MinimapSource {
        None,
        Normal,
        Secret
    };
    MinimapSource m_currentMinimapSource {MinimapSource::None};
    QScrollBar* m_verticalScrollBar {nullptr};
};
