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
#include <QPdfDocument>
#include <QPdfView>
#include <QPdfSearchModel>
#include <QPdfPageNavigator>
#include <QPdfPageSelector>
#include "SelectablePdfView.h"
#include <QShortcut>
#include <QToolBar>
#include <QStyle>
#include <QPainter>
#include <QPrinter>
#include <QPrintDialog>
#include <QTimer>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
    setupShortcuts();
}

void MainWindow::setupUi()
{
    m_doc = new QPdfDocument(this);
    m_view = new SelectablePdfView(this);
    m_view->setDocument(m_doc);

    // Fast default: directly MultiPage + FitToWidth (no double render)
    m_view->setPageMode(QPdfView::PageMode::MultiPage);
    m_view->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    m_view->setPageSpacing(0);
    m_view->setDocumentMargins(QMargins());

    setCentralWidget(m_view);

    auto* tb = addToolBar(tr("PDF"));
    tb->setMovable(false);
    tb->setIconSize(QSize(20, 20));
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    // Save As (PDF) first
    QIcon saveIcon = style()->standardIcon(QStyle::SP_DialogSaveButton);
    auto* saveAct = tb->addAction(saveIcon, tr("Kaydet"));
    saveAct->setToolTip(tr("Farklı Kaydet (PDF)"));
    // Print button next to save (use theme icon if available)
    auto* printAct = tb->addAction(tr("Yazdır"));
    printAct->setToolTip(tr("Yazdır"));
    
    // Page navigation
    QIcon prevIcon = style()->standardIcon(QStyle::SP_ArrowBack);
    QIcon nextIcon = style()->standardIcon(QStyle::SP_ArrowForward);
    auto* prevPage = tb->addAction(prevIcon, tr("Önceki"));
    auto* nextPage = tb->addAction(nextIcon, tr("Sonraki"));
    prevPage->setShortcut(Qt::Key_PageUp);
    nextPage->setShortcut(Qt::Key_PageDown);
    prevPage->setToolTip(tr("Önceki Sayfa (PgUp)"));
    nextPage->setToolTip(tr("Sonraki Sayfa (PgDn)"));
    tb->addSeparator();

    // Page selector
    m_pageCountLabel = new QLabel(this);
    m_pageCountLabel->setMinimumWidth(48);
    m_pageCountLabel->setAlignment(Qt::AlignCenter);
    tb->addWidget(m_pageCountLabel); // toplam sayfa sayısı solunda
    auto* pageSel = new QPdfPageSelector(this);
    pageSel->setDocument(m_doc);
    tb->addWidget(pageSel);
    tb->addSeparator();

    // Zoom controls
    QIcon zoomOutIcon = style()->standardIcon(QStyle::SP_ArrowDown);
    QIcon zoomInIcon  = style()->standardIcon(QStyle::SP_ArrowUp);
    auto* zoomOut = tb->addAction(zoomOutIcon, tr("-"));
    auto* zoomIn  = tb->addAction(zoomInIcon,  tr("+"));
    zoomIn->setShortcut(QKeySequence::ZoomIn);
    zoomOut->setShortcut(QKeySequence::ZoomOut);
    zoomIn->setToolTip(tr("Yakınlaştır (Ctrl +)"));
    zoomOut->setToolTip(tr("Uzaklaştır (Ctrl -)"));
    auto* fitW    = tb->addAction(tr("Genişlik"));
    auto* fitV    = tb->addAction(tr("Sayfa"));
    fitW->setToolTip(tr("Genişliğe Sığdır"));
    fitV->setToolTip(tr("Sayfaya Sığdır"));
    tb->addSeparator();

    // Search box and nav
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(tr("Ara (min 2 karakter)"));
    tb->addWidget(m_searchEdit);
    QIcon findPrevIcon = style()->standardIcon(QStyle::SP_MediaSkipBackward);
    QIcon findNextIcon = style()->standardIcon(QStyle::SP_MediaSkipForward);
    m_actFindPrev = tb->addAction(findPrevIcon, QString());
    m_actFindNext = tb->addAction(findNextIcon, QString());
    m_actFindNext->setShortcut(QKeySequence::FindNext);
    m_actFindPrev->setShortcut(QKeySequence::FindPrevious);
    m_actFindNext->setToolTip(tr("Sonraki eşleşme (F3)"));
    m_actFindPrev->setToolTip(tr("Önceki eşleşme (Shift+F3)"));
    m_searchStatus = new QLabel(tr("0 sonuç"), this);
    m_searchStatus->setMinimumWidth(64);
    m_searchStatus->setAlignment(Qt::AlignCenter);
    tb->addWidget(m_searchStatus);

    // (Pin/FocusMode removed)
    // Debounced search init; model is created lazily on first need
    m_searchDebounce = new QTimer(this);
    m_searchDebounce->setSingleShot(true);
    m_searchDebounce->setInterval(150); // debounce ~150ms
    connect(m_searchDebounce, &QTimer::timeout, this, [this]{
        const QString txt = m_searchEdit->text();
        ensureSearchModel();
        if (!m_searchModel)
            return;
        if (txt.size() >= 2) {
            m_searchModel->setSearchString(txt);
        } else {
            m_searchModel->setSearchString(QString());
            m_view->setCurrentSearchResultIndex(-1);
        }
        updateSearchStatus();
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString&){
        if (!m_searchDebounce) return;
        m_searchDebounce->start();
    });

    // Navigate between matches with Enter/Shift+Enter
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]{
        ensureSearchModel();
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
        ensureSearchModel();
        int count = m_searchModel->rowCount(QModelIndex());
        if (count <= 0) return;
        int idx = (m_view->currentSearchResultIndex() + 1) % count;
        m_view->setCurrentSearchResultIndex(idx);
        jumpToSearchResult(idx);
        updateSearchStatus();
    });
    connect(m_actFindPrev, &QAction::triggered, this, [this]{
        ensureSearchModel();
        int count = m_searchModel->rowCount(QModelIndex());
        if (count <= 0) return;
        int idx = m_view->currentSearchResultIndex();
        idx = (idx - 1 + count) % count;
        m_view->setCurrentSearchResultIndex(idx);
        jumpToSearchResult(idx);
        updateSearchStatus();
    });

    // Status updates
    // These will be connected once the search model is created
    connect(m_view, &QPdfView::currentSearchResultIndexChanged, this, &MainWindow::updateSearchStatus);
    // Page count label updates
    connect(m_doc, &QPdfDocument::pageCountChanged, this, [this](int){ updatePageCountLabel(); });

    // Hook page selector <-> view navigator
    if (auto* nav = m_view->pageNavigator()) {
        connect(nav, &QPdfPageNavigator::currentPageChanged, pageSel, &QPdfPageSelector::setCurrentPage);
        connect(pageSel, &QPdfPageSelector::currentPageChanged, this, [this, nav](int p){ nav->jump(p, QPointF(0,0)); });
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
            QMessageBox::warning(this, tr("Kaydet"), tr("Mevcut dosya yolu bilinmiyor."));
            return;
        }
        QString dest = QFileDialog::getSaveFileName(this, tr("Farklı Kaydet"),
                                                    QDir::home().filePath(QFileInfo(m_currentFilePath).fileName()),
                                                    tr("PDF Dosyaları (*.pdf)"));
        if (dest.isEmpty()) return;
        if (QFileInfo::exists(dest)) QFile::remove(dest);
        if (!QFile::copy(m_currentFilePath, dest)) {
            QMessageBox::critical(this, tr("Kaydet"), tr("Kaydetme başarısız: %1").arg(dest));
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
    auto* copyAct = new QAction(tr("Kopyala"), this);
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
        QMessageBox::critical(this, tr("PDF açılamadı"),
                              tr("Dosya açılamadı: %1\nHata kodu: %2")
                                  .arg(fi.absoluteFilePath()).arg(int(err)));
        return;
    }

    m_currentFilePath = fi.absoluteFilePath();
    m_currentFilePath = fi.absoluteFilePath();
    setWindowTitle(fi.fileName());
    updatePageCountLabel();

<<<<<<< HEAD
    // No progressive switch (avoid double render).

    // Load settings (e.g., FocusMode) after a document is opened
    loadSettings();
    if (m_actFocusPin)
        m_actFocusPin->setChecked(m_focusMode);
=======
    // After first event loop tick, switch to MultiPage + FitToWidth
    QTimer::singleShot(0, this, [this]{
        m_view->setPageMode(QPdfView::PageMode::MultiPage);
        m_view->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    });
>>>>>>> parent of 2d1102a (Ayarlar: FocusMode (raptiye). Odak kaybolunca uygulamayı kapat (toggle/persist settings.ini). Toolbar’ın en sağına raptiye eklendi.)
}

void MainWindow::updateSearchStatus()
{
    if (!m_searchStatus) return;
    const QString term = m_searchEdit ? m_searchEdit->text() : QString();
    const int count = m_searchModel ? m_searchModel->rowCount(QModelIndex()) : 0;
    const int idx = m_view ? m_view->currentSearchResultIndex() : -1;
    if (term.size() < 2 || count <= 0) {
        m_searchStatus->setText(tr("0 sonuç"));
        if (m_actFindPrev) m_actFindPrev->setEnabled(false);
        if (m_actFindNext) m_actFindNext->setEnabled(false);
        return;
    }
    m_searchStatus->setText(QString::fromLatin1("%1/%2").arg(idx >= 0 ? idx + 1 : 0).arg(count));
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

void MainWindow::ensureSearchModel()
{
    if (m_searchInitialized)
        return;
    m_searchInitialized = true;
    m_searchModel = new QPdfSearchModel(this);
    m_searchModel->setDocument(m_doc);
    m_view->setSearchModel(m_searchModel);

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
}
