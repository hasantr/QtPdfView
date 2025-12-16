/**
 * @file SelectablePdfView.h
 * @brief Extended QPdfView with text selection and copy support.
 *
 * SelectablePdfView extends Qt's QPdfView to add text selection capabilities.
 * Users can click and drag to select text, double-click to select words,
 * and use context menu or Ctrl+C to copy.
 *
 * Usage:
 * @code
 *   SelectablePdfView* view = new SelectablePdfView(this);
 *   view->setDocument(pdfDocument);
 *
 *   // Check if user has selected text
 *   if (view->hasSelection()) {
 *       view->copySelectionToClipboard();
 *   }
 *
 *   // Navigate to a specific location
 *   view->ensurePageRectVisible(pageNumber, boundingRect);
 * @endcode
 */

#pragma once

#include <QPdfView>
#include <QPdfSelection>
#include <QChar>
#include <QVector>
#include <optional>

class QResizeEvent;
class QEvent;

/**
 * @class SelectablePdfView
 * @brief PDF view widget with text selection and copy support.
 *
 * Features:
 * - Click and drag to select text
 * - Double-click to select words
 * - Right-click context menu with Copy and Select All
 * - Ctrl+C keyboard shortcut support
 * - Select all on current page or entire document
 */
class SelectablePdfView : public QPdfView {
    Q_OBJECT
public:
    /**
     * @brief Constructs a SelectablePdfView.
     * @param parent Parent widget
     */
    explicit SelectablePdfView(QWidget* parent = nullptr);

    /**
     * @brief Checks if any text is currently selected.
     * @return True if there is a selection, false otherwise
     */
    bool hasSelection() const;

    /**
     * @brief Clears the current text selection.
     */
    void clearSelection();

    /**
     * @brief Copies the selected text to the clipboard.
     * @return True if text was copied, false if no selection
     */
    bool copySelectionToClipboard();

    /**
     * @brief Selects all text on the current page.
     * @return True if selection was made, false otherwise
     */
    bool selectAllOnCurrentPage();

    /**
     * @brief Selects all text in the entire document.
     * @return True if selection was made, false otherwise
     */
    bool selectAllDocument();

    /**
     * @brief Copies all text from the entire document to clipboard.
     * @return True if text was copied, false otherwise
     */
    bool copyAllDocumentToClipboard();

    /**
     * @brief Scrolls to make a specific rectangle on a page visible.
     * @param page Page number (0-indexed)
     * @param rect Rectangle in page coordinates (points)
     * @param margin Extra margin around the rectangle (pixels)
     */
    void ensurePageRectVisible(int page, const QRectF& rect, int margin = 16);

    /**
     * @brief Converts viewport Y coordinate to document point Y.
     * @param viewportY Y coordinate in viewport pixels
     * @return Document Y position in points, or nullopt if invalid
     */
    std::optional<qreal> documentPointYForViewportY(qreal viewportY) const;

    /**
     * @brief Returns total document height in points.
     * @return Sum of all page heights
     */
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

signals:
    /**
     * @brief Emitted when the viewport geometry changes.
     *
     * Connect to this signal to update overlays or minimap
     * when the view is scrolled or resized.
     */
    void viewportGeometryChanged();

private:
    struct TextHitResult {
        int page {-1};
        int charIndex {-1};
        bool hasGlyph {false};
    };

    qreal currentScale() const;
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
};
