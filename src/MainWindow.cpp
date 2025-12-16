#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QFile>
#include <QLineEdit>
#include <QMessageBox>
#include <QLabel>
#include <QIcon>
#include <QColor>
#include <QtGlobal>
#include <QStringList>
#include <QPdfDocument>
#include <QPdfView>
#include <QPdfSelection>
#include <QPdfSearchModel>
#include <QPdfPageNavigator>
#include <QPdfPageSelector>
#include "SelectablePdfView.h"
#include "SecretSearchPanel.h"
#include <QShortcut>
#include <QToolBar>
#include <QStyle>
#include <QPainter>
#include <QPrinter>
#include <QPrintDialog>
#include <QListWidget>
#include <QDockWidget>
#include <QScrollBar>
#include <QProxyStyle>
#include <QTimer>
#include <QResizeEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

namespace {
class NoTransientScrollBarStyle : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;
    int styleHint(StyleHint hint,
                  const QStyleOption* option = nullptr,
                  const QWidget* widget = nullptr,
                  QStyleHintReturn* returnData = nullptr) const override
    {
        if (hint == QStyle::SH_ScrollBar_Transient)
            return 0;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setAcceptDrops(true);
    setupUi();
    setupThumbnailPanel();
    setupSecretSearchPanel();
    setupShortcuts();
}

void MainWindow::triggerAdvancedMinimapSearch(const QString& terms)
{
    runSecretSearch(terms);
}

void MainWindow::setupUi()
{
    m_doc = new QPdfDocument(this);
    m_view = new SelectablePdfView(this);
    m_view->setDocument(m_doc);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    // Favor fast, simple rendering but show all pages
    m_view->setPageMode(QPdfView::PageMode::MultiPage);
    m_view->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    m_view->setPageSpacing(0);
    m_view->setDocumentMargins(QMargins());

    setCentralWidget(m_view);

    m_verticalScrollBar = m_view->verticalScrollBar();
    if (m_verticalScrollBar) {
        m_verticalScrollBar->setMinimumWidth(26);
        m_verticalScrollBar->setStyleSheet(QStringLiteral(
            "QScrollBar:vertical { width: 26px; margin: 0px; }"
            "QScrollBar::handle:vertical { background: rgba(130,130,130,160); min-height: 28px; border-radius: 7px; }"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; border: none; }"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"));
    }

    QWidget* minimapParent = m_verticalScrollBar ? static_cast<QWidget*>(m_verticalScrollBar) : m_view;
    m_secretPanel = new SecretSearchPanel(minimapParent);
    m_secretPanel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_secretPanel->setStyleSheet(QStringLiteral("background: transparent;"));
    m_secretPanel->show();
    positionFloatingMinimap();

    if (m_verticalScrollBar) {
        connect(m_verticalScrollBar, &QScrollBar::valueChanged, this, [this](int){
            updateViewportOverlay();
        });
        connect(m_verticalScrollBar, &QScrollBar::rangeChanged, this, [this](int, int){
            updateViewportOverlay();
        });
    }
    connect(m_view, &SelectablePdfView::viewportGeometryChanged, this, [this]{
        updateViewportOverlay();
    });
    connect(m_view, &QPdfView::zoomFactorChanged, this, [this](qreal){
        updateViewportOverlay();
    });

    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(320);

    m_toolbar = addToolBar(tr("PDF"));
    auto* tb = m_toolbar;
    tb->setMovable(false);
    tb->setIconSize(QSize(20, 20));
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    auto iconOrFallback = [this](const char* path, QStyle::StandardPixmap fallback) -> QIcon {
        QIcon icon{QLatin1String(path)};
        if (icon.isNull())
            icon = style()->standardIcon(fallback);
        return icon;
    };

    // "Open" button (opens original file with system default app) - initially hidden
    m_openOriginalAct = tb->addAction(QIcon(), tr("Open"));
    m_openOriginalAct->setToolTip(tr("Open original file with default application"));
    m_openOriginalAct->setVisible(false);
    connect(m_openOriginalAct, &QAction::triggered, this, [this]{
        if (m_originalFilePath.isEmpty())
            return;
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(m_originalFilePath))) {
            QMessageBox::warning(this, tr("Open"), tr("Could not open file: %1").arg(m_originalFilePath));
        }
    });

    // Thumbnail toggle button
    QIcon listIcon = iconOrFallback(":/icons/pages.svg", QStyle::SP_FileDialogDetailedView);
    m_toggleThumbnails = tb->addAction(listIcon, tr("Pages"));
    m_toggleThumbnails->setCheckable(true);
    m_toggleThumbnails->setChecked(false);
    m_toggleThumbnails->setToolTip(tr("Show/Hide Page Thumbnails"));
    tb->addSeparator();

    // Save As (PDF) first
    QIcon saveIcon = iconOrFallback(":/icons/save.svg", QStyle::SP_DialogSaveButton);
    auto* saveAct = tb->addAction(saveIcon, tr("Save"));
    saveAct->setToolTip(tr("Save As (PDF)"));
    // Print button next to save (use theme icon if available)
    QIcon printIcon = iconOrFallback(":/icons/print.svg", QStyle::SP_FileDialogDetailedView);
    auto* printAct = tb->addAction(printIcon, tr("Print"));
    printAct->setToolTip(tr("Print"));
    // Mail button (system mail client)
    QIcon mailIcon = iconOrFallback(":/icons/email.svg", QStyle::SP_DialogOpenButton);
    auto* mailAct = tb->addAction(mailIcon, tr("Email"));
    mailAct->setToolTip(tr("Share via default email application"));
    
    // Page navigation
    QIcon prevIcon = iconOrFallback(":/icons/backpage.svg", QStyle::SP_ArrowBack);
    QIcon nextIcon = iconOrFallback(":/icons/nextpage.svg", QStyle::SP_ArrowForward);
    auto* prevPage = tb->addAction(prevIcon, QString());
    auto* nextPage = tb->addAction(nextIcon, QString());
    prevPage->setShortcut(Qt::Key_PageUp);
    nextPage->setShortcut(Qt::Key_PageDown);
    prevPage->setToolTip(tr("Previous Page (PgUp)"));
    nextPage->setToolTip(tr("Next Page (PgDn)"));

    // Page selector
    m_pageCountLabel = new QLabel(this);
    m_pageCountLabel->setMinimumWidth(48);
    m_pageCountLabel->setAlignment(Qt::AlignCenter);
    tb->addWidget(m_pageCountLabel);
    auto* pageSel = new QPdfPageSelector(this);
    pageSel->setDocument(m_doc);
    tb->addWidget(pageSel);
    tb->addSeparator();

    // Zoom controls
    QIcon zoomOutIcon = iconOrFallback(":/icons/zoomout.svg", QStyle::SP_ArrowDown);
    QIcon zoomInIcon  = iconOrFallback(":/icons/add.svg", QStyle::SP_ArrowUp);
    auto* zoomOut = tb->addAction(zoomOutIcon, tr("-"));
    auto* zoomIn  = tb->addAction(zoomInIcon,  tr("+"));
    zoomIn->setShortcut(QKeySequence::ZoomIn);
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    zoomIn->setToolTip(tr("Zoom In (Ctrl +)"));
    zoomOut->setToolTip(tr("Zoom Out (Ctrl -)"));
    QIcon fitWIcon = iconOrFallback(":/icons/width.svg", QStyle::SP_DesktopIcon);
    QIcon fitVIcon = iconOrFallback(":/icons/pageview.svg", QStyle::SP_DesktopIcon);
    auto* fitW    = tb->addAction(fitWIcon, tr("Width"));
    auto* fitV    = tb->addAction(fitVIcon, tr("Page"));
    fitW->setToolTip(tr("Fit to Width"));
    fitV->setToolTip(tr("Fit to Page"));
    tb->addSeparator();

    // Search box and nav
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(tr("Search (min 2 chars)"));
    tb->addWidget(m_searchEdit);
    QIcon findPrevIcon = iconOrFallback(":/icons/backfind.svg", QStyle::SP_MediaSkipBackward);
    QIcon findNextIcon = iconOrFallback(":/icons/nextfind.svg", QStyle::SP_MediaSkipForward);
    m_actFindPrev = tb->addAction(findPrevIcon, QString());
    m_actFindNext = tb->addAction(findNextIcon, QString());
    m_actFindNext->setShortcut(QKeySequence::FindNext);
    m_actFindPrev->setShortcut(QKeySequence::FindPrevious);
    m_actFindNext->setToolTip(tr("Next match (F3)"));
    m_actFindPrev->setToolTip(tr("Previous match (Shift+F3)"));
    m_searchStatus = new QLabel(tr("0 results"), this);
    m_searchStatus->setMinimumWidth(64);
    m_searchStatus->setAlignment(Qt::AlignCenter);
    tb->addWidget(m_searchStatus);

    // Search model highlights matches inside the view
    m_searchModel = new QPdfSearchModel(this);
    m_searchModel->setDocument(m_doc);
    m_view->setSearchModel(m_searchModel);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString&){
        if (m_searchDebounce)
            m_searchDebounce->start();
    });
    if (m_searchDebounce) {
        connect(m_searchDebounce, &QTimer::timeout, this, [this]{
            const QString txt = m_searchEdit ? m_searchEdit->text() : QString();
            if (txt.size() >= 2) {
                m_searchModel->setSearchString(txt);
                updateNormalSearchMinimap(txt);
            } else {
                m_searchModel->setSearchString(QString());
                m_view->setCurrentSearchResultIndex(-1);
                updateNormalSearchMinimap(QString());
            }
            updateSearchStatus();
        });
    }

    // Navigate between matches with Enter/Shift+Enter
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]{
        int count = m_searchModel->rowCount(QModelIndex());
        if (count <= 0)
            return;
        int idx = m_view->currentSearchResultIndex();
        idx = (idx + 1) % count;
        m_view->setCurrentSearchResultIndex(idx);
        jumpToSearchResult(idx);
        updateSearchStatus();
    });

    connect(m_actFindNext, &QAction::triggered, this, [this]{
        int count = m_searchModel->rowCount(QModelIndex());
        if (count <= 0) return;
        int idx = (m_view->currentSearchResultIndex() + 1) % count;
        m_view->setCurrentSearchResultIndex(idx);
        jumpToSearchResult(idx);
        updateSearchStatus();
    });
    connect(m_actFindPrev, &QAction::triggered, this, [this]{
        int count = m_searchModel->rowCount(QModelIndex());
        if (count <= 0) return;
        int idx = m_view->currentSearchResultIndex();
        idx = (idx - 1 + count) % count;
        m_view->setCurrentSearchResultIndex(idx);
        jumpToSearchResult(idx);
        updateSearchStatus();
    });

    // Status updates
    connect(m_searchModel, &QPdfSearchModel::countChanged, this, &MainWindow::updateSearchStatus);
    // Ensure highlighting kicks in while typing: set current index to 0 when results appear
    connect(m_searchModel, &QPdfSearchModel::countChanged, this, [this]{
        const QString term = m_searchEdit ? m_searchEdit->text() : QString();
        const int count = m_searchModel ? m_searchModel->rowCount(QModelIndex()) : 0;
        if (term.size() >= 2 && count > 0) {
            if (m_view->currentSearchResultIndex() < 0)
                m_view->setCurrentSearchResultIndex(0); // do not jump here
        } else {
            m_view->setCurrentSearchResultIndex(-1);
        }
    });
    connect(m_searchModel, &QPdfSearchModel::countChanged, this, [this]{
        const QString term = m_searchEdit ? m_searchEdit->text() : QString();
        if (term.size() >= 2)
            updateNormalSearchMinimap(term);
    });
    connect(m_view, &QPdfView::currentSearchResultIndexChanged, this, &MainWindow::updateSearchStatus);
    // Page count label updates
    connect(m_doc, &QPdfDocument::pageCountChanged, this, [this](int){
        updatePageCountLabel();
        updateSecretPageMetrics();
    });

    // Thumbnail toggle action connection
    connect(m_toggleThumbnails, &QAction::toggled, this, [this](bool checked){
        if (m_thumbnailDock)
            m_thumbnailDock->setVisible(checked);
    });

    // Hook page selector <-> view navigator
    if (auto* nav = m_view->pageNavigator()) {
        connect(nav, &QPdfPageNavigator::currentPageChanged, pageSel, &QPdfPageSelector::setCurrentPage);
        connect(pageSel, &QPdfPageSelector::currentPageChanged, this, [this, nav](int p){ nav->jump(p, QPointF(0,0)); });
        // Update thumbnail highlight when page changes
        connect(nav, &QPdfPageNavigator::currentPageChanged, this, &MainWindow::updateCurrentPageHighlight);
        connect(nav, &QPdfPageNavigator::currentPageChanged, this, &MainWindow::updateViewportOverlay);
    }

    // Navigation actions
    connect(prevPage, &QAction::triggered, this, [this]{
        if (!m_doc || !m_view->pageNavigator()) return;
        int p = m_view->pageNavigator()->currentPage();
        if (p > 0) m_view->pageNavigator()->jump(p - 1, QPointF(0,0));
    });
    connect(nextPage, &QAction::triggered, this, [this]{
        if (!m_doc || !m_view->pageNavigator()) return;
        int p = m_view->pageNavigator()->currentPage();
        if (p + 1 < m_doc->pageCount()) m_view->pageNavigator()->jump(p + 1, QPointF(0,0));
    });

    // Zoom actions
    connect(zoomIn, &QAction::triggered, this, [this]{
        m_view->setZoomMode(QPdfView::ZoomMode::Custom);
        m_view->setZoomFactor(m_view->zoomFactor() * 1.25);
    });
    connect(zoomOut, &QAction::triggered, this, [this]{
        m_view->setZoomMode(QPdfView::ZoomMode::Custom);
        m_view->setZoomFactor(m_view->zoomFactor() / 1.25);
    });
    connect(fitW, &QAction::triggered, this, [this]{ m_view->setZoomMode(QPdfView::ZoomMode::FitToWidth); });
    connect(fitV, &QAction::triggered, this, [this]{ m_view->setZoomMode(QPdfView::ZoomMode::FitInView); });

    // Save As action
    connect(saveAct, &QAction::triggered, this, [this]{
        if (m_currentFilePath.isEmpty()) {
            QMessageBox::warning(this, tr("Save"), tr("Current file path is unknown."));
            return;
        }
        QString dest = QFileDialog::getSaveFileName(this, tr("Save As"),
                                                    QDir::home().filePath(QFileInfo(m_currentFilePath).fileName()),
                                                    tr("PDF Files (*.pdf)"));
        if (dest.isEmpty()) return;
        if (QFileInfo::exists(dest)) QFile::remove(dest);
        if (!QFile::copy(m_currentFilePath, dest)) {
            QMessageBox::critical(this, tr("Save"), tr("Save failed: %1").arg(dest));
        }
    });

    // Print action (render each page to printer)
    connect(printAct, &QAction::triggered, this, [this]{
        if (!m_doc || m_doc->pageCount() <= 0) return;
        QPrinter printer(QPrinter::HighResolution);
        QPrintDialog dlg(&printer, this);
        if (dlg.exec() != QDialog::Accepted) return;
        QPainter painter(&printer);
        if (!painter.isActive()) return;
        const int pageCount = m_doc->pageCount();
        for (int i = 0; i < pageCount; ++i) {
            const QSize target = painter.viewport().size();
            if (target.isEmpty()) break;
            QImage img = m_doc->render(i, target);
            painter.drawImage(QPoint(0,0), img);
            if (i + 1 < pageCount)
                printer.newPage();
        }
    });

    connect(mailAct, &QAction::triggered, this, [this]{
        if (m_currentFilePath.isEmpty()) {
            QMessageBox::warning(this, tr("Email"), tr("Please open a PDF first."));
            return;
        }
        const QFileInfo fi(m_currentFilePath);
        const QString subject = tr("PDF sharing: %1").arg(fi.fileName());
        QString body = tr("File path: %1").arg(fi.absoluteFilePath());
        QUrl mailto(QStringLiteral("mailto:"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("subject"), subject);
        query.addQueryItem(QStringLiteral("body"), body);
        mailto.setQuery(query);
        if (!QDesktopServices::openUrl(mailto)) {
            QMessageBox::warning(this, tr("Email"), tr("Could not open default email application."));
        }
    });

    adjustToolBarStyle();
}

void MainWindow::setupShortcuts()
{
    // Ctrl+F focuses search
    auto* focusFind = new QShortcut(QKeySequence::Find, this);
    connect(focusFind, &QShortcut::activated, this, [this]{
        m_searchEdit->setFocus(Qt::ShortcutFocusReason);
        m_searchEdit->selectAll();
    });

    // Ctrl+C copies current selection (or current search match as fallback)
    auto* copyAct = new QAction(tr("Copy"), this);
    copyAct->setShortcut(QKeySequence::Copy);
    addAction(copyAct);
    connect(copyAct, &QAction::triggered, this, [this]{
        if (m_view->copySelectionToClipboard())
            return;
        // Fallback: copy current search match text
        const int idx = m_view->currentSearchResultIndex();
        if (idx >= 0) {
            QPdfLink link = m_searchModel->resultAtIndex(idx);
            if (link.isValid())
                link.copyToClipboard();
        }
    });

    // Esc clears search
    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(esc, &QShortcut::activated, this, [this]{ m_searchEdit->clear(); });
}

void MainWindow::openPdf(const QString& filePath)
{
    QFileInfo fi(filePath);
    const auto err = m_doc->load(filePath);
    if (err != QPdfDocument::Error::None) {
        QMessageBox::critical(this, tr("Could not open PDF"),
                              tr("Could not open file: %1\nError code: %2")
                                  .arg(fi.absoluteFilePath()).arg(int(err)));
        return;
    }

    m_currentFilePath = fi.absoluteFilePath();
    setWindowTitle(fi.fileName());
    updatePageCountLabel();

    // Thumbnail'ları güncelle
    updateThumbnails();
    updateSecretPageMetrics();
    updateNormalSearchMinimap(m_searchEdit ? m_searchEdit->text() : QString());
    updateViewportOverlay();
}

void MainWindow::updateSearchStatus()
{
    if (!m_searchStatus) return;
    const QString term = m_searchEdit ? m_searchEdit->text() : QString();
    const int count = m_searchModel ? m_searchModel->rowCount(QModelIndex()) : 0;
    const int idx = m_view ? m_view->currentSearchResultIndex() : -1;
    if (term.size() < 2 || count <= 0) {
        m_searchStatus->setText(tr("0 Results"));
        if (m_actFindPrev) m_actFindPrev->setEnabled(false);
        if (m_actFindNext) m_actFindNext->setEnabled(false);
        return;
    }
    m_searchStatus->setText(tr("%1 Results").arg(count));
    if (m_actFindPrev) m_actFindPrev->setEnabled(true);
    if (m_actFindNext) m_actFindNext->setEnabled(true);
}

void MainWindow::jumpToSearchResult(int idx)
{
    if (!m_searchModel || !m_view)
        return;
    if (idx < 0)
        return;
    QPdfLink link = m_searchModel->resultAtIndex(idx);
    if (!link.isValid())
        return;
    const auto rects = link.rectangles();
    if (rects.isEmpty()) {
        if (auto* nav = m_view->pageNavigator())
            nav->jump(link);
        return;
    }
    // Ensure the first match rectangle is visible within the page
    m_view->ensurePageRectVisible(link.page(), rects.first());
}

void MainWindow::updatePageCountLabel()
{
    if (!m_pageCountLabel) return;
    const int pc = m_doc ? m_doc->pageCount() : 0;
    m_pageCountLabel->setText(pc > 0 ? QString::number(pc) : QStringLiteral("-"));
}

void MainWindow::raiseAndActivate()
{
    // Bring window to front and activate
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    raise();
    activateWindow();
}

void MainWindow::resizeEvent(QResizeEvent* ev)
{
    QMainWindow::resizeEvent(ev);
    adjustToolBarStyle();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* ev)
{
    if (ev->mimeData()->hasUrls()) {
        const QList<QUrl> urls = ev->mimeData()->urls();
        for (const QUrl& url : urls) {
            if (url.isLocalFile()) {
                const QString path = url.toLocalFile().toLower();
                if (path.endsWith(QLatin1String(".pdf"))) {
                    ev->acceptProposedAction();
                    return;
                }
            }
        }
    }
    ev->ignore();
}

void MainWindow::dropEvent(QDropEvent* ev)
{
    if (!ev->mimeData()->hasUrls()) {
        ev->ignore();
        return;
    }

    const QList<QUrl> urls = ev->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            const QString path = url.toLocalFile();
            if (path.toLower().endsWith(QLatin1String(".pdf"))) {
                openPdf(path);
                ev->acceptProposedAction();
                return;
            }
        }
    }
    ev->ignore();
}

void MainWindow::setupThumbnailPanel()
{
    // Create QDockWidget for thumbnail panel on the left side
    m_thumbnailDock = new QDockWidget(tr("Pages"), this);
    m_thumbnailDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_thumbnailList = new QListWidget(m_thumbnailDock);
    m_thumbnailList->setViewMode(QListWidget::IconMode);
    m_thumbnailList->setIconSize(QSize(220, 220));
    m_thumbnailList->setSpacing(12);
    m_thumbnailList->setMovement(QListWidget::Static);
    m_thumbnailList->setResizeMode(QListWidget::Adjust);
    m_thumbnailList->setUniformItemSizes(true);

    m_thumbnailDock->setWidget(m_thumbnailList);
    addDockWidget(Qt::LeftDockWidgetArea, m_thumbnailDock);

    // Hidden by default (user preference)
    m_thumbnailDock->hide();

    // Navigate to page when thumbnail is clicked
    connect(m_thumbnailList, &QListWidget::currentRowChanged, this, [this](int row){
        if (row >= 0 && m_view && m_view->pageNavigator()) {
            m_view->pageNavigator()->jump(row, QPointF(0, 0));
        }
    });
}

void MainWindow::setupSecretSearchPanel()
{
    if (!m_secretPanel)
        return;

    connect(m_secretPanel, &SecretSearchPanel::markerActivated, this, [this](int page, const QRectF& rect){
        if (!m_view)
            return;
        QRectF targetRect = rect;
        if (!targetRect.isValid())
            targetRect = QRectF(QPointF(0, 0), QSizeF(10, 10));
        if (auto* nav = m_view->pageNavigator())
            nav->jump(page, targetRect.topLeft());
        m_view->ensurePageRectVisible(page, targetRect, 24);
    });

    clearMinimapMarkers(QString());
    updateViewportOverlay();
}

void MainWindow::updateThumbnails()
{
    if (!m_thumbnailList || !m_doc)
        return;

    m_thumbnailList->clear();

    const int pageCount = m_doc->pageCount();
    if (pageCount <= 0)
        return;

    // Create high quality preview for each page (2x render for sharper image)
    for (int i = 0; i < pageCount; ++i) {
        const QSize renderSize(440, 440);  // 2x high resolution
        QImage thumbnail = m_doc->render(i, renderSize);

        auto* item = new QListWidgetItem(QIcon(QPixmap::fromImage(thumbnail)),
                                         QString::number(i + 1),
                                         m_thumbnailList);
        item->setTextAlignment(Qt::AlignCenter);
        item->setData(Qt::UserRole, i); // Store page number
    }

    // Select first page
    if (pageCount > 0)
        m_thumbnailList->setCurrentRow(0);
}

void MainWindow::updateCurrentPageHighlight()
{
    if (!m_thumbnailList || !m_view || !m_view->pageNavigator())
        return;

    const int currentPage = m_view->pageNavigator()->currentPage();

    // Update list item (block signals to prevent signal loop)
    m_thumbnailList->blockSignals(true);
    if (currentPage >= 0 && currentPage < m_thumbnailList->count())
        m_thumbnailList->setCurrentRow(currentPage);
    m_thumbnailList->blockSignals(false);
}

void MainWindow::updateSecretPageMetrics()
{
    m_secretPageHeights.clear();
    if (!m_secretPanel)
        return;
    if (!m_doc) {
        m_secretPanel->setPageHeights({});
        clearMinimapMarkers(tr("No document"));
        return;
    }

    const int pageCount = m_doc->pageCount();
    if (pageCount <= 0) {
        m_secretPanel->setPageHeights({});
        clearMinimapMarkers(tr("No document"));
        return;
    }

    m_secretPageHeights.reserve(pageCount);
    for (int i = 0; i < pageCount; ++i) {
        const QSizeF pts = m_doc->pagePointSize(i);
        m_secretPageHeights.append(qMax<qreal>(pts.height(), 1.0));
    }
    m_secretPanel->setPageHeights(m_secretPageHeights);
    updateViewportOverlay();
}

void MainWindow::clearMinimapMarkers(const QString& message)
{
    if (!m_secretPanel)
        return;
    m_secretPanel->setMarkers({});
    if (!message.isEmpty())
        m_secretPanel->setStatusMessage(message);
    m_currentMinimapSource = MinimapSource::None;
}

void MainWindow::updateViewportOverlay()
{
    if (!m_view || !m_doc) {
        if (m_secretPanel)
            m_secretPanel->setViewportRange(-1.0, -1.0);
        positionFloatingMinimap();
        return;
    }

    QScrollBar* vsb = m_verticalScrollBar ? m_verticalScrollBar : m_view->verticalScrollBar();
    if (!vsb) {
        if (m_secretPanel)
            m_secretPanel->setViewportRange(-1.0, -1.0);
        positionFloatingMinimap();
        return;
    }

    if (!m_secretPanel) {
        positionFloatingMinimap();
        return;
    }

    const int pageStep = qMax(1, vsb->pageStep());
    const int maxVal = qMax(0, vsb->maximum());
    const int denom = pageStep + maxVal;
    if (denom <= 0) {
        m_secretPanel->setViewportRange(-1.0, -1.0);
        positionFloatingMinimap();
        return;
    }

    const int currentVal = vsb->value();
    qreal start = qBound<qreal>(0.0, qreal(currentVal) / qreal(denom), 1.0);
    qreal end = qBound<qreal>(start + 0.001, qreal(currentVal + pageStep) / qreal(denom), 1.0);
    m_secretPanel->setViewportRange(start, end);
    positionFloatingMinimap();
}

void MainWindow::positionFloatingMinimap()
{
    if (!m_secretPanel)
        return;

    if (!m_verticalScrollBar) {
        m_secretPanel->hide();
        return;
    }

    if (m_secretPanel->parentWidget() != m_verticalScrollBar)
        m_secretPanel->setParent(m_verticalScrollBar);

    const QRect sbGeom = m_verticalScrollBar->rect();
    const int w = sbGeom.width();
    const int h = sbGeom.height();

    m_secretPanel->setFixedWidth(w);
    m_secretPanel->setGeometry(0, 0, w, h);
    m_secretPanel->raise();
    m_secretPanel->show();
}

void MainWindow::adjustToolBarStyle()
{
    if (!m_toolbar)
        return;
    const int threshold = 900;
    const Qt::ToolButtonStyle desired = (width() < threshold)
        ? Qt::ToolButtonIconOnly
        : Qt::ToolButtonTextBesideIcon;
    if (m_toolbar->toolButtonStyle() != desired)
        m_toolbar->setToolButtonStyle(desired);
}

bool MainWindow::computePageOffsets(QVector<qreal>& offsets, qreal& totalHeight) const
{
    offsets.clear();
    totalHeight = 0.0;

    if (!m_doc)
        return false;
    const int pageCount = m_doc->pageCount();
    if (pageCount <= 0)
        return false;

    QVector<qreal> pageHeights = m_secretPageHeights;
    if (pageHeights.size() != pageCount) {
        pageHeights.clear();
        pageHeights.reserve(pageCount);
        for (int i = 0; i < pageCount; ++i)
            pageHeights.append(qMax<qreal>(m_doc->pagePointSize(i).height(), 1.0));
    }

    offsets.resize(pageCount);
    qreal acc = 0.0;
    for (int i = 0; i < pageCount; ++i) {
        offsets[i] = acc;
        acc += (i < pageHeights.size()) ? pageHeights.at(i) : 1.0;
    }
    totalHeight = acc > 0.0 ? acc : 1.0;
    return true;
}

void MainWindow::updateNormalSearchMinimap(const QString& term)
{
    if (!m_secretPanel)
        return;

    const QString trimmed = term.trimmed();
    if (!m_doc || m_doc->pageCount() <= 0 || trimmed.size() < 2) {
        if (m_currentMinimapSource == MinimapSource::Normal)
            clearMinimapMarkers(tr("0 Results"));
        return;
    }

    const int resultCount = m_searchModel ? m_searchModel->rowCount(QModelIndex()) : 0;
    if (resultCount <= 0) {
        clearMinimapMarkers(tr("0 Results"));
        m_currentMinimapSource = MinimapSource::Normal;
        return;
    }

    QVector<qreal> offsets;
    qreal totalHeight = 0.0;
    if (!computePageOffsets(offsets, totalHeight)) {
        clearMinimapMarkers(tr("No document"));
        return;
    }

    QVector<MiniMapMarker> markers;
    markers.reserve(resultCount);
    const QColor highlightColor(255, 215, 0, 180);
    for (int i = 0; i < resultCount; ++i) {
        QPdfLink link = m_searchModel->resultAtIndex(i);
        if (!link.isValid())
            continue;
        const int page = link.page();
        if (page < 0 || page >= offsets.size())
            continue;
        QRectF rect;
        const auto rects = link.rectangles();
        if (!rects.isEmpty())
            rect = rects.first();
        const qreal localY = rect.isValid() ? rect.center().y() : 0.0;

        MiniMapMarker marker;
        marker.page = page;
        marker.label = trimmed;
        marker.color = highlightColor;
        marker.pageRect = rect;
        marker.normalizedPos = qBound<qreal>(0.0, (offsets.at(page) + localY) / totalHeight, 1.0);
        markers.append(marker);
    }

    if (markers.isEmpty()) {
        clearMinimapMarkers(tr("0 Results"));
        m_currentMinimapSource = MinimapSource::Normal;
        return;
    }

    m_secretPanel->setMarkers(markers);
    m_secretPanel->setStatusMessage(tr("%1 Results").arg(markers.size()));
    m_currentMinimapSource = MinimapSource::Normal;
}

void MainWindow::runSecretSearch(const QString& termsText)
{
    if (!m_secretPanel) return;
    if (!m_doc || m_doc->pageCount() <= 0) {
        clearMinimapMarkers(tr("No PDF open"));
        return;
    }

    QStringList rawParts = termsText.split(QLatin1Char(';'));
    QStringList terms;
    terms.reserve(rawParts.size());
    for (const QString& part : std::as_const(rawParts)) {
        const QString cleaned = part.trimmed();
        if (!cleaned.isEmpty())
            terms << cleaned;
    }

    if (terms.isEmpty()) {
        clearMinimapMarkers(tr("Please enter search terms."));
        return;
    }

    QVector<MiniMapMarker> markers;
    QVector<int> counts;
    const int totalMatches = collectMarkersForTerms(terms, markers, counts);

    QStringList pieces;
    for (int i = 0; i < terms.size(); ++i) {
        pieces << QStringLiteral("%1:%2").arg(terms.at(i)).arg(counts.at(i));
    }

    QString summary;
    if (totalMatches == 0) {
        summary = tr("No results found");
    } else {
        summary = pieces.join(QStringLiteral("  |  "));
        summary.append(QStringLiteral("  ||  Total: %1").arg(totalMatches));
    }

    m_secretPanel->setMarkers(markers);
    m_secretPanel->setStatusMessage(summary);
    m_currentMinimapSource = MinimapSource::Secret;
}

void MainWindow::setOriginalFile(const QString& originalPath)
{
    if (originalPath.isEmpty()) {
        m_originalFilePath.clear();
        if (m_openOriginalAct) {
            m_openOriginalAct->setVisible(false);
            m_openOriginalAct->setIcon(QIcon());
        }
        return;
    }

    QFileInfo fi(originalPath);
    m_originalFilePath = fi.absoluteFilePath();

    // Set title to original file name
    setWindowTitle(fi.fileName());

    // Determine icon based on extension
    const QString ext = fi.suffix().toLower();
    QIcon fileIcon;
    if (ext == QStringLiteral("pdf")) {
        fileIcon = QIcon(QStringLiteral(":/icons/pdf.ico"));
    } else if (ext == QStringLiteral("udf")) {
        fileIcon = QIcon(QStringLiteral(":/icons/udf.ico"));
    } else {
        // Default file icon for other extensions
        fileIcon = style()->standardIcon(QStyle::SP_FileIcon);
    }

    if (m_openOriginalAct) {
        m_openOriginalAct->setIcon(fileIcon);
        m_openOriginalAct->setVisible(true);
        m_openOriginalAct->setToolTip(tr("Open: %1").arg(fi.fileName()));
    }
}

int MainWindow::collectMarkersForTerms(const QStringList& terms,
                                       QVector<MiniMapMarker>& markers,
                                       QVector<int>& counts)
{
    markers.clear();
    counts.clear();

    if (!m_doc || terms.isEmpty())
        return 0;

    if (m_secretPageHeights.size() != m_doc->pageCount())
        updateSecretPageMetrics();

    QVector<qreal> pageHeights = m_secretPageHeights;
    if (pageHeights.size() != m_doc->pageCount()) {
        pageHeights.clear();
        const int pc = m_doc->pageCount();
        pageHeights.reserve(pc);
        for (int i = 0; i < pc; ++i)
            pageHeights.append(qMax<qreal>(m_doc->pagePointSize(i).height(), 1.0));
    }

    const int pageCount = m_doc->pageCount();
    if (pageCount <= 0)
        return 0;

    QVector<qreal> pageOffsets(pageCount, 0.0);
    qreal acc = 0.0;
    for (int i = 0; i < pageCount; ++i) {
        pageOffsets[i] = acc;
        acc += (i < pageHeights.size()) ? pageHeights.at(i) : 1.0;
    }
    const qreal totalHeight = acc > 0.0 ? acc : 1.0;

    counts = QVector<int>(terms.size(), 0);
    const QColor highlightColor(255, 215, 0, 180);
    int totalMatches = 0;

    for (int page = 0; page < pageCount; ++page) {
        QPdfSelection textSel = m_doc->getAllText(page);
        if (!textSel.isValid())
            continue;
        const QString text = textSel.text();
        if (text.isEmpty())
            continue;

        for (int termIdx = 0; termIdx < terms.size(); ++termIdx) {
            const QString& term = terms.at(termIdx);
            int pos = 0;
            while ((pos = text.indexOf(term, pos, Qt::CaseInsensitive)) >= 0) {
                const int advance = qMax(1, term.size());
                QPdfSelection matchSel = m_doc->getSelectionAtIndex(page, pos, term.size());
                if (matchSel.isValid()) {
                    const QRectF bounds = matchSel.boundingRectangle();
                    const qreal localY = bounds.isValid() ? bounds.center().y() : 0.0;
                    MiniMapMarker marker;
                    marker.page = page;
                    marker.label = term;
                    marker.color = highlightColor;
                    marker.pageRect = bounds;
                    const qreal ratio = qBound<qreal>(0.0, (pageOffsets.at(page) + localY) / totalHeight, 1.0);
                    marker.normalizedPos = ratio;
                    markers.append(marker);
                    counts[termIdx] += 1;
                    ++totalMatches;
                }
                pos += advance;
            }
        }
    }

    return totalMatches;
}
