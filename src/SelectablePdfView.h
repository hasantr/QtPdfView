#pragma once

#include <QPdfView>
#include <QPdfSelection>
#include <QChar>
#include <QVector>
#include <optional>

class QResizeEvent;

class QEvent;

class SelectablePdfView : public QPdfView {
    Q_OBJECT
public:
    explicit SelectablePdfView(QWidget* parent = nullptr);

    bool hasSelection() const;
    void clearSelection();
    bool copySelectionToClipboard();
    bool selectAllOnCurrentPage();
    bool selectAllDocument();
    bool copyAllDocumentToClipboard();
    void ensurePageRectVisible(int page, const QRectF& rect, int margin = 16);
    std::optional<qreal> documentPointYForViewportY(qreal viewportY) const;
    qreal totalDocumentPointsHeight() const;

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void mouseDoubleClickEvent(QMouseEvent* ev) override;
    void contextMenuEvent(QContextMenuEvent* ev) override;
    void leaveEvent(QEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;

private:
    struct TextHitResult {
        int page {-1};
        int charIndex {-1};
        bool hasGlyph {false};
    };

    qreal currentScale() const; // page points -> pixels
    QPointF viewportToContent(const QPointF& p) const;
    qreal pageOffsetY(int page) const;
    qreal contentXOffsetFor(int page) const;
    QPointF contentToPagePointsFor(int page, const QPointF& pContent) const;
    void updateSelectionFromDrag();
    void selectWordAt(const QPointF& viewportPos);
    int pageAtViewportPos(const QPointF& viewportPos) const;
    std::optional<TextHitResult> hitTestCharacter(const QPointF& viewportPos) const;
    void updateHoverCursor(const QPointF& viewportPos);
    static bool isWordCharacter(QChar ch);

    bool m_dragging {false};
    QPointF m_dragStartViewport;
    QPointF m_dragEndViewport;
    std::optional<QPdfSelection> m_selection;
    int m_selectionPage {-1};
    bool m_allDocSelected {false};
    bool m_textCursorActive {false};
    QVector<QPdfSelection> m_allPageSelections;

signals:
    void viewportGeometryChanged();
};
