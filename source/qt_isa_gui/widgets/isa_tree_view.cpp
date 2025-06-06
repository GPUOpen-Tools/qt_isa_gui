//=============================================================================
// Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Isa tree view implementation.
//=============================================================================

#include "isa_tree_view.h"

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <algorithm>
#include <utility>

#include "qt_common/custom_widgets/scaled_tree_view.h"
#include "qt_common/utils/common_definitions.h"
#include "qt_common/utils/qt_util.h"

#include "isa_item_delegate.h"
#include "isa_widget.h"

/// struct containing info needed to sort model indices.
/// The y position is used for sorting the indices vertically by rows, and the visual column for sorting indices horizontally.
/// If the index is a comment it needs to be placed before all other columns except for line number.
struct CompareIndexInfo
{
    QModelIndex source_index;   ///< The source model index.
    int         visual_column;  ///< The order that the index's column appears visually on the table.
    int         y_pos;          ///< The y pixel coordinate of the index.
};

/// @brief Compare function that sorts model indices by y position and visual column index order.
///
/// Also makes sure comments always come before all other columns except for line number.
///
/// @param lhs The compare info for the left hand side index.
/// @param rhs The compare info for the right hand side model index.
///
/// @return true if the left model index is less than the right model index, false otherwise.
static bool CompareModelIndices(const CompareIndexInfo& lhs, const CompareIndexInfo& rhs)
{
    if (lhs.y_pos == rhs.y_pos)
    {
        const QModelIndex lhs_index = lhs.source_index;
        const QModelIndex rhs_index = rhs.source_index;

        // Put comments in the first row.
        if (lhs_index.data(IsaItemModel::kRowTypeRole).value<IsaItemModel::RowType>() == IsaItemModel::RowType::kComment)
        {
            // Put the opcode column before all other columns except for line number for comments rows.
            if (lhs_index.column() == IsaItemModel::Columns::kOpCode)
            {
                return rhs_index.column() != IsaItemModel::Columns::kLineNumber;
            }
            else if (rhs_index.column() == IsaItemModel::Columns::kOpCode)
            {
                return lhs_index.column() == IsaItemModel::Columns::kLineNumber;
            }
        }

        return lhs.visual_column < rhs.visual_column;
    }

    return lhs.y_pos < rhs.y_pos;
}

IsaTreeView::IsaTreeView(QWidget* parent)
    : QTreeView(parent)
    , isa_scroll_bar_(nullptr)
    , isa_item_delegate_(nullptr)
    , copy_line_numbers_(true)
    , last_pinned_row_(std::pair<int, int>(-1, -1))
    , paint_column_separators_(true)
{
    setObjectName("isa_tree_view_");

    // Allow resizing.
    header()->setSectionResizeMode(QHeaderView::ResizeMode::Interactive);
    header()->setResizeContentsPrecision(ScaledHeaderView::kResizeContentsPrecision_AllRows);
    header()->setSectionsMovable(true);

    connect(header(), &QHeaderView::sectionResized, this, [this](int local_index, int old_size, int new_size) {
        Q_UNUSED(local_index);
        Q_UNUSED(old_size);
        Q_UNUSED(new_size);

        // Make sure any custom painting of code block labels or comments gets repainted.
        viewport()->update();
    });

    // All rows should be the same.
    setUniformRowHeights(true);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Take more space if needed.
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    setMouseTracking(true);

    // Disable autoscroll to mitigate scrolling horizontally when clicking near the edge of a view.
    // Turn it on temporarily in order to allow scrolling vertically using keybinds.
    setAutoScroll(false);

    // Pick a normal fixed width font for the tree.
    QFont consolas_font("Consolas");
    consolas_font.setStyleHint(QFont::Monospace);
    setFont(consolas_font);

    // Make the header font bold.
    auto header_font = header()->font();
    header_font.setBold(true);
    header()->setFont(header_font);

    isa_item_delegate_ = std::make_unique<IsaItemDelegate>(this);
    setItemDelegate(isa_item_delegate_.get());

    // Allow contiguous selection per rows.
    setSelectionMode(QAbstractItemView::SelectionMode::ContiguousSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);

    // Disable alternating row colors and have this view paint them manually instead.
    // The default alternating row color behavior would otherwise paint over our attempt to let labels and comments span columns.
    setAlternatingRowColors(false);

    // Scroll bar to show hot spots and text search matches.
    isa_scroll_bar_ = new IsaVerticalScrollBar(this);

    setVerticalScrollBar(isa_scroll_bar_);

    connect(isa_scroll_bar_, &QScrollBar::valueChanged, this, &IsaTreeView::ScrollBarScrolled);

    connect(this, &QTreeView::expanded, this, &IsaTreeView::IndexExpandedOrCollapsed);
    connect(this, &QTreeView::collapsed, this, &IsaTreeView::IndexExpandedOrCollapsed);
}

IsaTreeView::~IsaTreeView()
{
}

void IsaTreeView::ReplaceDelegate(IsaItemDelegate* delegate)
{
    setItemDelegate(delegate);

    isa_item_delegate_.reset(delegate);
}

void IsaTreeView::RegisterScrollAreas(std::vector<QScrollArea*> container_scroll_areas)
{
    IsaItemDelegate* isa_item_delegate = qobject_cast<IsaItemDelegate*>(itemDelegate());

    if (isa_item_delegate != nullptr)
    {
        isa_item_delegate->RegisterScrollAreas(container_scroll_areas);
    }
}

void IsaTreeView::SetHotSpotLineNumbers(const std::set<QModelIndex>& source_indices)
{
    std::set<int> line_numbers;

    const QSortFilterProxyModel* proxy_model = qobject_cast<QSortFilterProxyModel*>(model());

    for (const auto& source_index : source_indices)
    {
        if (!source_index.parent().isValid())
        {
            // Should be an instruction index.
            continue;
        }

        int line_number = 0;

        // Get the relative line number of the requested hot spot instruction, accounting for the
        // expand/collapse state of previous code blocks.

        if (proxy_model != nullptr)
        {
            const QModelIndex instruction_proxy_index = proxy_model->mapFromSource(source_index);
            const QModelIndex code_block_proxy_index  = proxy_model->mapFromSource(source_index.parent());

            for (int i = 0; i < code_block_proxy_index.row(); i++)
            {
                line_number++;  // +1 for previous code block's line number.

                const QModelIndex next_code_block_index = proxy_model->index(i, IsaItemModel::Columns::kLineNumber);

                // Add previous code block instruction count if previous code block is expanded.
                if (isExpanded(next_code_block_index))
                {
                    line_number += proxy_model->rowCount(next_code_block_index);
                }
            }

            line_number++;  // +1 for hot spot code block's line number.

            // Add the hot spot instruction's index if its code block is expanded.
            if (isExpanded(code_block_proxy_index))
            {
                line_number += instruction_proxy_index.row();
            }

            line_numbers.emplace(line_number);
        }
    }

    isa_scroll_bar_->SetHotSpotLineNumbers(line_numbers);
}

void IsaTreeView::SetSearchMatchLineNumbers(const QString search_text, const std::set<QModelIndex>& source_indices)
{
    auto* delegate = qobject_cast<IsaItemDelegate*>(itemDelegate());

    if (delegate != nullptr)
    {
        delegate->SetSearchText(search_text);
    }

    std::set<int> line_numbers;

    const QSortFilterProxyModel* proxy_model = qobject_cast<QSortFilterProxyModel*>(model());

    for (const auto& source_index : source_indices)
    {
        int line_number = 0;

        QModelIndex proxy_index        = source_index;
        QModelIndex proxy_index_parent = source_index.parent();

        // Get the relative line number of the requested search match index, accounting for the
        // expand/collapse state of previous code blocks.

        if (proxy_model != nullptr)
        {
            proxy_index = proxy_model->mapFromSource(proxy_index);

            int row = 0;

            if (proxy_index_parent.isValid())
            {
                // Search match matches an instruction.

                proxy_index_parent = proxy_model->mapFromSource(proxy_index_parent);

                row = proxy_index_parent.row();
            }
            else
            {
                // Search match matches a code block.

                row = proxy_index.row();
            }

            for (int i = 0; i < row; i++)
            {
                line_number++;  // +1 for previous code block's line number.

                const QModelIndex next_code_block_index = proxy_model->index(i, IsaItemModel::Columns::kLineNumber);

                // Add previous code block instruction count if previous code block is expanded.
                if (isExpanded(next_code_block_index))
                {
                    line_number += proxy_model->rowCount(next_code_block_index);
                }
            }

            line_number++;  // +1 for search match code block's line number.

            // If a search match matches an instruction (not a code block), and its parent code block is expanded,
            // add the instruction's index too.
            if (proxy_index_parent.isValid() && isExpanded(proxy_index_parent))
            {
                line_number += proxy_index.row();
            }

            line_numbers.emplace(line_number);
        }
    }

    isa_scroll_bar_->SetSearchMatchLineNumbers(line_numbers);
}

void IsaTreeView::ShowBranchInstructionsMenu(QVector<QModelIndex> source_indices, QPoint global_position)
{
    QMenu branch_instruction_menu(this);

    QMap<QString, QModelIndex> action_to_index_map;

    for (const auto& source_index : source_indices)
    {
        QString line_number        = source_index.siblingAtColumn(IsaItemModel::kLineNumber).data().toString();
        QString branch_instruction = source_index.siblingAtColumn(IsaItemModel::kOpCode).data().toString();
        QString menu_action_text   = line_number + ": " + branch_instruction;

        branch_instruction_menu.addAction(menu_action_text);

        action_to_index_map[menu_action_text] = source_index;
    }

    const QAction* result_action = branch_instruction_menu.exec(global_position);

    if (result_action != nullptr && action_to_index_map.find(result_action->text()) != action_to_index_map.end())
    {
        const QModelIndex& source_index = action_to_index_map.value(result_action->text());

        ScrollToIndex(source_index, true, true, true);
    }

    setCursor(Qt::ArrowCursor);
}

void IsaTreeView::ScrollToIndex(const QModelIndex source_index, bool record, bool select_row, bool notify_listener)
{
    const QSortFilterProxyModel* isa_tree_proxy_model = qobject_cast<const QSortFilterProxyModel*>(this->model());

    QModelIndex isa_tree_view_index = source_index;

    if (isa_tree_proxy_model != nullptr)
    {
        isa_tree_view_index = isa_tree_proxy_model->mapFromSource(isa_tree_view_index);
    }

    isa_tree_view_index = isa_tree_view_index.siblingAtColumn(IsaItemModel::kLineNumber);

    scrollTo(isa_tree_view_index, QAbstractItemView::ScrollHint::PositionAtCenter);

    if (select_row)
    {
        selectionModel()->setCurrentIndex(isa_tree_view_index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }

    // Make sure the custom row background color repaints.
    viewport()->update();

    if (record)
    {
        emit ScrolledToBranchOrLabel(source_index);
    }

    if (notify_listener)
    {
        emit ScrolledToIndex(source_index);
    }
}

void IsaTreeView::HideTooltip() const
{
    const IsaItemDelegate* isa_delegate = qobject_cast<IsaItemDelegate*>(itemDelegate());

    if (isa_delegate != nullptr)
    {
        isa_delegate->HideTooltip();
    }
}

void IsaTreeView::ReplayBranchOrLabelSelection(QModelIndex branch_label_source_index)
{
    ScrollToIndex(branch_label_source_index, false, true, true);
}

void IsaTreeView::drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const auto row_height = option.rect.height();

    if (row_height == 0)
    {
        // If the row height is zero we wouldn't paint anything anyway.
        return;
    }

    // Manually paint a gray background color for every other row.

    const int  rows_y_coordinate = option.rect.y() / row_height;
    const bool even_row          = (rows_y_coordinate % 2) == 0;  // Assume first column / line number column.

    if (even_row)
    {
        const QColor background_row_color = QtCommon::QtUtils::ColorTheme::Get().GetCurrentThemeColors().isa_background_row_color;

        painter->fillRect(option.rect, background_row_color);
    }

    // Paint the column separators.
    if (paint_column_separators_)
    {
        const QAbstractItemModel* proxy_model = index.model();

        if (proxy_model != nullptr)
        {
            int column_x_pos = -horizontalScrollBar()->value();
            for (int i = 0; i < proxy_model->columnCount(); i++)
            {
                QRect index_rect   = option.rect;
                int   column_width = header()->sectionSize(header()->logicalIndex(i));

                index_rect.setX(column_x_pos);
                index_rect.setWidth(column_width);

                painter->save();
                auto pen = painter->pen();
                pen.setColor(QtCommon::QtUtils::ColorTheme::Get().GetCurrentThemeColors().column_separator_color);
                painter->setPen(pen);
                painter->drawLine(index_rect.topRight(), index_rect.bottomRight());
                painter->restore();

                column_x_pos += column_width;
            }
        }
    }

    // Paint the rest of the rows contents on top of the background.
    QTreeView::drawRow(painter, option, index);
}

void IsaTreeView::keyPressEvent(QKeyEvent* event)
{
    bool event_handled = false;

    // The keyboard modifiers are different for PC/Linux vs macOS
#ifdef Q_OS_MACOS
    if (event->key() == Qt::Key_C && (event->modifiers() & Qt::MetaModifier))
#else
    if (event->key() == Qt::Key_C && (event->modifiers() & Qt::ControlModifier))
#endif  // Q_OS_MACOS
    {
        CopyRowsToClipboard();

        event->accept();
        event_handled = true;
    }

    SetAutoScrollObject set_auto_scroll_object(this);

    if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down || event->key() == Qt::Key_PageUp || event->key() == Qt::Key_PageDown)
    {
        // Temporarily enable autoScroll so that relevant keybinds will scroll the view after moving the selected index.
        set_auto_scroll_object.EnableAutoScroll();
    }

    // The isa_widget will handle Ctrl+Right/Ctrl+Left to expand/collapse code blocks, so ignore the event and pass it back up to the widget.
    if ((event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) && (event->modifiers() & Qt::ControlModifier))
    {
        event->ignore();
        event_handled = true;
    }

    if (!event_handled)
    {
        QTreeView::keyPressEvent(event);
    }
}

void IsaTreeView::ToggleCopyLineNumbers()
{
    copy_line_numbers_ = !copy_line_numbers_;
}

void IsaTreeView::CopyRowsToClipboard()
{
    const QItemSelectionModel* selection_model = selectionModel();

    // Copy/Paste.
    if (selection_model != nullptr && selection_model->hasSelection())
    {
        QString clipboard_text = "";

        QModelIndexList selection = selection_model->selectedIndexes();

        std::vector<CompareIndexInfo> view_sorted_selection;

        std::map<int, int> column_max_widths;

        for (const QModelIndex& index : selection)
        {
            CompareIndexInfo compare_index_info;
            compare_index_info.source_index = index;

            IsaProxyModel* proxy_model = qobject_cast<IsaProxyModel*>(model());
            if (proxy_model != nullptr)
            {
                compare_index_info.source_index = proxy_model->mapToSource(index);
            }

            const int column_index = index.column();

            compare_index_info.visual_column = header()->visualIndex(column_index);
            compare_index_info.y_pos         = visualRect(index).y();

            // Check whether line numbers should be included.
            if (column_index == IsaItemModel::kLineNumber && !copy_line_numbers_)
            {
                continue;
            }

            view_sorted_selection.push_back(compare_index_info);

            // Ignore the column widths of comments and code blocks.
            if (column_index != IsaItemModel::kLineNumber && isFirstColumnSpanned(index.row(), index.parent()))
            {
                continue;
            }

            QString text = index.data(Qt::DisplayRole).toString();

            // Indent opcodes.
            if (compare_index_info.source_index.column() == IsaItemModel::kOpCode && compare_index_info.source_index.parent().isValid() &&
                compare_index_info.source_index.data(IsaItemModel::kRowTypeRole).value<IsaItemModel::RowType>() == IsaItemModel::RowType::kCode)
            {
                text = QString("    " + text);
            }

            const int text_length = text.length();

            if (column_max_widths.find(compare_index_info.visual_column) != column_max_widths.end())
            {
                const int old_max_width = column_max_widths.at(compare_index_info.visual_column);

                if (text_length > old_max_width)
                {
                    column_max_widths[compare_index_info.visual_column] = text_length;
                }
            }
            else
            {
                column_max_widths[compare_index_info.visual_column] = text_length;
            }
        }

        // The selected items returned by the selection item model are not guaranteed to be sorted; sort them so
        // they are pasted in the same order that they appear on screen.
        std::sort(view_sorted_selection.begin(), view_sorted_selection.end(), CompareModelIndices);

        int y_pos = view_sorted_selection.front().y_pos;

        for (const CompareIndexInfo& compare_index_info : view_sorted_selection)
        {
            if (compare_index_info.y_pos > y_pos)
            {
                clipboard_text.append("\n");
                y_pos = compare_index_info.y_pos;
            }

            QString text = compare_index_info.source_index.data(Qt::DisplayRole).toString();

            // Indent opcodes.
            if (compare_index_info.source_index.column() == IsaItemModel::kOpCode && compare_index_info.source_index.parent().isValid() &&
                compare_index_info.source_index.data(IsaItemModel::kRowTypeRole).value<IsaItemModel::RowType>() == IsaItemModel::RowType::kCode)
            {
                text = QString("    " + text);
            }

            QString formatted_text = QString("%1\t").arg(text, -column_max_widths[compare_index_info.visual_column]);

            clipboard_text.append(formatted_text);
            clipboard_text.append(" ");
        }

        if (!clipboard_text.isEmpty())
        {
            QClipboard* clipboard = QApplication::clipboard();
            clipboard->setText(clipboard_text);
        }
    }
}

void IsaTreeView::IndexExpandedOrCollapsed(const QModelIndex index)
{
    Q_UNUSED(index);

    // Force hide the tooltip if any block is expanded or collapsed.
    const IsaItemDelegate* isa_delegate = qobject_cast<IsaItemDelegate*>(itemDelegate());

    if (isa_delegate != nullptr)
    {
        isa_delegate->HideTooltip();
    }
}

void IsaTreeView::ScrollBarScrolled(int value)
{
    Q_UNUSED(value);

    QAbstractItemModel* model = this->model();

    if (nullptr != model)
    {
        const QModelIndex top_left = indexAt(QPoint(0, 0));

        const QModelIndex last_pinned_parent_index = model->index(last_pinned_row_.first, 0, QModelIndex());
        const QModelIndex last_pinned_index        = model->index(last_pinned_row_.second, 0, last_pinned_parent_index);

        if (last_pinned_index.isValid() && last_pinned_index.model() == model)
        {
            setFirstColumnSpanned(last_pinned_index.row(), last_pinned_index.parent(), false);
        }
        if (!isFirstColumnSpanned(top_left.row(), top_left.parent()))
        {
            setFirstColumnSpanned(top_left.row(), top_left.parent(), true);
            last_pinned_row_ = std::pair<int, int>(top_left.parent().row(), top_left.row());
        }
        else
        {
            ClearLastPinnedndex();
        }
    }

    // Notify the viewport to refresh.
    viewport()->update();
}
