#include "MiniMapWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
constexpr int kMiniMapDefaultWidthPx = 22;
constexpr int kMiniMapMinWidthPx = 10;
constexpr int kMiniMapMaxWidthPx = 64;
}

MiniMapWidget::MiniMapWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setStyleSheet(QStringLiteral("background: transparent;"));
    setAutoFillBackground(false);
    setMinimumWidth(kMiniMapMinWidthPx);
    setMaximumWidth(kMiniMapMaxWidthPx);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize MiniMapWidget::minimumSizeHint() const
{
    return QSize(kMiniMapMinWidthPx, 160);
}

QSize MiniMapWidget::sizeHint() const
{
    return QSize(kMiniMapDefaultWidthPx, 240);
}

void MiniMapWidget::setPageHeights(const QVector<qreal>& heights)
{
    m_pageHeights = heights;
    update();
}

void MiniMapWidget::setMarkers(const QVector<MiniMapMarker>& markers)
{
    m_markers = markers;
    std::stable_sort(m_markers.begin(), m_markers.end(), [](const MiniMapMarker& a, const MiniMapMarker& b){
        return a.normalizedPos < b.normalizedPos;
    });
    update();
}

void MiniMapWidget::setViewportRange(qreal startNormalized, qreal endNormalized)
{
    if (startNormalized < 0.0 || endNormalized < 0.0 || endNormalized <= startNormalized) {
        if (m_hasViewportRange) {
            m_hasViewportRange = false;
            update();
        }
        return;
    }

    const qreal clampedStart = qBound<qreal>(0.0, startNormalized, 1.0);
    const qreal clampedEnd = qBound<qreal>(clampedStart + 0.001, endNormalized, 1.0);
    m_viewportStart = clampedStart;
    m_viewportEnd = clampedEnd;
    if (!m_hasViewportRange) {
        m_hasViewportRange = true;
        update();
    } else {
        update();
    }
}

void MiniMapWidget::paintEvent(QPaintEvent* ev)
{
    Q_UNUSED(ev);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = rect();

    const qreal total = totalHeight();
    if (total <= 0.0)
        return;

    if (m_drawPageBackgrounds) {
        const qreal innerX = r.left();
        const qreal innerW = qMax<qreal>(r.width(), 2.0);
        qreal yCursor = r.top();
        QColor pageColor(200, 200, 200, 32);
        for (qreal h : m_pageHeights) {
            const qreal hh = (h / total) * r.height();
            const QRectF pageRect(innerX, yCursor, innerW, qMax(hh, 2.0));
            p.fillRect(pageRect, pageColor);
            yCursor += hh;
        }
    }

    if (m_markers.isEmpty())
        return;

    for (const MiniMapMarker& marker : m_markers) {
        const qreal y = markerToY(marker, r);
        const qreal clampedY = qBound(r.top(), y, r.bottom());
        QPen pen(marker.color);
        pen.setWidthF(1.0);
        p.setPen(pen);
        p.drawLine(r.left() + 2, clampedY, r.right() - 2, clampedY);
    }
}

void MiniMapWidget::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_markers.isEmpty()) {
        QToolTip::hideText();
        return;
    }
    const QRectF r = rect();
    const qreal y = ev->position().y();
    const qreal threshold = 6.0;

    const MiniMapMarker* best = markerNearY(y, threshold, r);
    if (best) {
        const QString hint = QStringLiteral("%1 (Sayfa %2)")
                                 .arg(best->label.isEmpty() ? QObject::tr("Sonuç") : best->label)
                                 .arg(best->page + 1);
        if (hint != m_lastHint)
            QToolTip::showText(ev->globalPosition().toPoint(), hint, this);
        m_lastHint = hint;
    } else {
        QToolTip::hideText();
        m_lastHint.clear();
    }
}

void MiniMapWidget::leaveEvent(QEvent* ev)
{
    QToolTip::hideText();
    m_lastHint.clear();
    QWidget::leaveEvent(ev);
}

void MiniMapWidget::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton && !m_markers.isEmpty()) {
        const QRectF r = rect();
        if (const MiniMapMarker* marker = markerNearY(ev->position().y(), 8.0, r)) {
            emit markerActivated(*marker);
            ev->accept();
            return;
        }
    }
    QWidget::mousePressEvent(ev);
}

qreal MiniMapWidget::totalHeight() const
{
    if (m_pageHeights.isEmpty())
        return 0.0;
    qreal total = 0.0;
    for (qreal v : m_pageHeights)
        total += qMax<qreal>(v, 1.0);
    return total;
}

qreal MiniMapWidget::markerToY(const MiniMapMarker& marker, const QRectF& area) const
{
    const qreal ratio = qBound<qreal>(0.0, marker.normalizedPos, 1.0);
    return area.top() + ratio * area.height();
}

const MiniMapMarker* MiniMapWidget::markerNearY(qreal y, qreal threshold, const QRectF& area) const
{
    const MiniMapMarker* best = nullptr;
    qreal bestDist = threshold;
    for (const MiniMapMarker& marker : m_markers) {
        const qreal my = markerToY(marker, area);
        const qreal dist = std::abs(my - y);
        if (dist <= bestDist) {
            bestDist = dist;
            best = &marker;
        }
    }
    return best;
}
