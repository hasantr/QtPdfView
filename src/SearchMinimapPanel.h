/**
 * @file SearchMinimapPanel.h
 * @brief A panel widget that displays search results as markers on a minimap.
 *
 * SearchMinimapPanel provides a visual overview of search result locations
 * within a PDF document. It overlays on the scrollbar and shows colored
 * markers indicating where matches are found.
 *
 * Usage:
 * @code
 *   SearchMinimapPanel* panel = new SearchMinimapPanel(scrollBar);
 *   panel->setPageHeights(heights);  // Set document page heights
 *   panel->setMarkers(markers);       // Set search result markers
 *   panel->setViewportRange(0.2, 0.4); // Highlight visible area
 * @endcode
 */

#pragma once

#include <QWidget>
#include <QVector>

#include "MiniMapWidget.h"
class MiniMapWidget;

/**
 * @class SearchMinimapPanel
 * @brief Displays search results on a minimap overlay.
 *
 * This widget wraps MiniMapWidget and provides a simplified interface
 * for showing search results as visual markers. It's designed to be
 * placed over a scrollbar to give users a document overview.
 */
class SearchMinimapPanel : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs a SearchMinimapPanel.
     * @param parent Parent widget (typically a scrollbar)
     */
    explicit SearchMinimapPanel(QWidget* parent = nullptr);

    /**
     * @brief Sets the heights of each page in the document.
     * @param heights Vector of page heights in points
     *
     * This information is used to calculate marker positions
     * proportionally within the minimap.
     */
    void setPageHeights(const QVector<qreal>& heights);

    /**
     * @brief Sets the search result markers to display.
     * @param markers Vector of MiniMapMarker structs with position and color info
     */
    void setMarkers(const QVector<MiniMapMarker>& markers);

    /**
     * @brief Sets the currently visible viewport range.
     * @param start Normalized start position (0.0 to 1.0)
     * @param end Normalized end position (0.0 to 1.0)
     *
     * This can be used to highlight which portion of the document
     * is currently visible in the main view.
     */
    void setViewportRange(qreal start, qreal end);

signals:
    /**
     * @brief Emitted when user clicks on a marker.
     * @param page The page number of the clicked marker
     * @param rect The bounding rectangle of the match on that page
     */
    void markerActivated(int page, const QRectF& rect);

private:
    MiniMapWidget* m_minimap {nullptr};
};
