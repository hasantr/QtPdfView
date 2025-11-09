#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QFile>
#include <QLineEdit>
#include <QMessageBox>
#include <QPdfDocument>
#include <QPdfView>
#include <QPdfSearchModel>
#include <QPdfPageNavigator>
#include <QPdfPageSelector>
#include "SelectablePdfView.h"
#include <QShortcut>
#include <QToolBar>
#include <QStyle>

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
    m_searchEdit->setPlaceholderText(tr("Ara (Enter: sonraki)"));
    tb->addWidget(m_searchEdit);
    auto* findPrev = tb->addAction(tr("←"));
    auto* findNext = tb->addAction(tr("→"));

    // Search model highlights matches inside the view
    m_searchModel = new QPdfSearchModel(this);
    m_searchModel->setDocument(m_doc);
    m_view->setSearchModel(m_searchModel);
    connect(m_searchEdit, &QLineEdit::textChanged, m_searchModel, &QPdfSearchModel::setSearchString);

    // Navigate between matches with Enter/Shift+Enter
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]{
        int count = m_searchModel->rowCount(QModelIndex());
        if (count <= 0)
            return;
        int idx = m_view->currentSearchResultIndex();
        idx = (idx + 1) % count;
        m_view->setCurrentSearchResultIndex(idx);
    });

    connect(findNext, &QAction::triggered, this, [this]{
        int count = m_searchModel->rowCount(QModelIndex());
        if (count <= 0) return;
        int idx = (m_view->currentSearchResultIndex() + 1) % count;
        m_view->setCurrentSearchResultIndex(idx);
    });
    connect(findPrev, &QAction::triggered, this, [this]{
        int count = m_searchModel->rowCount(QModelIndex());
        if (count <= 0) return;
        int idx = m_view->currentSearchResultIndex();
        idx = (idx - 1 + count) % count;
        m_view->setCurrentSearchResultIndex(idx);
    });

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
    setWindowTitle(fi.fileName());
}
