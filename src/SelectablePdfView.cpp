#include "SelectablePdfView.h"

#include <QAbstractItemModel>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfPageNavigator>
#include <QScrollBar>

SelectablePdfView::SelectablePdfView(QWidget* parent)
    : QPdfView(parent)
{
}

void SelectablePdfView::clearSelection()
{
    m_selection.reset();
    viewport()->update();
}

qreal SelectablePdfView::currentScale() const
{
    // FitToWidth: scale approximated from viewport width and page point width
    const int page = pageNavigator() ? pageNavigator()->currentPage() : 0;
    if (!document() || page < 0)
        return 1.0;

    QSizeF pts = document()->pagePointSize(page);
    if (pts.width() <= 0.0)
        return 1.0;

    const auto m = documentMargins();
    const qreal availW = viewport()->width() - m.left() - m.right();
    if (availW <= 0)
        return 1.0;
    return availW / pts.width();
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
    QColor fill(0, 120, 215, 70); // Windows selection-like
    QColor stroke(0, 120, 215, 180);
    p.setPen(QPen(stroke, 1.0));
    p.setBrush(fill);

    const qreal s = currentScale();
    const auto m = documentMargins();
    const int hOff = horizontalScrollBar()->value();
    const int vOff = verticalScrollBar()->value();
    const qreal yOffPage = pageOffsetY(m_selectionPage);

    const auto polys = m_selection->bounds();
    for (const QPolygonF& polyPts : polys) {
        QPolygonF polyPx;
        polyPx.reserve(polyPts.size());
        for (const QPointF& pt : polyPts) {
            QPointF px(m.left() + pt.x() * s - hOff,
                       m.top() + yOffPage + pt.y() * s - vOff);
            polyPx << px;
        }
        p.drawPolygon(polyPx);
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

QPointF SelectablePdfView::contentToPagePointsFor(int page, const QPointF& pContent) const
{
    const auto m = documentMargins();
    const qreal s = currentScale();
    const qreal yOff = pageOffsetY(page);
    return QPointF((pContent.x() - m.left()) / s,
                   (pContent.y() - m.top() - yOff) / s);
}

bool SelectablePdfView::copySelectionToClipboard()
{
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
    viewport()->update();
    return hasSelection();
}

void SelectablePdfView::contextMenuEvent(QContextMenuEvent* ev)
{
    QMenu menu(this);
    QAction* actCopy = menu.addAction(tr("Kopyala"));
    actCopy->setEnabled(hasSelection());
    QAction* actSelectAll = menu.addAction(tr("Tümünü Seç (Bu Sayfa)"));

    QAction* chosen = menu.exec(ev->globalPos());
    if (!chosen) return;
    if (chosen == actCopy) {
        copySelectionToClipboard();
    } else if (chosen == actSelectAll) {
        selectAllOnCurrentPage();
    }
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
