/**
 * @file MiniMapWidget.h
 * @brief A widget that displays markers on a vertical minimap.
 *
 * MiniMapWidget provides a visual representation of marker positions
 * within a document. It's designed to overlay on a scrollbar and show
 * where items of interest (like search results) are located.
 *
 * Usage:
 * @code
 *   MiniMapWidget* minimap = new MiniMapWidget(scrollBar);
 *   minimap->setPageHeights({792.0, 792.0, 612.0}); // PDF page heights in points
 *
 *   QVector<MiniMapMarker> markers;
 *   MiniMapMarker m;
 *   m.normalizedPos = 0.25;  // 25% down the document
 *   m.color = Qt::yellow;
 *   m.page = 0;
 *   markers.append(m);
 *   minimap->setMarkers(markers);
 * @endcode
 */

#pragma once

#include <QColor>
#include <QString>
#include <QVector>
#include <QWidget>
#include <QRectF>

/**
 * @struct MiniMapMarker
 * @brief Represents a single marker on the minimap.
 */
struct MiniMapMarker {
    qreal normalizedPos {0.0};  ///< Position within document (0.0 to 1.0)
    QColor color;               ///< Marker line color
    QString label;              ///< Optional label for tooltip
    int page {0};               ///< Page number (0-indexed)
    QRectF pageRect;            ///< Bounding rectangle on the page (in points)
};

/**
 * @class MiniMapWidget
 * @brief Displays document markers as horizontal lines on a vertical strip.
 *
 * This widget is typically placed over a scrollbar to provide a visual
 * overview of where items are located in a long document.
 */
class MiniMapWidget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs a MiniMapWidget.
     * @param parent Parent widget (typically a scrollbar)
     */
    explicit MiniMapWidget(QWidget* parent = nullptr);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    /**
     * @brief Sets the heights of each page for proportional positioning.
     * @param heights Vector of page heights in points
     */
    void setPageHeights(const QVector<qreal>& heights);

    /**
     * @brief Sets the markers to display on the minimap.
     * @param markers Vector of MiniMapMarker structs
     */
    void setMarkers(const QVector<MiniMapMarker>& markers);

    /**
     * @brief Sets the currently visible viewport range.
     * @param startNormalized Start position (0.0 to 1.0)
     * @param endNormalized End position (0.0 to 1.0)
     */
    void setViewportRange(qreal startNormalized, qreal endNormalized);

    /**
     * @brief Enables or disables drawing of page background rectangles.
     * @param enabled True to draw backgrounds, false to hide them
     */
    void setDrawPageBackgrounds(bool enabled) { m_drawPageBackgrounds = enabled; update(); }

signals:
    /**
     * @brief Emitted when a marker is clicked.
     * @param marker The clicked marker
     */
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
