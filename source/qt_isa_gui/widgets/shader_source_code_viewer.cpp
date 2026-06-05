//=============================================================================
// Copyright (c) 2020-2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for a source code viewer.
//=============================================================================

#include "shader_source_code_viewer.h"

#include <QMenu>
#include <QtWidgets>

#include "qt_common/utils/qt_util.h"

namespace
{

    // Highlight color for the selected line the cursor is on.
    const QColor kColorHighlightedRow[ColorThemeType::kColorThemeTypeCount] = {QColor(Qt::yellow).lighter(170), QColor(80, 80, 40, 100)};

}  // namespace

void ShaderSourceCodeViewer::FindSearchResultIndices(const QString& text, const QString& text_to_find, std::vector<size_t>& search_result_indices)
{
    // Make sure that neither text value is empty.
    if (!text.isEmpty() && !text_to_find.isEmpty())
    {
        // Step the cursor through the entire field of text to search.
        int cursor_index = text.indexOf(text_to_find, 0);

        // Step through the text to search until we hit the end.
        const int search_string_length = text_to_find.size();
        while (cursor_index != -1)
        {
            // Found a result occurrence. Push it into the results list.
            search_result_indices.push_back(static_cast<size_t>(cursor_index));

            // Search for the next result location.
            cursor_index = text.indexOf(text_to_find, cursor_index + search_string_length);
        }
    }
}

ShaderSourceCodeViewer::ShaderSourceCodeViewer(QWidget* parent, ShaderSourceLanguage lang)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);

    line_number_area_ = new LineNumberArea(this);

    // The duration of time between the text cursor blinking on and off.
    static const int kCursorBlinkToggleDurationMs = 500;

    // Create the blinking cursor update timer.
    cursor_blink_timer_ = new QTimer(this);
    cursor_blink_timer_->setInterval(kCursorBlinkToggleDurationMs);
    cursor_blink_timer_->start();

    // Connect signals.
    ConnectSignals();

    // Set the border color.
    setObjectName("sourceCodeEditor");

    // Create the syntax highlighter.
    if (lang != ShaderSourceLanguage::Unknown)
    {
        syntax_highlighter_ = std::make_unique<ShaderSyntaxHighlighter>(document(), lang);
    }

    UpdateLineNumberAreaWidth();

    // Initialize rendering of highlighted lines within the editor.
    UpdateCursorPosition();

    // Set the default font.
    QTextDocument* doc = this->document();
    if (doc != nullptr)
    {
        QFont consolas_font("Consolas");
        consolas_font.setStyleHint(QFont::Monospace);
        consolas_font.setPointSize(9);
        setFont(consolas_font);
        doc->setDefaultFont(consolas_font);

        // A tab is the same width as 4 spaces.
        QFontMetrics metrics(consolas_font);
        int          tab_width = metrics.horizontalAdvance(' ') * 4;
        setTabStopDistance(tab_width);
    }

    // Configure the word wrap mode.
    setWordWrapMode(QTextOption::NoWrap);

    // Disable accepting dropped files.
    setAcceptDrops(false);

    // Copy action.
    copy_text_action_ = new QAction("Copy", this);
    copy_text_action_->setShortcut(QKeySequence("Ctrl+C"));

    // Select All action.
    select_all_text_action_ = new QAction("Select all", this);
    select_all_text_action_->setShortcut(QKeySequence("Ctrl+A"));

    // Copy action.
    connect(copy_text_action_, &QAction::triggered, this, &QPlainTextEdit::copy);

    // Select All action.
    connect(select_all_text_action_, &QAction::triggered, this, &QPlainTextEdit::selectAll);

    context_menu_ = std::unique_ptr<QMenu>(createStandardContextMenu());

    // Reconstruct the context menu.
    context_menu_->clear();
    context_menu_->addAction(copy_text_action_);
    context_menu_->addSeparator();
    context_menu_->addAction(select_all_text_action_);
    setContextMenuPolicy(Qt::CustomContextMenu);

    // Set hand pointer for the context menu.
    context_menu_->setCursor(Qt::PointingHandCursor);

    // Connect the signal for showing the context menu.
    connect(this, &ShaderSourceCodeViewer::customContextMenuRequested, this, &ShaderSourceCodeViewer::ShowContextMenu);

    // Save the original highlight color to support double-click highlighting.
    const auto palette        = this->palette();
    original_highlight_color_ = palette.color(QPalette::Highlight);
}

int ShaderSourceCodeViewer::LineNumberAreaWidth() const
{
    int digits = 1;
    int max    = qMax(1, blockCount());
    while (max >= 10)
    {
        max /= 10;
        ++digits;
    }

    int space = 15 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;

    return space;
}

void ShaderSourceCodeViewer::ScrollToLine(int line_number)
{
    // Compute the last visible text block.
    auto       last_visible_line_position = QPoint(0, viewport()->height() - 1);
    QTextBlock last_visible_block         = cursorForPosition(last_visible_line_position).block();

    // Get the first and last visible line numbers in the editor.
    int first_visible_line_number = firstVisibleBlock().blockNumber();
    int last_visible_line_number  = last_visible_block.blockNumber();

    // Convert the incoming 1-based line number to a 0-based index to match blockNumber().
    int line_index = line_number - 1;

    // Only scroll the textbox if the target line is not currently visible.
    // Scroll 5 lines above the cursor position if the cursor is too close to the editor upper bound.
    const bool is_on_screen = (first_visible_line_number < line_index) && (line_index <= last_visible_line_number);
    if (!is_on_screen)
    {
        // Scroll to 5 lines above the given line index, clamped to the scrollbar minimum.
        int scroll_value = qMax(verticalScrollBar()->minimum(), line_index - 5);
        verticalScrollBar()->setValue(scroll_value);
    }
    else if (line_index - first_visible_line_number < 5)
    {
        verticalScrollBar()->setValue(verticalScrollBar()->value() - 5);
    }
}

void ShaderSourceCodeViewer::SetText(const QString& txt)
{
    QTextDocument* doc = this->document();
    if (doc != nullptr)
    {
        // Set the text.
        doc->setPlainText(txt);

        // Set the cursor to the first line and column.
        this->moveCursor(QTextCursor::Start);
        this->ensureCursorVisible();
    }
}

void ShaderSourceCodeViewer::ClearText()
{
    QTextDocument* doc = this->document();

    if (doc != nullptr && !doc->isEmpty())
    {
        doc->clear();
    }
}

const std::string& ShaderSourceCodeViewer::GetTitleBarText() const
{
    return title_bar_notification_text_;
}

void ShaderSourceCodeViewer::SetTitleBarText(const std::string& text)
{
    title_bar_notification_text_ = text;
}

void ShaderSourceCodeViewer::HandleToggleCursorVisibility()
{
    // Toggle the visibility of the cursor and trigger a repaint.
    is_cursor_visible_ = !is_cursor_visible_;
    viewport()->update();
}

void ShaderSourceCodeViewer::UpdateLineNumberAreaWidth()
{
    setViewportMargins(LineNumberAreaWidth(), 0, 0, 0);
}

void ShaderSourceCodeViewer::UpdateLineNumberArea(const QRect& rect, const int dy)
{
    if (dy)
    {
        line_number_area_->scroll(0, dy);
    }
    else
    {
        line_number_area_->update(0, rect.y(), line_number_area_->width(), rect.height());
    }

    if (rect.contains(viewport()->rect()))
    {
        UpdateLineNumberAreaWidth();
    }
}

bool ShaderSourceCodeViewer::GetCurrentLineText(QString& line_text) const
{
    // Get the current line.
    QTextCursor cursor = this->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    int lines = 1;
    while (cursor.positionInBlock() > 0)
    {
        cursor.movePosition(QTextCursor::Up);
        lines++;
    }

    QTextBlock block = cursor.block().previous();
    while (block.isValid())
    {
        lines += block.lineCount();
        block = block.previous();
    }

    // Extract the current line's text.
    bool is_valid = GetTextAtLine(lines, line_text) && !line_text.isEmpty();
    return is_valid;
}

bool ShaderSourceCodeViewer::IsIncludeDirectiveLine(const QString& line_text) const
{
    // We use this token to identify if a line is an include directive.
    static const char* INCLUDE_DIR_TOKEN    = "#include ";
    bool               is_include_directive = line_text.startsWith(INCLUDE_DIR_TOKEN);
    return is_include_directive;
}

void ShaderSourceCodeViewer::ShowContextMenu(const QPoint& pt)
{
    Q_UNUSED(pt);

    // Only enable copy if there is text selected.
    QTextCursor cursor        = this->textCursor();
    bool        has_selection = cursor.hasSelection();
    copy_text_action_->setEnabled(has_selection);

    // Show the context menu to the user where the mouse is.
    context_menu_->exec(QCursor::pos());
}

void ShaderSourceCodeViewer::ConnectSignals()
{
    connect(this, &ShaderSourceCodeViewer::blockCountChanged, this, &ShaderSourceCodeViewer::UpdateLineNumberAreaWidth);

    connect(this, &ShaderSourceCodeViewer::updateRequest, this, &ShaderSourceCodeViewer::UpdateLineNumberArea);

    connect(this, &ShaderSourceCodeViewer::cursorPositionChanged, this, &ShaderSourceCodeViewer::UpdateCursorPosition);

    connect(cursor_blink_timer_, &QTimer::timeout, this, &ShaderSourceCodeViewer::HandleToggleCursorVisibility);
}

void ShaderSourceCodeViewer::HighlightCursorLine(QList<QTextEdit::ExtraSelection>& selections)
{
    QTextEdit::ExtraSelection current_line_selection;
    current_line_selection.format.setBackground(kColorHighlightedRow[QtCommon::QtUtils::ColorTheme::Get().GetColorTheme()]);
    current_line_selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    current_line_selection.cursor = textCursor();
    current_line_selection.cursor.clearSelection();

    // Add the current line's selection to the output list.
    selections.append(current_line_selection);
}

void ShaderSourceCodeViewer::HighlightCorrelatedSourceLines(QList<QTextEdit::ExtraSelection>& selections)
{
    QTextDocument* file_document = document();
    for (int row_index : highlighted_row_indices_)
    {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(kColorHighlightedRow[QtCommon::QtUtils::ColorTheme::Get().GetColorTheme()]);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);

        QTextBlock  line_text_block = file_document->findBlockByLineNumber(row_index - 1);
        QTextCursor cursor(line_text_block);
        selection.cursor = cursor;
        selection.cursor.clearSelection();

        // Add the line highlight to the output list.
        selections.append(selection);
    }
}

void ShaderSourceCodeViewer::HandleHighlightedLinesSet(const QList<int>& line_indices)
{
    // Set the list of highlighted lines.
    highlighted_row_indices_ = line_indices;

    // A list of highlights to apply to the source editor lines.
    QList<QTextEdit::ExtraSelection> extra_selections;

    // Automatic correlation: add highlights for the disassembly-correlated source lines.
    HighlightCorrelatedSourceLines(extra_selections);

    // If there aren't any correlated lines to highlight, just highlight the user's currently-selected line.
    if (extra_selections.empty())
    {
        // Standard user selection: add a highlight for the current line.
        HighlightCursorLine(extra_selections);
    }

    // Set the line selections in the source editor.
    setExtraSelections(extra_selections);
}

void ShaderSourceCodeViewer::SetHighlightedLines(const QList<int>& line_indices)
{
    // Set the list of highlighted lines.
    highlighted_row_indices_ = line_indices;

    // Perform the cursor position change related logic.
    UpdateCursorPositionHelper(true);

    if (!line_indices.isEmpty())
    {
        // Emit a signal indicating that the highlighted line has changed.
        emit SelectedLineChanged(this, line_indices[0]);
    }
}

void ShaderSourceCodeViewer::UpdateCursorPositionHelper(bool is_correlated)
{
    // A list of highlights to apply to the source editor lines.
    QList<QTextEdit::ExtraSelection> extra_selections;

    if (is_correlated)
    {
        // Automatic correlation: add highlights for the disassembly-correlated source lines.
        HighlightCorrelatedSourceLines(extra_selections);
    }

    // If there aren't any correlated lines to highlight, just highlight the user's currently-selected line.
    if (extra_selections.empty())
    {
        // Standard user selection: add a highlight for the current line.
        HighlightCursorLine(extra_selections);
    }

    // Set the line selections in the source editor.
    setExtraSelections(extra_selections);

    // Track the current and previously-selected line numbers.
    int current_line = GetSelectedLineNumber();
    if (current_line != last_cursor_position_)
    {
        // If the selected line number has changed, emit a signal.
        emit SelectedLineChanged(this, current_line);
        last_cursor_position_ = current_line;
    }
}

void ShaderSourceCodeViewer::paintEvent(QPaintEvent* event)
{
    // Invoke the default QPlainTextEdit paint function.
    QPlainTextEdit::paintEvent(event);

    // Paint the cursor manually.
    if (is_cursor_visible_)
    {
        QPainter cursor_painter(viewport());

        // Draw the cursor on top of the text editor.
        QRect cursor_line = cursorRect();
        cursor_line.setWidth(1);
        cursor_painter.fillRect(cursor_line, Qt::SolidPattern);
    }
}

void ShaderSourceCodeViewer::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);

    QRect cr = contentsRect();
    line_number_area_->setGeometry(QRect(cr.left(), cr.top(), LineNumberAreaWidth(), cr.height()));

    emit EditorResized();
}

void ShaderSourceCodeViewer::hideEvent(QHideEvent* event)
{
    Q_UNUSED(event);

    emit EditorHidden();
}

void ShaderSourceCodeViewer::UpdateCursorPosition()
{
    UpdateCursorPositionHelper(false);
}

bool ShaderSourceCodeViewer::SetSyntaxHighlighting(ShaderSourceLanguage lang)
{
    bool result = (lang != ShaderSourceLanguage::Unknown);

    if (result)
    {
        syntax_highlighter_.reset();

        syntax_highlighter_ = std::make_unique<ShaderSyntaxHighlighter>(document(), lang);
    }

    return result;
}

int ShaderSourceCodeViewer::GetSelectedLineNumber() const
{
    return textCursor().blockNumber() + 1;
}

bool ShaderSourceCodeViewer::GetTextAtLine(int line_number, QString& text) const
{
    bool ret = false;

    bool is_line_valid = line_number >= 1 && line_number <= document()->blockCount();
    if (is_line_valid)
    {
        QTextBlock lineBlock = document()->findBlockByLineNumber(line_number - 1);
        text                 = lineBlock.text();
        ret                  = true;
    }

    return ret;
}

void ShaderSourceCodeViewer::LineNumberAreaPaintEvent(QPaintEvent* event)
{
    QPainter painter(line_number_area_);

    painter.fillRect(event->rect(), qApp->palette().color(QPalette::AlternateBase));

    QTextBlock block        = firstVisibleBlock();
    int        block_number = block.blockNumber();
    int        top          = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
    int        bottom       = top + static_cast<int>(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom())
    {
        if (block.isVisible() && bottom >= event->rect().top())
        {
            QString number = QString::number(block_number + 1);

            painter.setPen(qApp->palette().color(QPalette::Text));

            QFont default_font = this->document()->defaultFont();
            painter.setFont(default_font);
            painter.drawText(0, top, line_number_area_->width(), fontMetrics().height(), Qt::AlignCenter, number);
        }

        block  = block.next();
        top    = bottom;
        bottom = top + static_cast<int>(blockBoundingRect(block).height());
        ++block_number;
    }
}

void ShaderSourceCodeViewer::mousePressEvent(QMouseEvent* event)
{
    // Close the context menu if it is open.
    if (context_menu_ != nullptr)
    {
        context_menu_->close();
    }

    // Restore the highlight color that could have been modified during double click.
    QPalette palette = this->palette();
    palette.setColor(QPalette::Highlight, original_highlight_color_);
    setPalette(palette);

    QPlainTextEdit::mousePressEvent(event);
}

void ShaderSourceCodeViewer::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);

    QColor kHighlightGreenColor     = QColor::fromRgb(124, 252, 0);
    QColor kHighlightDarkGreenColor = QColor::fromRgb(0, 172, 102);

    if (QtCommon::QtUtils::ColorTheme::Get().GetColorTheme() == ColorThemeType::kColorThemeTypeDark)
    {
        kHighlightGreenColor     = QColor::fromRgb(72, 128, 0);
        kHighlightDarkGreenColor = QColor::fromRgb(0, 51, 26);
    }

    // Override the double-click event to avoid QPlainTextEdit
    // from interpreting the sequence that happens when we simulate
    // a left click as an event of a triple click and select the entire
    // row's text.

    // Get the word under the cursor.
    QTextCursor cursor = this->textCursor();
    cursor.select(QTextCursor::SelectionType::WordUnderCursor);
    this->setTextCursor(cursor);

    // Highlight the current line with yellow background.
    QTextEdit::ExtraSelection current_line_selection;
    current_line_selection.format.setBackground(kColorHighlightedRow[QtCommon::QtUtils::ColorTheme::Get().GetColorTheme()]);
    current_line_selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    current_line_selection.cursor = textCursor();
    current_line_selection.cursor.clearSelection();

    // Add the current line's selection to the output list.
    QList<QTextEdit::ExtraSelection> extra_selections;
    extra_selections.append(current_line_selection);

    // Setup foreground and background colors to
    // highlight the double clicked word.
    QBrush  background_brush(kHighlightGreenColor);
    QBrush  text_brush(QtCommon::QtUtils::ColorTheme::Get().GetCurrentThemeColors().graphics_scene_text_color);
    QPen    outline_color(Qt::gray, 1);
    QString selected_text = cursor.selectedText();

    // Get indices of all matching text.
    std::vector<size_t> search_result_indices;
    FindSearchResultIndices(this->toPlainText(), selected_text, search_result_indices);

    for (auto text_position : search_result_indices)
    {
        QTextEdit::ExtraSelection selected_word;
        selected_word.cursor = QTextCursor(document());
        selected_word.cursor.setPosition(static_cast<int>(text_position));
        selected_word.cursor.setPosition(static_cast<int>(text_position) + selected_text.length(), QTextCursor::KeepAnchor);
        selected_word.format.setForeground(text_brush);
        selected_word.format.setBackground(background_brush);
        selected_word.format.setProperty(QTextFormat::OutlinePen, outline_color);

        // Save the selection.
        extra_selections.append(selected_word);
    }

    // Highlight the current selection in darker green.
    QPalette palette = this->palette();
    palette.setColor(QPalette::Highlight, kHighlightDarkGreenColor);
    setPalette(palette);

    // Set the selections that were just created.
    setExtraSelections(extra_selections);
}