#pragma once

#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>
#include <QRectF>

struct MiniMapMarker {
    qreal normalizedPos {0.0}; // 0..1, absolute position within document
    QColor color;
    QString label;
    int page {0};
    QRectF pageRect;
};

class MiniMapWidget : public QWidget {
    Q_OBJECT
public:
    explicit MiniMapWidget(QWidget* parent = nullptr);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    void setPageHeights(const QVector<qreal>& heights);
    void setMarkers(const QVector<MiniMapMarker>& markers);
    void setViewportRange(qreal startNormalized, qreal endNormalized);
    void setDrawPageBackgrounds(bool enabled) { m_drawPageBackgrounds = enabled; update(); }

signals:
    void markerActivated(const MiniMapMarker& marker);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void leaveEvent(QEvent* ev) override;

private:
    QVector<qreal> m_pageHeights;
    QVector<MiniMapMarker> m_markers;
    QString m_lastHint;
    bool m_hasViewportRange {false};
    qreal m_viewportStart {0.0};
    qreal m_viewportEnd {0.0};
    bool m_drawPageBackgrounds {true};

    qreal totalHeight() const;
    qreal markerToY(const MiniMapMarker& marker, const QRectF& area) const;
    const MiniMapMarker* markerNearY(qreal y, qreal threshold, const QRectF& area) const;
};
