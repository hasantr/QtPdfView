/**
 * @file SelectablePdfView.cpp
 * @brief Implementation of text selection for PDF view.
 */

#include "SelectablePdfView.h"

#include <QAbstractItemModel>
#include <QContextMenuEvent>
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QClipboard>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfPageNavigator>
#include <QResizeEvent>
#include <QScrollBar>
#include <QtGlobal>
#include <QtMath>
#include <array>

SelectablePdfView::SelectablePdfView(QWidget* parent)
    : QPdfView(parent)
{
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
}

bool SelectablePdfView::hasSelection() const
{
    if (m_allDocSelected) {
        for (const QPdfSelection& sel : m_allPageSelections) {
            if (sel.isValid())
                return true;
        }
        return false;
    }
    return m_selection.has_value() && m_selection->isValid();
}

void SelectablePdfView::clearSelection()
{
    m_selection.reset();
    m_allDocSelected = false;
    m_allPageSelections.clear();
    m_selectionPage = -1;
    viewport()->update();
}

qreal SelectablePdfView::currentScale() const
{
    if (!document() || !pageNavigator())
        return 1.0;

    const int page = qBound(0, pageNavigator()->currentPage(), document()->pageCount() - 1);
    const QSizeF pts = document()->pagePointSize(page);
    if (pts.width() <= 0.0 || pts.height() <= 0.0)
        return 1.0;

    const auto m = documentMargins();
    const qreal availW = viewport()->width() - m.left() - m.right();
    const qreal availH = viewport()->height() - m.top() - m.bottom();

    switch (zoomMode()) {
    case QPdfView::ZoomMode::FitToWidth:
        return (availW > 0.0) ? (availW / pts.width()) : 1.0;
    case QPdfView::ZoomMode::FitInView: {
        const qreal sW = (availW > 0.0) ? (availW / pts.width()) : 1.0;
        const qreal sH = (availH > 0.0) ? (availH / pts.height()) : 1.0;
        return qMin(sW, sH);
    }
    default:
        // Custom: zoomFactor is in CSS pixels per PDF point; 1.0 ~= 96/72 on most platforms
        return zoomFactor() * (logicalDpiX() / 72.0);
    }
}

QPointF SelectablePdfView::viewportToContent(const QPointF& p) const
{
    return QPointF(p.x() + horizontalScrollBar()->value(),
                   p.y() + verticalScrollBar()->value());
}

void SelectablePdfView::updateSelectionFromDrag()
{
    if (!document() || !pageNavigator())
        return;
    const int page = pageNavigator()->currentPage();
    const QPointF aPts = contentToPagePointsFor(page, viewportToContent(m_dragStartViewport));
    const QPointF bPts = contentToPagePointsFor(page, viewportToContent(m_dragEndViewport));
    m_selection = document()->getSelection(page, aPts, bPts);
    m_selectionPage = page;
    m_allDocSelected = false;
    m_allPageSelections.clear();
    viewport()->update();
}

void SelectablePdfView::paintEvent(QPaintEvent* ev)
{
    QPdfView::paintEvent(ev);

    if (!hasSelection())
        return;

    // Draw selection overlay
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor fill(0, 120, 215, 70); // Windows selection-like
    const QColor stroke(0, 120, 215, 180);
    p.setPen(QPen(stroke, 1.0));
    p.setBrush(fill);

    const qreal s = currentScale();
    const auto m = documentMargins();
    const int hOff = horizontalScrollBar()->value();
    const int vOff = verticalScrollBar()->value();

    auto drawSelectionForPage = [&](int page, const QPdfSelection& sel) {
        if (!sel.isValid() || page < 0)
            return;
        const qreal xOffCenter = contentXOffsetFor(page);
        const qreal yOffPage = pageOffsetY(page);
        const auto polys = sel.bounds();
        for (const QPolygonF& polyPts : polys) {
            QPolygonF polyPx;
            polyPx.reserve(polyPts.size());
            for (const QPointF& pt : polyPts) {
                QPointF px(xOffCenter + m.left() + pt.x() * s - hOff,
                           m.top() + yOffPage + pt.y() * s - vOff);
                polyPx << px;
            }
            p.drawPolygon(polyPx);
        }
    };

    if (m_allDocSelected && !m_allPageSelections.isEmpty()) {
        const int count = m_allPageSelections.size();
        for (int i = 0; i < count; ++i)
            drawSelectionForPage(i, m_allPageSelections.at(i));
    } else if (m_selection && m_selection->isValid()) {
        drawSelectionForPage(m_selectionPage, *m_selection);
    }
}

qreal SelectablePdfView::pageOffsetY(int page) const
{
    if (!document() || page <= 0)
        return 0.0;
    const qreal s = currentScale();
    qreal y = 0.0;
    const int spacing = pageSpacing();
    for (int i = 0; i < page; ++i) {
        const QSizeF pts = document()->pagePointSize(i);
        y += pts.height() * s + spacing;
    }
    return y;
}

qreal SelectablePdfView::contentXOffsetFor(int page) const
{
    if (!document())
        return 0.0;
    const int safePage = qBound(0, page, document()->pageCount() - 1);
    const QSizeF pts = document()->pagePointSize(safePage);
    if (pts.width() <= 0.0)
        return 0.0;
    const auto m = documentMargins();
    const qreal s = currentScale();
    const qreal contentW = pts.width() * s + m.left() + m.right();
    const qreal extra = viewport()->width() - contentW;
    return extra > 0.0 ? extra / 2.0 : 0.0;
}

QPointF SelectablePdfView::contentToPagePointsFor(int page, const QPointF& pContent) const
{
    const auto m = documentMargins();
    const qreal s = currentScale();
    const qreal yOff = pageOffsetY(page);
    const qreal xOffCenter = contentXOffsetFor(page);
    return QPointF((pContent.x() - m.left() - xOffCenter) / s,
                   (pContent.y() - m.top() - yOff) / s);
}

bool SelectablePdfView::copySelectionToClipboard()
{
    if (m_allDocSelected)
        return copyAllDocumentToClipboard();
    if (!hasSelection())
        return false;
    m_selection->copyToClipboard();
    return true;
}

bool SelectablePdfView::selectAllOnCurrentPage()
{
    if (!document() || !pageNavigator())
        return false;
    const int page = pageNavigator()->currentPage();
    m_selection = document()->getAllText(page);
    m_selectionPage = page;
    m_allDocSelected = false;
    m_allPageSelections.clear();
    viewport()->update();
    return hasSelection();
}

bool SelectablePdfView::selectAllDocument()
{
    if (!document())
        return false;

    m_allPageSelections.clear();
    m_selection.reset();
    m_allDocSelected = false;

    const int pageCount = document()->pageCount();
    if (pageCount <= 0)
        return false;

    m_allPageSelections.reserve(pageCount);
    bool anyValid = false;
    for (int i = 0; i < pageCount; ++i) {
        QPdfSelection sel = document()->getAllText(i);
        if (sel.isValid())
            anyValid = true;
        m_allPageSelections.append(sel);
    }

    if (!anyValid)
        return false;

    m_allDocSelected = true;
    int currentPage = 0;
    if (auto* nav = pageNavigator())
        currentPage = qBound(0, nav->currentPage(), pageCount - 1);
    m_selectionPage = currentPage;
    if (currentPage >= 0 && currentPage < m_allPageSelections.size() &&
        m_allPageSelections.at(currentPage).isValid()) {
        m_selection = m_allPageSelections.at(currentPage);
    }
    viewport()->update();
    return true;
}

bool SelectablePdfView::copyAllDocumentToClipboard()
{
    if (!document()) return false;
    QString all;
    all.reserve(4096);
    const int pc = document()->pageCount();
    for (int i = 0; i < pc; ++i) {
        QPdfSelection sel = document()->getAllText(i);
        if (sel.isValid()) {
            if (!all.isEmpty()) all += QLatin1Char('\n');
            all += sel.text();
        }
    }
    if (all.isEmpty()) return false;
    QGuiApplication::clipboard()->setText(all);
    return true;
}

void SelectablePdfView::contextMenuEvent(QContextMenuEvent* ev)
{
    QMenu menu(this);
    QAction* actCopy = menu.addAction(tr("Copy"));
    actCopy->setEnabled(hasSelection());
    QAction* actSelectAll = menu.addAction(tr("Select All (This Page)"));
    QAction* actSelectAllDoc = menu.addAction(tr("Select All (Document)"));

    QAction* chosen = menu.exec(ev->globalPos());
    if (!chosen) return;
    if (chosen == actCopy) {
        copySelectionToClipboard();
    } else if (chosen == actSelectAll) {
        selectAllOnCurrentPage();
    } else if (chosen == actSelectAllDoc) {
        selectAllDocument();
    }
}

void SelectablePdfView::ensurePageRectVisible(int page, const QRectF& rect, int margin)
{
    if (!document()) return;
    const qreal s = currentScale();
    const auto m = documentMargins();
    const qreal yOff = pageOffsetY(page);

    QRectF rPx(m.left() + rect.left() * s,
               m.top() + yOff + rect.top() * s,
               rect.width() * s,
               rect.height() * s);
    rPx.adjust(-margin, -margin, margin, margin);

    const int vw = viewport()->width();
    const int vh = viewport()->height();
    int hVal = horizontalScrollBar()->value();
    int vVal = verticalScrollBar()->value();

    if (rPx.left() < hVal)
        hVal = int(std::floor(rPx.left()));
    else if (rPx.right() > hVal + vw)
        hVal = int(std::ceil(rPx.right() - vw));

    if (rPx.top() < vVal)
        vVal = int(std::floor(rPx.top()));
    else if (rPx.bottom() > vVal + vh)
        vVal = int(std::ceil(rPx.bottom() - vh));

    horizontalScrollBar()->setValue(hVal);
    verticalScrollBar()->setValue(vVal);
    viewport()->update();
}

void SelectablePdfView::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStartViewport = ev->position();
        m_dragEndViewport = m_dragStartViewport;
        updateSelectionFromDrag();
        ev->accept();
        return;
    }
    QPdfView::mousePressEvent(ev);
}

void SelectablePdfView::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_dragging) {
        m_dragEndViewport = ev->position();
        updateSelectionFromDrag();
        ev->accept();
        return;
    }
    QPdfView::mouseMoveEvent(ev);
    updateHoverCursor(ev->position());
}

void SelectablePdfView::mouseReleaseEvent(QMouseEvent* ev)
{
    if (m_dragging && ev->button() == Qt::LeftButton) {
        m_dragging = false;
        m_dragEndViewport = ev->position();
        updateSelectionFromDrag();
        ev->accept();
        return;
    }
    QPdfView::mouseReleaseEvent(ev);
}

void SelectablePdfView::mouseDoubleClickEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton) {
        m_dragging = false;  // Cancel single click drag
        selectWordAt(ev->position());
        ev->accept();
        return;
    }
    QPdfView::mouseDoubleClickEvent(ev);
}

void SelectablePdfView::leaveEvent(QEvent* ev)
{
    if (m_textCursorActive) {
        if (QWidget* vp = viewport())
            vp->unsetCursor();
        else
            unsetCursor();
        m_textCursorActive = false;
    }
    QPdfView::leaveEvent(ev);
}

void SelectablePdfView::resizeEvent(QResizeEvent* ev)
{
    QPdfView::resizeEvent(ev);
    emit viewportGeometryChanged();
}

void SelectablePdfView::selectWordAt(const QPointF& viewportPos)
{
    if (!document())
        return;

    const auto hit = hitTestCharacter(viewportPos);
    if (!hit || hit->page < 0)
        return;

    QPdfSelection pageTextSel = document()->getAllText(hit->page);
    if (!pageTextSel.isValid())
        return;

    const QString pageText = pageTextSel.text();
    if (pageText.isEmpty() || hit->charIndex < 0 || hit->charIndex >= pageText.size())
        return;

    if (!isWordCharacter(pageText.at(hit->charIndex)))
        return;

    int wordStart = hit->charIndex;
    while (wordStart > 0 && isWordCharacter(pageText.at(wordStart - 1)))
        --wordStart;

    int wordEnd = hit->charIndex + 1;
    const int textSize = pageText.size();
    while (wordEnd < textSize && isWordCharacter(pageText.at(wordEnd)))
        ++wordEnd;

    const int length = wordEnd - wordStart;
    if (length <= 0)
        return;

    QPdfSelection wordSelection = document()->getSelectionAtIndex(hit->page, wordStart, length);
    if (!wordSelection.isValid())
        return;

    m_selection = wordSelection;
    m_selectionPage = hit->page;
    m_allDocSelected = false;
    m_allPageSelections.clear();
    viewport()->update();
}

int SelectablePdfView::pageAtViewportPos(const QPointF& viewportPos) const
{
    if (!document())
        return -1;
    const int pageCount = document()->pageCount();
    if (pageCount <= 0)
        return -1;

    const auto m = documentMargins();
    QPointF contentPos = viewportToContent(viewportPos);
    qreal y = contentPos.y() - m.top();
    if (y < 0)
        y = 0;
    const qreal scale = currentScale();
    const qreal spacing = pageSpacing();
    qreal cursorY = 0.0;
    for (int i = 0; i < pageCount; ++i) {
        const QSizeF pts = document()->pagePointSize(i);
        const qreal pageHeight = pts.height() * scale;
        const qreal nextCursor = cursorY + pageHeight;
        if (y >= cursorY && y < nextCursor)
            return i;
        cursorY = nextCursor + spacing;
    }
    return pageCount - 1;
}

std::optional<SelectablePdfView::TextHitResult> SelectablePdfView::hitTestCharacter(const QPointF& viewportPos) const
{
    if (!document())
        return std::nullopt;

    const int page = pageAtViewportPos(viewportPos);
    if (page < 0)
        return std::nullopt;

    const QPointF contentPos = viewportToContent(viewportPos);
    const QPointF pagePt = contentToPagePointsFor(page, contentPos);

    constexpr qreal probeDelta = 3.0;
    const std::array<QPointF, 4> probes = {
        QPointF(pagePt.x() + probeDelta, pagePt.y()),
        QPointF(pagePt.x() - probeDelta, pagePt.y()),
        QPointF(pagePt.x(), pagePt.y() + probeDelta),
        QPointF(pagePt.x(), pagePt.y() - probeDelta)
    };

    for (const QPointF& probe : probes) {
        QPdfSelection sel = document()->getSelection(page, pagePt, probe);
        if (!sel.isValid())
            continue;
        TextHitResult info;
        info.page = page;
        info.charIndex = sel.startIndex();
        info.hasGlyph = !sel.text().isEmpty();
        return info;
    }

    return std::nullopt;
}

void SelectablePdfView::updateHoverCursor(const QPointF& viewportPos)
{
    QWidget* vp = viewport();
    if (!vp)
        return;

    bool wantTextCursor = false;
    if (document()) {
        if (const auto hit = hitTestCharacter(viewportPos))
            wantTextCursor = hit->hasGlyph;
    }

    if (wantTextCursor != m_textCursorActive) {
        m_textCursorActive = wantTextCursor;
        if (wantTextCursor)
            vp->setCursor(Qt::IBeamCursor);
        else
            vp->unsetCursor();
    }
}

bool SelectablePdfView::isWordCharacter(QChar ch)
{
    if (ch.isLetterOrNumber())
        return true;
    if (ch.category() == QChar::Punctuation_Connector)
        return true;
    return ch == QLatin1Char('_') || ch == QLatin1Char('-');
}

std::optional<qreal> SelectablePdfView::documentPointYForViewportY(qreal viewportY) const
{
    if (!document())
        return std::nullopt;
    const QPointF viewportPos(0.0, viewportY);
    const int page = pageAtViewportPos(viewportPos);
    if (page < 0)
        return std::nullopt;

    QPointF contentPos = viewportToContent(viewportPos);
    QPointF pagePt = contentToPagePointsFor(page, contentPos);
    const qreal pageHeight = document()->pagePointSize(page).height();
    if (pageHeight > 0.0)
        pagePt.setY(qBound<qreal>(0.0, pagePt.y(), pageHeight));

    qreal absoluteY = 0.0;
    const int pageCount = document()->pageCount();
    const int limit = qMax(0, qMin(page, pageCount));
    for (int i = 0; i < limit; ++i)
        absoluteY += document()->pagePointSize(i).height();
    absoluteY += pagePt.y();
    if (absoluteY < 0.0)
        absoluteY = 0.0;
    return absoluteY;
}

qreal SelectablePdfView::totalDocumentPointsHeight() const
{
    if (!document())
        return 0.0;
    qreal total = 0.0;
    const int pageCount = document()->pageCount();
    for (int i = 0; i < pageCount; ++i)
        total += document()->pagePointSize(i).height();
    return total;
}
