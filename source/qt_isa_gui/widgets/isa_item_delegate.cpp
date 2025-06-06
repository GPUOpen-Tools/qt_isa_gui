//=============================================================================
// Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for an isa item delegate.
//=============================================================================

#include "isa_item_delegate.h"

#include <QColor>
#include <QEvent>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QPointF>
#include <QSortFilterProxyModel>

#include "qt_common/utils/common_definitions.h"
#include "qt_common/utils/qt_util.h"

#include "qt_isa_gui/utility/isa_dictionary.h"

#include "isa_item_model.h"
#include "isa_tree_view.h"

/// @brief Paint a token's text using a color based on its type or syntax.
///
/// @param [in] token                The token to paint.
/// @param [in] token_rectangle      The rectangle to paint to.
/// @param [in] painter              The painter.
/// @param [in] color_coding_enabled True to apply a color coding to the token, false otherwise.
static void PaintTokenText(const IsaItemModel::Token& token, const QRectF& token_rectangle, QPainter* painter, const bool color_coding_enabled)
{
    if (color_coding_enabled)
    {
        QColor color;
        auto   pen = painter->pen();

        if (token.type == IsaItemModel::TokenType::kBranchLabelType)
        {
            QColor operand_color;
            if (QtCommon::QtUtils::ColorTheme::Get().GetColorTheme() == kColorThemeTypeLight)
            {
                operand_color = kIsaLightThemeColorDarkMagenta;
            }
            else
            {
                operand_color = kIsaDarkThemeColorDarkMagenta;
            }

            // Operand that is the target of a branch instruction.
            color = operand_color;
        }
        else if (!IsaColorCodingDictionaryInstance::GetInstance().ShouldHighlight(token.token_text, color))
        {
            color = pen.color();
        }

        pen.setColor(color);
        painter->setPen(pen);
    }

    painter->drawText(token_rectangle, Qt::TextSingleLine, token.token_text.c_str());
}

/// @brief Paint a comma.
///
/// @param [in] comma_rectangle      The rectangle to paint to.
/// @param [in] painter              The painter.
static void PaintCommaText(const QRectF& comma_rectangle, QPainter* painter)
{
    painter->drawText(comma_rectangle, ",");
}

IsaItemDelegate::IsaItemDelegate(IsaTreeView* view, QObject* parent)
    : QStyledItemDelegate(parent)
    , view_(view)
    , mouse_over_code_block_index_(-1)
    , mouse_over_instruction_index_(-1)
    , mouse_over_token_index_(-1)
    , tooltip_(nullptr)
{
    tooltip_ = new IsaTooltip(view, view->viewport());

    // Force hide the tooltip if the tree view is scrolled.
    connect(view_->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this]() { tooltip_->hide(); });
    connect(view_->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() { tooltip_->hide(); });
}

IsaItemDelegate::~IsaItemDelegate()
{
}

void IsaItemDelegate::RegisterScrollAreas(std::vector<QScrollArea*> container_scroll_areas)
{
    tooltip_->RegisterScrollAreas(container_scroll_areas);
}

bool IsaItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
{
    Q_UNUSED(option);

    // Normal bounds checking.
    if (!index.isValid())
    {
        return true;
    }

    const auto* proxy_model  = qobject_cast<QSortFilterProxyModel*>(model);
    QModelIndex source_index = (proxy_model != nullptr) ? proxy_model->mapToSource(index) : index;

    switch (event->type())
    {
    case QEvent::MouseButtonRelease:
    {
        const QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);

        if (mouse_event != nullptr && mouse_event->button() == Qt::LeftButton)
        {
            const qreal offset = view_->header()->sectionPosition(index.column()) - view_->horizontalScrollBar()->value();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            const qreal view_x_position = mouse_event->pos().x();
#else
            const qreal view_x_position = mouse_event->position().x();
#endif

            qreal  local_x_position        = view_x_position - offset;
            int    token_under_mouse_index = -1;
            QRectF isa_token_global_hit_box;

            // Make sure the x position accounts for columns that span across the view.
            AdjustXPositionForSpannedColumns(index, proxy_model, source_index, local_x_position);

            // Determine if there is a token that is selectable underneath the mouse.
            SetSelectableTokenUnderMouse(source_index, index, local_x_position, selected_isa_token_, token_under_mouse_index, offset, isa_token_global_hit_box);

            // Determine if there is a branch label token underneath the mouse.
            const bool label_clicked = SetBranchLabelTokenUnderMouse(source_index, local_x_position);

            if (label_clicked)
            {
                QVector<QModelIndex> branch_label_indices = qvariant_cast<QVector<QModelIndex>>(source_index.data(IsaItemModel::kBranchIndexRole));

                if (source_index.column() == IsaItemModel::kOpCode)
                {
                    if (branch_label_indices.size() > 1)
                    {
                        // Label is referenced by more than 1 branch instruction so show a popup menu with all of the branch instructions.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                        view_->ShowBranchInstructionsMenu(branch_label_indices, mouse_event->globalPos());
#else
                        view_->ShowBranchInstructionsMenu(branch_label_indices, mouse_event->globalPosition().toPoint());
#endif
                    }
                    else if (!branch_label_indices.isEmpty())
                    {
                        QModelIndex source_branch_label_index = branch_label_indices.front();

                        // Label is only referenced by 1 branch instruction to just scroll to it right away.
                        view_->ScrollToIndex(source_branch_label_index, true, true, true);
                    }

                    // Stop processing the mouse release and scroll to the 1st corresponding label instead.
                    return true;
                }
                else if (source_index.column() == IsaItemModel::kOperands && !branch_label_indices.isEmpty())
                {
                    QModelIndex source_branch_label_index = branch_label_indices.front();

                    // Stop processing the mouse release and scroll to the 1st corresponding label instead.
                    view_->ScrollToIndex(source_branch_label_index, true, true, true);
                    return true;
                }
            }

            // Tell any attached views to refresh everything.
            model->dataChanged(QModelIndex(), QModelIndex());
        }

        break;
    }
    case QEvent::MouseMove:
    {
        mouse_over_code_block_index_  = -1;
        mouse_over_instruction_index_ = -1;
        mouse_over_token_index_       = -1;

        const QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
        const qreal        offset      = view_->header()->sectionPosition(index.column()) - view_->horizontalScrollBar()->value();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        const qreal view_x_position = mouse_event->pos().x();
#else

        const qreal view_x_position = mouse_event->position().x();
#endif

        qreal local_x_position = view_x_position - offset;

        QRectF isa_token_global_hit_box;

        // Make sure the x position accounts for columns that span across the view.
        AdjustXPositionForSpannedColumns(index, proxy_model, source_index, local_x_position);

        // Determine if there is a token that is selectable underneath the mouse.
        const bool mouse_over_isa_token = SetSelectableTokenUnderMouse(
            source_index, index, local_x_position, mouse_over_isa_token_, mouse_over_token_index_, offset, isa_token_global_hit_box);

        if (mouse_over_isa_token)
        {
            mouse_over_code_block_index_  = source_index.parent().row();
            mouse_over_instruction_index_ = source_index.row();
        }

        // Determine if there is a branch label token underneath the mouse.
        SetBranchLabelTokenUnderMouse(source_index, local_x_position);

        // Tell any attached views to refresh everything.
        model->dataChanged(QModelIndex(), QModelIndex());

        // Immediately hide the tooltip if the index that the mouse is over is different from the last index the timer started at.
        if (source_index != tooltip_timeout_source_index_)
        {
            tooltip_timer_.stop();

            tooltip_->hide();

            tooltip_timeout_source_index_ = QModelIndex();
        }

        // Show, hide, or don't touch the tooltip depending on what token is underneath the mouse.
        if (mouse_over_isa_token && source_index.column() == IsaItemModel::kOpCode)
        {
            //  The mouse collided with an op code token.

            if ((!tooltip_->isVisible() || source_index != tooltip_timeout_source_index_) && !tooltip_timer_.isActive())
            {
                // The tooltip isn't visible yet, or it's visible but the index under the mouse has changed, and the timer isn't active.
                // Save the op code token's hit box, save the index the mouse is at, and start a timer for the tooltip.

                tooltip_timeout_source_index_  = source_index;
                tooltip_timeout_token_hit_box_ = isa_token_global_hit_box;

                tooltip_timer_.start(IsaTooltip::kTooltipDelayMs);
            }
        }
        else
        {
            // The mouse is not over a valid op code token. Invalidate the index that tracks the tooltip's last position.
            // Also immediately stop the timer and hide the tooltip.

            tooltip_timer_.stop();

            tooltip_timeout_source_index_ = QModelIndex();

            tooltip_->hide();
        }

        break;
    }
    default:
    {
        break;
    }
    }

    return false;
}

void IsaItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& model_index) const
{
    // Bounds checking.
    if (!model_index.isValid())
    {
        return;
    }

    const IsaProxyModel* proxy_model  = qobject_cast<const IsaProxyModel*>(model_index.model());
    const IsaItemModel*  source_model = nullptr;
    QModelIndex          source_model_index;

    // Get the source model and index.
    if (proxy_model == nullptr)
    {
        source_model_index = model_index;
        source_model       = qobject_cast<const IsaItemModel*>(model_index.model());
    }
    else
    {
        source_model_index = proxy_model->mapToSource(model_index);
        source_model       = qobject_cast<const IsaItemModel*>(proxy_model->sourceModel());
    }

    if (source_model == nullptr)
    {
        return;
    }

    const auto row_type                  = qvariant_cast<IsaItemModel::RowType>(source_model_index.data(IsaItemModel::UserRoles::kRowTypeRole));
    const bool is_comment                = row_type == IsaItemModel::RowType::kComment;
    int        proxy_index_y_position    = -1;
    const bool block_label_pinned_to_top = BlockLabelPinnedToTop(source_model_index, model_index, proxy_index_y_position);

    QStyleOptionViewItem initialized_option = option;
    initStyleOption(&initialized_option, source_model_index);

    painter->save();
    painter->setFont(initialized_option.font);

    // Pin instructions' block labels to the top of the screen and paint them instead of painting an instruction.
    if (block_label_pinned_to_top)
    {
        // This index is part of a child row and is at the top of its tree's viewport.
        // Instead of painting the child row normally, paint its parent label, as if it is pinned to the top of the viewport.
        // The pinned parent label should span across columns.

        const auto parent_op_code_source = source_model_index.parent().siblingAtColumn(IsaItemModel::kOpCode);
        const auto parent_op_code_proxy  = (proxy_model != nullptr) ? proxy_model->mapFromSource(parent_op_code_source) : parent_op_code_source;
        const auto x_position            = GetColumnSpanStartPosition(is_comment, parent_op_code_proxy);

        PaintSpanned(painter, initialized_option, parent_op_code_source, x_position);

        painter->restore();
        return;
    }

    // If this row is selected or moused-over, render a highlight.
    if ((((initialized_option.state & QStyle::State_Selected) != 0) || ((initialized_option.state & QStyle::State_MouseOver) != 0)))
    {
        initialized_option.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &initialized_option, painter, initialized_option.widget);
    }

    // Don't try to paint any columns not defined in the isa model.
    if (source_model_index.column() >= IsaItemModel::kColumnCount)
    {
        painter->restore();
        return;
    }

    QRectF paint_rectangle = initialized_option.rect;

    // Advance the starting position of the text by a predefined indent for child instruction op codes not pinned to the top of the view.
    if ((source_model_index.column() == IsaItemModel::kOpCode) && (row_type != IsaItemModel::RowType::kComment) && (source_model_index.parent().isValid()) &&
        (proxy_index_y_position != 0))
    {
        paint_rectangle.setX(paint_rectangle.x() + initialized_option.fontMetrics.horizontalAdvance(IsaItemModel::kOpCodeColumnIndent));
    }

    // Paint a highlight rectangle for any text search matches in columns defined in the isa model, except the line number column.
    bool    paint_highlight = false;
    QString display_role_text_for_highlight;

    if (view_->isFirstColumnSpanned(model_index.row(), model_index.parent()))
    {
        // Highlight comments and labels which also span columns.

        paint_highlight = true;

        // Comments and labels are stored in the op code column so get them from the op code column's display role.
        const auto op_code_source_index = source_model_index.siblingAtColumn(IsaItemModel::kOpCode);
        display_role_text_for_highlight = op_code_source_index.data(Qt::DisplayRole).toString();
        const auto op_code_proxy_index  = (proxy_model != nullptr) ? proxy_model->mapFromSource(op_code_source_index) : op_code_source_index;
        const auto x_position           = GetColumnSpanStartPosition(is_comment, op_code_proxy_index);

        paint_rectangle.setX(x_position);
    }
    else if ((source_model_index.column() == IsaItemModel::kOpCode) || (source_model_index.column() == IsaItemModel::kOperands) ||
             (source_model_index.column() == IsaItemModel::kPcAddress) || source_model_index.column() == IsaItemModel::kBinaryRepresentation)
    {
        // Highlighting op codes, operands, addresses and binary representation.

        paint_highlight = true;

        // Get their concatenated text from their own display roles.
        display_role_text_for_highlight = source_model_index.data(Qt::DisplayRole).toString();
    }

    if (paint_highlight)
    {
        PaintSearchHighlight(painter, paint_rectangle, display_role_text_for_highlight, source_model->GetFixedFontCharacterWidth(), source_model_index);
    }

    // Get a default text color if applicable.
    QVariant color_data = source_model_index.data(Qt::ForegroundRole);
    if (color_data != QVariant())
    {
        auto pen = painter->pen();
        pen.setColor(color_data.value<QColor>());
        painter->setPen(pen);
    }

    // Custom paint all columns defined in the isa model.
    if (source_model_index.column() == IsaItemModel::kLineNumber)
    {
        // Use the line number column to paint line numbers and any rows that span across columns.
        // Comments and block labels should span across columns.

        if (!block_label_pinned_to_top && source_model->LineNumbersVisible())
        {
            // Paint line #s if they aren't turned off and if this isn't a pinned block label.

            const auto line_number_text = source_model_index.data(Qt::DisplayRole).toString() + IsaItemModel::kColumnPadding;
            QRectF     line_number_rect = initialized_option.rect;

            // Right align the line number to its column.
            const int line_number_column_width = view_->header()->sectionSize(view_->header()->logicalIndex(0));
            const int line_number_text_width   = initialized_option.fontMetrics.horizontalAdvance(line_number_text);
            const int scroll_bar_position      = view_->horizontalScrollBar()->value();
            const int line_number_x_position   = line_number_column_width - line_number_text_width - scroll_bar_position;

            line_number_rect.setX(line_number_x_position);
            line_number_rect.setWidth(initialized_option.fontMetrics.horizontalAdvance(line_number_text));

            painter->drawText(line_number_rect, Qt::Alignment(Qt::AlignLeft | Qt::AlignTop), line_number_text);
        }

        const auto op_code_source_index = source_model_index.siblingAtColumn(IsaItemModel::kOpCode);

        if (!source_model_index.parent().isValid())
        {
            // Paint all parent block labels across columns.

            PaintSpanned(painter, initialized_option, op_code_source_index, paint_rectangle.x());
        }
        else if (row_type == IsaItemModel::RowType::kComment)
        {
            // Paint child comments across columns.

            PaintSpanned(painter, initialized_option, op_code_source_index, paint_rectangle.x());
        }
    }
    else if ((source_model_index.column() == IsaItemModel::kOpCode) && source_model_index.parent().isValid() && (proxy_index_y_position != 0) &&
             (row_type != IsaItemModel::RowType::kComment))
    {
        // Child instruction in the op code column that is not at the top of its tree's viewport.
        // Paint color coded op codes.

        auto font = painter->font();
        font.setBold(true);
        painter->setFont(font);

        const std::vector<IsaItemModel::Token> op_code_token = qvariant_cast<std::vector<IsaItemModel::Token>>(source_model_index.data(Qt::UserRole));

        PaintText(painter, initialized_option, source_model_index, paint_rectangle, op_code_token, 0, false);
    }
    else if ((source_model_index.column() == IsaItemModel::kOperands) && source_model_index.parent().isValid() && (proxy_index_y_position != 0) &&
             (row_type != IsaItemModel::RowType::kComment))
    {
        // Child instruction in the operands column that is not at the top of its tree's viewport.
        // Paint color coded operands.

        auto font = painter->font();
        font.setBold(true);
        painter->setFont(font);

        std::vector<std::vector<IsaItemModel::Token>> tokens =
            qvariant_cast<std::vector<std::vector<IsaItemModel::Token>>>(source_model_index.data(Qt::UserRole));

        std::pair<int, QRectF> token_info = std::pair<int, QRectF>(0, paint_rectangle);

        // Break the operands down into their individual tokens, and paint them token by token.
        for (size_t i = 0; i < tokens.size(); i++)
        {
            const auto& operand_tokens = tokens.at(i);

            token_info = PaintText(painter, initialized_option, source_model_index, token_info.second, operand_tokens, token_info.first, false);

            // Add a comma if it is not the last operand.
            if (i < tokens.size() - 1)
            {
                PaintCommaText(token_info.second, painter);

                token_info.second.adjust(initialized_option.fontMetrics.horizontalAdvance(QString(IsaItemModel::kOperandDelimiter)), 0, 0, 0);
            }
        }
    }
    else if (source_model_index.column() == IsaItemModel::kPcAddress || source_model_index.column() == IsaItemModel::kBinaryRepresentation)
    {
        // Paint pc address and binary representation as plain text.

        painter->drawText(paint_rectangle, initialized_option.displayAlignment, source_model_index.data(Qt::DisplayRole).toString());
    }

    painter->restore();
}

QSize IsaItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QSize size_hint(0, 0);

    const QAbstractProxyModel* proxy_model  = qobject_cast<const QAbstractProxyModel*>(index.model());
    const IsaItemModel*        source_model = nullptr;

    if (proxy_model == nullptr)
    {
        source_model = qobject_cast<const IsaItemModel*>(index.model());
    }
    else
    {
        source_model = qobject_cast<const IsaItemModel*>(proxy_model->sourceModel());
    }

    if (source_model == nullptr)
    {
        return size_hint;
    }

    QModelIndex source_model_index;

    if (proxy_model == nullptr)
    {
        // No proxy being used.
        source_model_index = index;
    }
    else
    {
        // Proxy being used.
        source_model_index = proxy_model->mapToSource(index);
    }

    if (source_model_index.column() >= IsaItemModel::kColumnCount)
    {
        return QStyledItemDelegate::sizeHint(option, index);
    }

    return source_model->ColumnSizeHint(source_model_index.column(), view_);
}

void IsaItemDelegate::ConnectTooltipTimerCallback(bool connect_timer)
{
    if (connect_timer)
    {
        connect(&tooltip_timer_, &QTimer::timeout, this, &IsaItemDelegate::TooltipTimerCallback);
    }
    else
    {
        disconnect(&tooltip_timer_, &QTimer::timeout, this, &IsaItemDelegate::TooltipTimerCallback);
    }
}

void IsaItemDelegate::HideTooltip() const
{
    tooltip_->hide();
}

bool IsaItemDelegate::BlockLabelPinnedToTop(const QModelIndex& source_model_index, const QModelIndex& proxy_model_index, int& proxy_index_y_position) const
{
    const QRect proxy_index_visual_rect = view_->visualRect(proxy_model_index);
    proxy_index_y_position              = proxy_index_visual_rect.y();

    if (source_model_index.parent().isValid() && proxy_index_y_position == 0)
    {
        return true;
    }

    return false;
}

void IsaItemDelegate::PaintSearchHighlight(QPainter*         painter,
                                           const QRectF&     rectangle,
                                           const QString     display_role_text,
                                           const qreal       fixed_font_character_width,
                                           const QModelIndex source_index) const
{
    if (search_text_.isEmpty())
    {
        return;
    }

    const auto sibling_line_number_source_index = source_index.sibling(source_index.row(), IsaItemModel::kLineNumber);

    // Text length and highlight rectangle width.
    const qreal search_text_length        = search_text_.length();
    const qreal highlight_rectangle_width = fixed_font_character_width * search_text_length;

    // Font and metrics.
    const auto font         = painter->font();
    const auto font_metrics = QFontMetricsF(font);

    // Colors.
    const QColor isa_search_match_color = QtCommon::QtUtils::ColorTheme::Get().GetCurrentThemeColors().isa_search_match_row_color;
    const QColor selection_color        = QtCommon::QtUtils::ColorTheme::Get().GetCurrentPalette().color(QPalette::Highlight);
    QColor       search_match_color;

    // Offsets.
    qsizetype search_text_match_index = 0;
    QRectF    highlight_rectangle     = rectangle;

    // Paint a highlight rectangle over every text search match.
    while ((search_text_match_index = display_role_text.indexOf(search_text_, search_text_match_index, Qt::CaseInsensitive)) != -1)
    {
        const qreal text_start = (fixed_font_character_width * static_cast<qreal>(search_text_match_index)) + rectangle.x();

        highlight_rectangle.setX(text_start);
        highlight_rectangle.setWidth(highlight_rectangle_width);

        // Use the palette's selection color if this index belongs to the current search row, otherwise use the isa search color.
        search_match_color =
            (sibling_line_number_source_index.isValid() && search_source_index_.isValid() && (sibling_line_number_source_index == search_source_index_))
                ? selection_color
                : isa_search_match_color;

        painter->fillRect(highlight_rectangle, search_match_color);

        search_text_match_index = search_text_match_index + search_text_length;
    }
}

void IsaItemDelegate::TooltipTimerCallback()
{
    // Stop the timer so that it only starts again if there is another valid isa token collision.
    tooltip_timer_.stop();

    // Check if the current position of the mouse is within a delta of what the position was when the timer started.
    const auto current_mouse_global_position = QCursor::pos();

    if (tooltip_timeout_token_hit_box_.contains(current_mouse_global_position))
    {
        // The current position of the mouse is close enough, so show the tooltip.

        const QVariant data = tooltip_timeout_source_index_.data(IsaItemModel::UserRoles::kDecodedIsa);
        if (data.isValid())
        {
            const auto decoded_info = qvariant_cast<amdisa::InstructionInfo>(data);

            tooltip_->UpdateText(decoded_info);

            tooltip_->UpdatePosition(current_mouse_global_position);

            tooltip_->show();
        }
    }
    else
    {
        tooltip_timeout_source_index_ = QModelIndex();
    }
}

void IsaItemDelegate::AdjustXPositionForSpannedColumns(const QModelIndex&           index,
                                                       const QSortFilterProxyModel* proxy,
                                                       QModelIndex&                 source_index,
                                                       qreal&                       local_x_position)
{
    if (view_->isFirstColumnSpanned(index.row(), index.parent()))
    {
        int opcode_index = IsaItemModel::kOpCode;

        if (proxy != nullptr)
        {
            opcode_index = proxy->mapFromSource(source_index.siblingAtColumn(IsaItemModel::kOpCode)).column();
        }

        if (opcode_index != -1 && local_x_position > view_->header()->sectionPosition(opcode_index))
        {
            int next_index = view_->header()->logicalIndex(view_->header()->visualIndex(opcode_index) + 1);

            if (next_index == -1 || local_x_position < view_->header()->sectionPosition(next_index))
            {
                source_index = source_index.siblingAtColumn(IsaItemModel::kOpCode);
                local_x_position -= view_->header()->sectionPosition(opcode_index);
            }
        }
    }
}

int IsaItemDelegate::GetColumnSpanStartPosition(const bool is_comment, const QModelIndex proxy_index) const
{
    int x_position = 0;

    if (is_comment || !proxy_index.isValid())
    {
        // When paintint spanning text for a comment or for a label while the op code column is not visible,
        // start painting right after the line # column.
        x_position = view_->header()->sectionPosition(view_->header()->logicalIndex(1));
    }
    else
    {
        // When painting spanning text for a label while the op code column is visible,
        // start painting at the op code column.
        x_position = view_->header()->sectionPosition(proxy_index.column());
    }

    return x_position;
}

bool IsaItemDelegate::SetSelectableTokenUnderMouse(const QModelIndex&   source_index,
                                                   const QModelIndex&   proxy_index,
                                                   const int            local_x_position,
                                                   IsaItemModel::Token& isa_token_under_mouse,
                                                   int&                 isa_token_under_mouse_index,
                                                   const int            offset,
                                                   QRectF&              isa_token_hit_box)
{
    isa_token_under_mouse.Clear();
    isa_token_under_mouse_index = -1;

    // Check if the index is the op code or operands column, because only those columns store and display tokens.
    if (source_index.column() != IsaItemModel::Columns::kOpCode && source_index.column() != IsaItemModel::Columns::kOperands)
    {
        return false;
    }

    // Check if the index is a block label pinned to the top of the view, because we don't show tokens if it is pinned.
    int proxy_index_y_position = -1;

    if (BlockLabelPinnedToTop(source_index, proxy_index, proxy_index_y_position))
    {
        return false;
    }

    // Get the tokens at the index.
    std::vector<IsaItemModel::Token> tokens;

    if (source_index.column() == IsaItemModel::Columns::kOpCode)
    {
        tokens = qvariant_cast<std::vector<IsaItemModel::Token>>(source_index.data(Qt::UserRole));
    }
    else
    {
        const std::vector<std::vector<IsaItemModel::Token>> tokens_per_operands =
            qvariant_cast<std::vector<std::vector<IsaItemModel::Token>>>(source_index.data(Qt::UserRole));

        for (const auto& tokens_per_operand : tokens_per_operands)
        {
            tokens.insert(tokens.end(), tokens_per_operand.begin(), tokens_per_operand.end());
        }
    }

    // Check if the mouse position is directly over any token.
    for (int i = 0; i < static_cast<int>(tokens.size()); i++)
    {
        const auto& isa_token = tokens.at(i);

        if (isa_token.is_selectable && local_x_position >= isa_token.x_position_start && local_x_position <= isa_token.x_position_end)
        {
            // Save the isa token.
            isa_token_under_mouse_index = i;
            isa_token_under_mouse       = isa_token;

            // Save the token's hit box in global coordinates.
            const auto token_left   = view_->mapToGlobal(QPoint(offset + isa_token.x_position_start, 0)).x();
            const auto token_right  = view_->mapToGlobal(QPoint(offset + isa_token.x_position_end, 0)).x();
            const auto token_top    = view_->mapToGlobal(QPoint(0, view_->visualRect(proxy_index).y())).y() + view_->header()->height();
            const auto token_height = view_->visualRect(proxy_index).height();

            const auto top_left  = QPointF(token_left, token_top);
            const auto bot_right = QPointF(token_right, token_top + token_height);

            isa_token_hit_box = QRectF(top_left, bot_right);

            return true;
        }
    }

    return false;
}

bool IsaItemDelegate::SetBranchLabelTokenUnderMouse(const QModelIndex& source_index, const int local_x_position)
{
    bool hover_over_label = false;

    if (source_index.column() == IsaItemModel::kOpCode)
    {
        const bool is_mouse_over_branch_label = source_index.data(IsaItemModel::kLabelBranchRole).toBool();

        if (is_mouse_over_branch_label)
        {
            // Label is referenced by branch instructions.

            std::vector<IsaItemModel::Token> tokens = qvariant_cast<std::vector<IsaItemModel::Token>>(source_index.data(Qt::UserRole));

            IsaItemModel::Token token;

            if (!tokens.empty())
            {
                token = tokens.front();
            }

            if (token.type == IsaItemModel::TokenType::kLabelType && local_x_position >= token.x_position_start && local_x_position <= token.x_position_end)
            {
                hover_over_label = true;

                view_->setCursor(Qt::PointingHandCursor);
            }
        }
    }
    else if (source_index.column() == IsaItemModel::kOperands)
    {
        std::vector<std::vector<IsaItemModel::Token>> tokens = qvariant_cast<std::vector<std::vector<IsaItemModel::Token>>>(source_index.data(Qt::UserRole));

        IsaItemModel::Token token;

        if (!tokens.empty() && !tokens.front().empty())
        {
            token = tokens.front().front();
        }

        if (token.type == IsaItemModel::TokenType::kBranchLabelType && local_x_position >= token.x_position_start && local_x_position <= token.x_position_end)
        {
            hover_over_label = true;

            view_->setCursor(Qt::PointingHandCursor);
        }
    }

    if (!hover_over_label)
    {
        view_->setCursor(Qt::ArrowCursor);
    }

    return hover_over_label;
}

void IsaItemDelegate::PaintTokenHighlight(const IsaItemModel::Token& token,
                                          const QRectF&              isa_token_rectangle,
                                          QPainter*                  painter,
                                          const QFontMetrics&        font_metrics,
                                          int                        code_block_index,
                                          int                        instruction_index,
                                          int                        token_index) const
{
    bool is_token_selected = false;

    if (token.type == IsaItemModel::TokenType::kScalarRegisterType || token.type == IsaItemModel::TokenType::kVectorRegisterType)
    {
        // Check if register numbers match.

        if (((token.type == IsaItemModel::TokenType::kScalarRegisterType && selected_isa_token_.type == IsaItemModel::TokenType::kScalarRegisterType) ||
             (token.type == IsaItemModel::TokenType::kVectorRegisterType && selected_isa_token_.type == IsaItemModel::TokenType::kVectorRegisterType)))
        {
            // Comparing scalar to scalar or vector to vector.

            if (selected_isa_token_.start_register_index != -1 && token.start_register_index != -1)
            {
                // End can be -1 but start should never be -1.

                if (selected_isa_token_.end_register_index == -1 && token.end_register_index != -1)
                {
                    // The selected/clicked token is a single register but the token we are checking is a range.

                    if (selected_isa_token_.start_register_index >= token.start_register_index &&
                        selected_isa_token_.start_register_index <= token.end_register_index)
                    {
                        is_token_selected = true;
                    }
                }
                else if (token.end_register_index == -1 && selected_isa_token_.end_register_index != -1)
                {
                    // The token we are checking is a single register but the selected/clicked is a range.

                    if (token.start_register_index >= selected_isa_token_.start_register_index &&
                        token.start_register_index <= selected_isa_token_.end_register_index)
                    {
                        is_token_selected = true;
                    }
                }
                else if (token.end_register_index == -1 && selected_isa_token_.end_register_index == -1)
                {
                    // Both tokens are a single register.

                    if (token.start_register_index == selected_isa_token_.start_register_index)
                    {
                        is_token_selected = true;
                    }
                }
                else if ((token.start_register_index >= selected_isa_token_.start_register_index &&
                          token.start_register_index <= selected_isa_token_.end_register_index) ||
                         (token.end_register_index >= selected_isa_token_.start_register_index &&
                          token.end_register_index <= selected_isa_token_.end_register_index))

                {
                    // Both tokens are ranges and the ranges overlap.

                    is_token_selected = true;
                }
            }
        }
    }
    else
    {
        // Op code or constant, string check should be sufficient.
        if (selected_isa_token_.token_text == token.token_text)
        {
            is_token_selected = true;
        }
    }

    // Use the same color for light and dark mode.
    const auto token_highlight_color = kIsaLightThemeColorLightPink;

    if (is_token_selected)
    {
        // This token matches the currently selected token so highlight it.

        QRectF highlighted_operand_rectangle = isa_token_rectangle;
        highlighted_operand_rectangle.setWidth(font_metrics.horizontalAdvance(token.token_text.c_str()));

        painter->fillRect(highlighted_operand_rectangle, token_highlight_color);
    }
    else if (mouse_over_isa_token_.token_text == token.token_text && code_block_index == mouse_over_code_block_index_ &&
             instruction_index == mouse_over_instruction_index_ && mouse_over_token_index_ == token_index)
    {
        // This token is underneath the mouse so highlight it.

        QRectF highlighted_token_rectangle = isa_token_rectangle;
        highlighted_token_rectangle.setWidth(font_metrics.horizontalAdvance(token.token_text.c_str()));

        painter->fillRect(highlighted_token_rectangle, token_highlight_color);
    }
}

void IsaItemDelegate::PaintSpanned(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& source_index, int x_position) const
{
    painter->save();

    auto font = source_index.data(Qt::FontRole).value<QFont>();
    font.setBold(true);
    painter->setFont(font);

    QVariant op_code_color_data = source_index.data(Qt::ForegroundRole);

    if (op_code_color_data != QVariant())
    {
        auto pen = painter->pen();
        pen.setColor(op_code_color_data.value<QColor>());
        painter->setPen(pen);
    }

    const std::vector<IsaItemModel::Token> tokens     = qvariant_cast<std::vector<IsaItemModel::Token>>(source_index.data(Qt::UserRole));
    const auto                             row_type   = qvariant_cast<IsaItemModel::RowType>(source_index.data(IsaItemModel::UserRoles::kRowTypeRole));
    const bool                             is_comment = row_type == IsaItemModel::RowType::kComment;

    x_position -= view_->horizontalScrollBar()->value();

    QRect text_rectangle = option.rect;
    text_rectangle.setX(x_position);
    text_rectangle.setWidth(view_->width() - text_rectangle.x());

    PaintText(painter, option, source_index, text_rectangle, tokens, 0, is_comment);

    painter->restore();
}

std::pair<int, QRectF> IsaItemDelegate::PaintText(QPainter*                        painter,
                                                  const QStyleOptionViewItem&      option,
                                                  const QModelIndex&               source_index,
                                                  QRectF                           token_rectangle,
                                                  std::vector<IsaItemModel::Token> tokens,
                                                  int                              token_index,
                                                  bool                             is_comment) const
{
    if (is_comment)
    {
        painter->drawText(token_rectangle, Qt::Alignment(Qt::AlignLeft | Qt::AlignTop), source_index.data(Qt::DisplayRole).toString());
    }
    else if (!source_index.parent().isValid())
    {
        if (!tokens.empty())
        {
            const bool color_coding_enabled = source_index.data(IsaItemModel::kLineEnabledRole).toBool();
            painter->save();
            PaintTokenText(tokens.front(), token_rectangle, painter, color_coding_enabled);
            painter->restore();
        }
    }
    else
    {
        for (size_t i = 0; i < tokens.size(); i++)
        {
            const auto token = tokens.at(i);

            if (token.is_selectable)
            {
                PaintTokenHighlight(token,
                                    token_rectangle,
                                    painter,
                                    option.fontMetrics,
                                    source_index.parent().row(),
                                    source_index.row(),
                                    token_index);  // Assume 0 index for op code.
            }

            const bool color_coding_enabled = source_index.data(IsaItemModel::kLineEnabledRole).toBool();

            painter->save();
            PaintTokenText(token, token_rectangle, painter, color_coding_enabled);
            if (token.type == IsaItemModel::TokenType::kBranchLabelType)
            {
                QPoint label_underline_start(token_rectangle.x() + token.x_position_start, token_rectangle.bottom());
                QPoint label_underline_end(token_rectangle.x() + token.x_position_end, token_rectangle.bottom());

                // Re-use color and draw a line underneath the target of the branch instruction.
                painter->drawLine(label_underline_start, label_underline_end);
            }
            painter->restore();
            const QString token_text(token.token_text.c_str());
            token_rectangle.adjust(option.fontMetrics.horizontalAdvance(token_text), 0, 0, 0);

            // Add a space if it is not the last token in the operand.
            if (i < tokens.size() - 1)
            {
                token_rectangle.adjust(option.fontMetrics.horizontalAdvance(IsaItemModel::kOperandTokenSpace), 0, 0, 0);
            }

            token_index++;
        }
    }

    return std::pair<int, QRectF>(token_index, token_rectangle);
}
