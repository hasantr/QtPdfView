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

    // Favor fast, simple rendering but show all pages
    m_view->setPageMode(QPdfView::PageMode::MultiPage);
    m_view->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    m_view->setPageSpacing(0);
    m_view->setDocumentMargins(QMargins());

    setCentralWidget(m_view);

    auto* tb = addToolBar(tr("PDF"));
    tb->setMovable(false);

    // Save As (PDF) first
    QIcon saveIcon = style()->standardIcon(QStyle::SP_DialogSaveButton);
    auto* saveAct = tb->addAction(saveIcon, tr("Kaydet"));
    // Print button next to save
    auto* printAct = tb->addAction(tr("Yazdır"));
    
    // Page navigation
    auto* prevPage = tb->addAction(tr("Önceki"));
    auto* nextPage = tb->addAction(tr("Sonraki"));
    tb->addSeparator();

    // Page selector
    auto* pageSel = new QPdfPageSelector(this);
    pageSel->setDocument(m_doc);
    tb->addWidget(pageSel);
    tb->addSeparator();

    // Zoom controls
    auto* zoomOut = tb->addAction(tr("-"));
    auto* zoomIn  = tb->addAction(tr("+"));
    auto* fitW    = tb->addAction(tr("Genişlik"));
    auto* fitV    = tb->addAction(tr("Sayfa"));
    tb->addSeparator();

    // Search box and nav
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(tr("Ara (min 2 karakter)"));
    tb->addWidget(m_searchEdit);
    m_actFindPrev = tb->addAction(tr("←"));
    m_actFindNext = tb->addAction(tr("→"));
    m_searchStatus = new QLabel(tr("0 sonuç"), this);
    tb->addWidget(m_searchStatus);

    // Search model highlights matches inside the view
    m_searchModel = new QPdfSearchModel(this);
    m_searchModel->setDocument(m_doc);
    m_view->setSearchModel(m_searchModel);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& txt){
        if (txt.size() >= 2) {
            m_searchModel->setSearchString(txt);
        } else {
            m_searchModel->setSearchString(QString());
            m_view->setCurrentSearchResultIndex(-1);
        }
        updateSearchStatus();
    });

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
    connect(m_view, &QPdfView::currentSearchResultIndexChanged, this, &MainWindow::updateSearchStatus);

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
    if (rects.isEmpty())
        return;
    QPointF loc = rects.first().center();
    if (auto* nav = m_view->pageNavigator())
        nav->jump(link.page(), loc, 0);
}
