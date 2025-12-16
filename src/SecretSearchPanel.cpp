#include "SecretSearchPanel.h"

#include "MiniMapWidget.h"

#include <QVBoxLayout>

SecretSearchPanel::SecretSearchPanel(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    m_minimap = new MiniMapWidget(this);
    m_minimap->setDrawPageBackgrounds(false);
    layout->addWidget(m_minimap, 1);

    connect(m_minimap, &MiniMapWidget::markerActivated, this, [this](const MiniMapMarker& marker){
        emit markerActivated(marker.page, marker.pageRect);
    });
}

void SecretSearchPanel::setPageHeights(const QVector<qreal>& heights)
{
    if (m_minimap)
        m_minimap->setPageHeights(heights);
}

void SecretSearchPanel::setMarkers(const QVector<MiniMapMarker>& markers)
{
    if (m_minimap)
        m_minimap->setMarkers(markers);
}

void SecretSearchPanel::setStatusMessage(const QString& text)
{
    Q_UNUSED(text);
}

void SecretSearchPanel::setViewportRange(qreal start, qreal end)
{
    if (m_minimap)
        m_minimap->setViewportRange(start, end);
}
