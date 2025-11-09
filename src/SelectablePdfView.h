#pragma once

#include <QPdfView>
#include <QPdfSelection>
#include <optional>

class SelectablePdfView : public QPdfView {
    Q_OBJECT
public:
    explicit SelectablePdfView(QWidget* parent = nullptr);

    bool hasSelection() const { return m_selection.has_value() && m_selection->isValid(); }
    void clearSelection();
    bool copySelectionToClipboard();
    bool selectAllOnCurrentPage();
    bool selectAllDocument();
    bool copyAllDocumentToClipboard();

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void contextMenuEvent(QContextMenuEvent* ev) override;

private:
    qreal currentScale() const; // page points -> pixels
    QPointF viewportToContent(const QPointF& p) const;
    qreal pageOffsetY(int page) const;
    QPointF contentToPagePointsFor(int page, const QPointF& pContent) const;
    void updateSelectionFromDrag();

    bool m_dragging {false};
    QPointF m_dragStartViewport;
    QPointF m_dragEndViewport;
    std::optional<QPdfSelection> m_selection;
    int m_selectionPage {-1};
    bool m_allDocSelected {false};
};
