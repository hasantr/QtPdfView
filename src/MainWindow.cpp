#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QPdfDocument>
#include <QPdfView>
#include <QPdfSearchModel>
#include "SelectablePdfView.h"
#include <QShortcut>
#include <QToolBar>

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

    auto* tb = addToolBar(tr("Ara"));
    tb->setMovable(false);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(tr("Ara (Enter ile vurgula)"));
    tb->addWidget(m_searchEdit);

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

    setWindowTitle(fi.fileName());
}
