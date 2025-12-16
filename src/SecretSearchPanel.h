#pragma once

#include <QWidget>
#include <QVector>

#include "MiniMapWidget.h"
class MiniMapWidget;

class SecretSearchPanel : public QWidget {
    Q_OBJECT
public:
    explicit SecretSearchPanel(QWidget* parent = nullptr);

    void setPageHeights(const QVector<qreal>& heights);
    void setMarkers(const QVector<MiniMapMarker>& markers);
    void setStatusMessage(const QString& text);
    void setViewportRange(qreal start, qreal end);

signals:
    void markerActivated(int page, const QRectF& rect);

private:
    MiniMapWidget* m_minimap {nullptr};
};
