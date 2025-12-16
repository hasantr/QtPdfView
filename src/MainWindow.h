/**
 * @file MainWindow.h
 * @brief Main application window for QtPdfView PDF viewer.
 *
 * MainWindow provides a complete PDF viewing experience with:
 * - PDF document display with multi-page view
 * - Text search with result highlighting
 * - Text selection and copy
 * - Page thumbnails panel
 * - Search result minimap on scrollbar
 * - Zoom controls (fit to width, fit to page, custom)
 * - Print and Save As functionality
 * - Drag and drop file opening
 *
 * Usage:
 * @code
 *   MainWindow window;
 *   window.openPdf("document.pdf");
 *   window.show();
 * @endcode
 */

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
class SearchMinimapPanel;
class QScrollBar;
class QDragEnterEvent;
class QDropEvent;

/**
 * @class MainWindow
 * @brief The main application window for PDF viewing.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    /**
     * @brief Constructs the main window.
     * @param parent Parent widget (typically nullptr for main window)
     */
    explicit MainWindow(QWidget* parent = nullptr);

    /**
     * @brief Triggers a multi-term search and displays results on minimap.
     * @param terms Search terms separated by semicolons (e.g., "word1;word2;word3")
     *
     * This method searches for multiple terms simultaneously and shows
     * all matches as markers on the scrollbar minimap.
     */
    void triggerMultiTermSearch(const QString& terms);

    /**
     * @brief Opens a PDF file for viewing.
     * @param filePath Path to the PDF file
     *
     * Loads the PDF document, updates thumbnails, and resets the view.
     * Shows an error dialog if the file cannot be opened.
     */
    void openPdf(const QString& filePath);

    /**
     * @brief Sets the original file path for the "Open" button.
     * @param originalPath Path to the original file (can be non-PDF)
     *
     * When viewing a converted PDF, this sets the path to the original
     * file so the user can open it with the system default application.
     * The window title is also set to this filename.
     */
    void setOriginalFile(const QString& originalPath);

    /**
     * @brief Brings the window to front and activates it.
     *
     * Used by single-instance mode to activate existing window
     * when a second instance tries to start.
     */
    void raiseAndActivate();

protected:
    void resizeEvent(QResizeEvent* ev) override;
    void dragEnterEvent(QDragEnterEvent* ev) override;
    void dropEvent(QDropEvent* ev) override;

private:
    // UI Setup
    void setupUi();
    void setupShortcuts();
    void setupThumbnailPanel();
    void setupSearchMinimap();

    // Search functionality
    void updateSearchStatus();
    void jumpToSearchResult(int idx);
    void updateSearchMinimap(const QString& term);
    void clearMinimapMarkers(const QString& message = QString());
    void runMultiTermSearch(const QString& terms);
    int collectMarkersForTerms(const QStringList& terms,
                               QVector<MiniMapMarker>& markers,
                               QVector<int>& counts);

    // Page/document updates
    void updatePageCountLabel();
    void updateThumbnails();
    void updateCurrentPageHighlight();
    void updatePageMetrics();
    bool computePageOffsets(QVector<qreal>& offsets, qreal& totalHeight) const;

    // Minimap/viewport
    void updateViewportOverlay();
    void positionFloatingMinimap();

    // Toolbar
    void adjustToolBarStyle();

    // Document and view
    QPdfDocument* m_doc {nullptr};
    SelectablePdfView* m_view {nullptr};
    QString m_currentFilePath;
    QString m_originalFilePath;

    // Search components
    QLineEdit* m_searchEdit {nullptr};
    QPdfSearchModel* m_searchModel {nullptr};
    QLabel* m_searchStatus {nullptr};
    QAction* m_actFindPrev {nullptr};
    QAction* m_actFindNext {nullptr};
    QTimer* m_searchDebounce {nullptr};

    // Toolbar and actions
    QToolBar* m_toolbar {nullptr};
    QAction* m_openOriginalAct {nullptr};
    QAction* m_toggleThumbnails {nullptr};
    QLabel* m_pageCountLabel {nullptr};

    // Thumbnails
    QListWidget* m_thumbnailList {nullptr};
    QDockWidget* m_thumbnailDock {nullptr};

    // Search minimap
    SearchMinimapPanel* m_minimapPanel {nullptr};
    QVector<qreal> m_pageHeights;
    QScrollBar* m_verticalScrollBar {nullptr};

    enum class MinimapSource {
        None,
        NormalSearch,
        MultiTermSearch
    };
    MinimapSource m_currentMinimapSource {MinimapSource::None};
};
