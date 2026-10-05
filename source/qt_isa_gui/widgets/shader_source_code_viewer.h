//=============================================================================
// Copyright (c) 2020-2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Header for shader source code viewer.
//=============================================================================

#ifndef QTISAGUI_WIDGETS_SHADER_SOURCE_CODE_VIEWER_H_
#define QTISAGUI_WIDGETS_SHADER_SOURCE_CODE_VIEWER_H_

#include <memory>
#include <string>
#include <vector>

#include <QAction>
#include <QMenu>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QSize>
#include <QTimer>
#include <QWidget>

#include "qt_isa_gui/utility/shader_source_syntax_highlighter.h"

/// @brief ShaderSourceCodeViewer is a read-only plain text widget for viewing and searching shader source code.
///
/// It supports basic syntax highlighting and search highlighting.
class ShaderSourceCodeViewer : public QPlainTextEdit
{
    Q_OBJECT

public:
    /// @brief Finds all occurrences of a substring within a given text.
    ///
    /// Search the provided text for all occurrences of the specified substring
    /// and record the starting indices of each occurrence in the output vector.
    ///
    /// @param [in]  text                 The text to search within.
    /// @param [in]  text_to_find         The substring to search for.
    /// @param [out] search_result_indices A vector to store the starting indices of each occurrence of the substring.
    static void FindSearchResultIndices(const QString& text, const QString& text_to_find, std::vector<size_t>& search_result_indices);

    /// @brief Constructor.
    ///
    /// Make read only, disable word wrap, disable drops.
    /// Connect signals, setup context menu, set font and colors.
    ///
    /// @param [in] parent The parent widget.
    /// @param [in] lang   The shader source language for syntax highlighting.
    ShaderSourceCodeViewer(QWidget* parent = nullptr, ShaderSourceLanguage lang = ShaderSourceLanguage::Unknown);

    /// @brief Get the text of the title bar notification currently set for this editor.
    ///
    /// @return The title bar notification text.
    const std::string& GetTitleBarText() const;

    /// @brief Get the selected line number that the cursor is on.
    ///
    /// @return The line number of the currently selected line.
    int GetSelectedLineNumber() const;

    /// @brief Retrieve the text at the specified line.
    ///
    /// @param [in]  line_number The line number to retrieve text from.
    /// @param [out] text        The text at the specified line.
    ///
    /// @return true if the line number is valid and text was retrieved, false otherwise.
    bool GetTextAtLine(int line_number, QString& text) const;

    /// @brief A callback used to paint the line number area.
    ///
    /// @param [in] event The paint event.
    void LineNumberAreaPaintEvent(QPaintEvent* event);

    /// @brief Compute the width of the line number area.
    ///
    /// @return The width of the line number area.
    int LineNumberAreaWidth() const;

    /// @brief Scroll the editor to the given line number.
    ///
    /// @param [in] line_number The line number to scroll to.
    virtual void ScrollToLine(int line_number);

    /// @brief Sets the contents of the source code view.
    ///
    /// @param [in] txt The text to set in the source code view.
    void SetText(const QString& txt);

    /// @brief Clears the contents of the source code view.
    void ClearText();

    /// @brief Set the text of the title bar notification for this editor.
    ///
    /// @param [in] text The title bar notification text to set.
    void SetTitleBarText(const std::string& text);

    /// @brief Set required syntax highlighting.
    ///
    /// @param [in] lang The shader source language for syntax highlighting.
    ///
    /// @return true if the syntax highlighting was successfully set, false otherwise.
    bool SetSyntaxHighlighting(ShaderSourceLanguage lang);

    /// @brief Highlight lines in the source code.
    ///
    /// Apply a colored highlight to the background of each given row and notify the disassembly view.
    ///
    /// @param [in] line_indices The list of line indices to highlight.
    void SetHighlightedLines(const QList<int>& line_indices);

public slots:
    /// @brief Lines in the disassembly view were highlighted. Apply a colored highlight to the background of each given row in the source code. Don't notify the disassembly view.
    ///
    /// @param [in] line_indices The list of line indices to highlight.
    virtual void HandleHighlightedLinesSet(const QList<int>& line_indices);

    /// @brief Update left margin based on current line number area width.
    virtual void UpdateLineNumberAreaWidth();

signals:
    /// @brief A signal emitted when the source editor is hidden.
    void EditorHidden();

    /// @brief A signal emitted when the source editor is resized.
    void EditorResized();

    /// @brief A signal emitted when the user changes the selected line index.
    ///
    /// @param [in] editor     The source code viewer that emitted the signal.
    /// @param [in] line_index The new selected line index.
    void SelectedLineChanged(ShaderSourceCodeViewer* editor, int line_index);

protected slots:

    /// @brief Show this source view's context menu to the user.
    ///
    /// @param [in] location The location where the context menu should be displayed.
    virtual void ShowContextMenu(const QPoint& location);

    /// @brief Scroll the line number area or request it to repaint itself.
    ///
    /// @param [in] rect The rectangle to refresh if no dy is provided.
    /// @param [in] dy   Scroll delta to apply to the line number area.
    virtual void UpdateLineNumberArea(const QRect& rect, const int dy);

    /// @brief Update cursor position assuming no correlation.
    virtual void UpdateCursorPosition();

protected:
    /// @brief An overridden paint handler responsible for painting a blinking cursor when the editor doesn't have focus.
    ///
    /// @param [in] event The paint event.
    virtual void paintEvent(QPaintEvent* event) Q_DECL_OVERRIDE;

    /// @brief An overridden resize handler responsible for recomputing editor geometry.
    ///
    /// @param [in] event The resize event.
    virtual void resizeEvent(QResizeEvent* event) Q_DECL_OVERRIDE;

    /// @brief An overridden "widget was hidden" handler used to emit a signal indicating a visibility change.
    ///
    /// @param [in] event The hide event.
    virtual void hideEvent(QHideEvent* event) Q_DECL_OVERRIDE;

    /// @brief The overridden mousePressEvent.
    ///
    /// @param [in] event The mouse press event.
    virtual void mousePressEvent(QMouseEvent* event) Q_DECL_OVERRIDE;

    /// @brief Mouse double-click event.
    ///
    /// @param [in] event The mouse double-click event.
    virtual void mouseDoubleClickEvent(QMouseEvent* event) Q_DECL_OVERRIDE;

    /// @brief Connect the editor signals.
    void ConnectSignals();

    /// @brief Used to append a row highlight selection for the current line.
    ///
    /// @param [in,out] selections The list of extra selections to append to.
    virtual void HighlightCursorLine(QList<QTextEdit::ExtraSelection>& selections);

    /// @brief Used to append row highlights for correlated source code lines.
    ///
    /// @param [in,out] selections The list of extra selections to append to.
    virtual void HighlightCorrelatedSourceLines(QList<QTextEdit::ExtraSelection>& selections);

    /// @brief Helper function that performs the logic which is related to a cursor position update event.
    ///
    /// @param [in] is_correlated True if the event is correlation-based, false otherwise.
    void UpdateCursorPositionHelper(bool is_correlated);

    /// @brief Get the text of the current line.
    ///
    /// @param [out] line_txt The text of the current line.
    ///
    /// @return true if the line text was retrieved successfully, false otherwise.
    bool GetCurrentLineText(QString& line_txt) const;

    /// @brief Check if the provided line is an include directive.
    ///
    /// @param [in] line_text The line text to check.
    ///
    /// @return true if the given text represents an include directive, false otherwise.
    bool IsIncludeDirectiveLine(const QString& line_text) const;

    bool       is_cursor_visible_    = true;  ///< The flag indicating if the cursor is visible or hidden.
    int        last_cursor_position_ = 0;     ///< The last selected line number, used to suppress redundant SelectedLineChanged emissions.
    QTimer*    cursor_blink_timer_;           ///< A timer used to toggle the visibility of the blinking cursor.
    QList<int> highlighted_row_indices_;      ///< The list of rows painted with the highlight color.
    QWidget*   line_number_area_ = nullptr;   ///< The line number display widget.
    std::unique_ptr<ShaderSyntaxHighlighter> syntax_highlighter_ =
        nullptr;                                               ///< The syntax highlighter used to alter the rendering of keywords in the source editor.
    std::string            title_bar_notification_text_;       ///< The notification text shown in the Code Editor title bar.
    QAction*               copy_text_action_       = nullptr;  ///< Action for copying text.
    QAction*               select_all_text_action_ = nullptr;  ///< Action for selecting all text.
    std::unique_ptr<QMenu> context_menu_           = nullptr;  ///< This source view's context menu.
    QColor                 original_highlight_color_;          ///< The original palette highlight color, saved to restore after double-click highlighting.

private slots:
    /// @brief Toggle internal cursor visibility state and request repaint.
    void HandleToggleCursorVisibility();
};

/// @brief A widget used to paint the line number gutter in the source editor.
class LineNumberArea : public QWidget
{
public:
    /// @brief Constructor.
    ///
    /// @param [in] editor The source code viewer that owns this line number area.
    LineNumberArea(ShaderSourceCodeViewer* editor)
        : QWidget(editor)
        , code_editor_(editor)
    {
    }

    /// @brief Override the sizeHint based on the line number gutter width.
    ///
    /// @return The size hint for the line number area.
    virtual QSize sizeHint() const Q_DECL_OVERRIDE
    {
        return QSize(code_editor_->LineNumberAreaWidth(), 0);
    }

protected:
    /// @brief Overridden paint for drawing the gutter with line numbers.
    ///
    /// @param [in] event The paint event.
    virtual void paintEvent(QPaintEvent* event) Q_DECL_OVERRIDE
    {
        code_editor_->LineNumberAreaPaintEvent(event);
    }

    /// @brief Overridden double click event for the line number gutter area.
    ///
    /// @param [in] event The mouse double-click event.
    virtual void mouseDoubleClickEvent(QMouseEvent* event) Q_DECL_OVERRIDE
    {
        event->accept();
    }

private:
    ShaderSourceCodeViewer* code_editor_ = nullptr;  ///< The editor where line numbers will be painted.
};
#endif  // QTISAGUI_WIDGETS_SHADER_SOURCE_CODE_VIEWER_H_
