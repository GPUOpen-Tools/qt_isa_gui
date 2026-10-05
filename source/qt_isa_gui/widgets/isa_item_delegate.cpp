//=============================================================================
// Copyright (c) 2022-2026 Advanced Micro Devices, Inc. All rights reserved.
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

#include "amdisa/isa_decoder.h"

#include "isa_item_model.h"
#include "isa_tree_view.h"

/// @brief Paint a token's text using a color based on its type or syntax.
///
/// @param [in] token                The token to paint.
/// @param [in] source_index         The source index of the token that is being painted.
/// @param [in] token_rectangle      The rectangle to paint to.
/// @param [in] painter              The painter.
/// @param [in] color_coding_enabled True to apply a color coding to the token, false otherwise.
static void PaintTokenText(const IsaItemModel::Token&                 token,
                           const amdisa::FunctionalGroupSubgroupInfo& functional_group_info,
                           const QRectF&                              token_rectangle,
                           QPainter*                                  painter,
                           const bool                                 color_coding_enabled)
{
    painter->save();

    if (color_coding_enabled)
    {
        QColor color;
        auto   pen = painter->pen();

        if (token.type == IsaItemModel::TokenType::kBranchLabelType)
        {
            QColor operand_color = kIsaColorMagenta;

            // Operand that is the target of a branch instruction.
            color = operand_color;
        }
        else if (!IsaColorCodingDictionaryInstance::GetInstance().ShouldHighlight(token.token_text, functional_group_info, color))
        {
            color = pen.color();
        }

        pen.setColor(color);
        painter->setPen(pen);
    }

    painter->drawText(token_rectangle, Qt::TextSingleLine, token.token_text.c_str());

    painter->restore();
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

void IsaItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& proxy_index) const
{
    if (!proxy_index.isValid())
    {
        return;
    }

    const IsaProxyModel* proxy_model = qobject_cast<const IsaProxyModel*>(proxy_index.model());

    if (proxy_model == nullptr)
    {
        return;
    }

    const IsaItemModel* source_model = qobject_cast<const IsaItemModel*>(proxy_model->sourceModel());

    if (source_model == nullptr)
    {
        return;
    }

    const QModelIndex source_index              = proxy_model->mapToSource(proxy_index);
    const bool        block_label_pinned_to_top = BlockLabelPinnedToTop(proxy_index);

    QStyleOptionViewItem initialized_option = option;
    initStyleOption(&initialized_option, source_index);
    QRectF paint_rectangle = initialized_option.rect;

    painter->save();
    painter->setFont(initialized_option.font);

    if (block_label_pinned_to_top)
    {
        // Pin instructions' block label to the top of the viewport and paint them instead of painting the instruction.

        PaintPinnedBlockLabel(painter, source_index, paint_rectangle, proxy_model);

        painter->restore();
        return;
    }

    PaintRowSelection(painter, initialized_option);

    if (source_index.column() >= IsaItemModel::kColumnCount)
    {
        // Don't try to paint any columns not defined in the isa model.
        painter->restore();
        return;
    }

    const auto          row_type          = source_index.data(IsaItemModel::UserRoles::kRowTypeRole).value<IsaItemModel::RowType>();
    const auto          span_columns      = view_->isFirstColumnSpanned(proxy_index.row(), proxy_index.parent());
    const auto          display_role_text = GetIndexPlainText(span_columns, source_index);
    const QFontMetricsF font_metrics(view_->font(), view_);

    AdjustPaintRectangle(paint_rectangle, row_type, source_index, proxy_model, font_metrics, span_columns);

    PaintSearchHighlight(painter, paint_rectangle, display_role_text, source_model->GetFixedFontCharacterWidth(), source_index);

    // Get a default text color if applicable.
    const auto default_text_color = source_index.data(Qt::ForegroundRole).value<QColor>();
    auto       pen                = painter->pen();
    pen.setColor(default_text_color);
    painter->setPen(pen);

    QModelIndex source_index_to_paint = source_index;

    if (source_index.column() == IsaItemModel::kLineNumber)
    {
        if (source_model->LineNumbersVisible())
        {
            // Paint line #s if they aren't turned off.
            PaintLineNumber(painter, source_index, option.rect);
        }

        if (span_columns)
        {
            // Rows that span columns get their meta-data from the op code column.
            source_index_to_paint = source_index.siblingAtColumn(IsaItemModel::kOpCode);
        }
    }

    // Custom paint all columns defined in the isa model.
    PaintText(painter, source_index_to_paint, paint_rectangle);

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

bool IsaItemDelegate::BlockLabelPinnedToTop(const QModelIndex& proxy_index) const
{
    const auto proxy_index_visual_rect = view_->visualRect(proxy_index);
    const auto proxy_index_y_position  = proxy_index_visual_rect.y();

    if (proxy_index.parent().isValid() && proxy_index_y_position == 0)
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
        // When painting spanning text for comments or isa labels while the op code column is not visible,
        // start painting right after the line # column.
        x_position = view_->header()->sectionPosition(view_->header()->logicalIndex(1));
    }
    else
    {
        // When painting spanning text for an isa label while the op code column is visible,
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

    if (BlockLabelPinnedToTop(proxy_index))
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
                                          const QFontMetricsF&       font_metrics,
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
    const auto token_highlight_color = kIsaColorLightPink;

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

void IsaItemDelegate::PaintText(QPainter* painter, const QModelIndex& source_index, QRectF paint_rectangle) const
{
    painter->save();

    if ((source_index.column() != IsaItemModel::kPcAddress && source_index.column() != IsaItemModel::kBinaryRepresentation))
    {
        // Paint comments and isa as bold.
        auto font = painter->font();
        font.setBold(true);
        painter->setFont(font);
    }

    // Refresh the starting color for the source index provided.
    const auto text_color = source_index.data(Qt::ForegroundRole).value<QColor>();
    auto       pen        = painter->pen();
    pen.setColor(text_color);
    painter->setPen(pen);

    // Refresh the style option for the source index provided.
    QStyleOptionViewItem initialized_option;
    initStyleOption(&initialized_option, source_index);

    const auto row_type = qvariant_cast<IsaItemModel::RowType>(source_index.data(IsaItemModel::UserRoles::kRowTypeRole));

    if ((row_type == IsaItemModel::RowType::kComment) ||
        ((row_type == IsaItemModel::RowType::kIsa) &&
         (source_index.column() == IsaItemModel::kPcAddress || source_index.column() == IsaItemModel::kBinaryRepresentation)))
    {
        // Paint parent and child comments, pc address and binary representation, as a single string using a single color.

        painter->drawText(paint_rectangle, initialized_option.displayAlignment, source_index.data(Qt::DisplayRole).toString());
    }
    else if ((row_type == IsaItemModel::RowType::kIsa))
    {
        if ((source_index.column() == IsaItemModel::kOpCode))
        {
            // Paint parent isa block label or child op code as a single color coded token.

            const std::vector<IsaItemModel::Token> op_code_token        = qvariant_cast<std::vector<IsaItemModel::Token>>(source_index.data(Qt::UserRole));
            const bool                             color_coding_enabled = source_index.data(IsaItemModel::kLineEnabledRole).toBool();
            const QFontMetricsF                    font_metrics(view_->font(), view_);

            PaintTokenHighlight(op_code_token.front(),
                                paint_rectangle,
                                painter,
                                font_metrics,
                                source_index.parent().row(),
                                source_index.row(),
                                0);  // Assume 0 index for op code.

            const amdisa::InstructionInfo& instruction_info = source_index.data(IsaItemModel::UserRoles::kDecodedIsa).value<amdisa::InstructionInfo>();
            const amdisa::FunctionalGroupSubgroupInfo& functional_group_info = instruction_info.functional_group_subgroup_info;

            PaintTokenText(op_code_token.front(), functional_group_info, paint_rectangle, painter, color_coding_enabled);
        }
        else if ((source_index.column() == IsaItemModel::kOperands) && (source_index.parent().isValid()))
        {
            // Paint child isa operands as a series of color coded tokens.

            PaintOperands(painter, source_index, paint_rectangle);
        }
    }

    painter->restore();
}

void IsaItemDelegate::PaintOperands(QPainter* painter, const QModelIndex& source_index, QRectF paint_rectangle) const
{
    std::vector<std::vector<IsaItemModel::Token>> operand_groups_tokens =
        qvariant_cast<std::vector<std::vector<IsaItemModel::Token>>>(source_index.data(Qt::UserRole));

    int token_index = 0;  // Track token index to assist painting selection highlight.

    const QFontMetricsF font_metrics(view_->font(), view_);

    // Pass an empty functional group info struct for operands.
    amdisa::FunctionalGroupSubgroupInfo functional_group_info{};

    // Iterate over operands.
    for (size_t i = 0; i < operand_groups_tokens.size(); i++)
    {
        const auto& operand_tokens = operand_groups_tokens.at(i);

        // Iterate over tokens within an operand.
        for (size_t j = 0; j < operand_tokens.size(); j++)
        {
            const auto token = operand_tokens.at(j);

            // Check if we need to paint a selection highlight.
            if (token.is_selectable)
            {
                PaintTokenHighlight(token, paint_rectangle, painter, font_metrics, source_index.parent().row(), source_index.row(), token_index);
            }

            // Paint a color coded operand token.
            const bool color_coding_enabled = source_index.data(IsaItemModel::kLineEnabledRole).toBool();
            PaintTokenText(token, functional_group_info, paint_rectangle, painter, color_coding_enabled);

            // Re-use color and draw a line underneath tokens that are the target of a branch instruction.
            if (token.type == IsaItemModel::TokenType::kBranchLabelType)
            {
                QPoint label_underline_start(paint_rectangle.x() + token.x_position_start, paint_rectangle.bottom());
                QPoint label_underline_end(paint_rectangle.x() + token.x_position_end, paint_rectangle.bottom());

                painter->drawLine(label_underline_start, label_underline_end);
            }

            // Move x position forward.
            const QString token_text(token.token_text.c_str());
            paint_rectangle.adjust(font_metrics.horizontalAdvance(token_text), 0, 0, 0);

            // Add a space if it is not the last token in the operand.
            if (j < operand_tokens.size() - 1)
            {
                paint_rectangle.adjust(font_metrics.horizontalAdvance(IsaItemModel::kOperandTokenSpace), 0, 0, 0);
            }

            token_index++;
        }

        // Add a comma if it is not the last operand.
        if (i < operand_groups_tokens.size() - 1)
        {
            PaintCommaText(paint_rectangle, painter);

            paint_rectangle.adjust(font_metrics.horizontalAdvance(QString(IsaItemModel::kOperandDelimiter)), 0, 0, 0);
        }
    }
}

void IsaItemDelegate::PaintLineNumber(QPainter* painter, const QModelIndex& source_index, QRectF paint_rectangle) const
{
    const QFontMetricsF font_metrics(view_->font(), view_);
    const auto          line_number_text = source_index.data(Qt::DisplayRole).toString() + IsaItemModel::kColumnPadding;

    // Right align the line number to its column.
    const int line_number_column_width = view_->header()->sectionSize(view_->header()->logicalIndex(0));
    const int line_number_text_width   = font_metrics.horizontalAdvance(line_number_text);
    const int scroll_bar_position      = view_->horizontalScrollBar()->value();
    const int line_number_x_position   = line_number_column_width - line_number_text_width - scroll_bar_position;

    paint_rectangle.setX(line_number_x_position);
    paint_rectangle.setWidth(font_metrics.horizontalAdvance(line_number_text));

    painter->drawText(paint_rectangle, Qt::Alignment(Qt::AlignLeft | Qt::AlignTop), line_number_text);
}

void IsaItemDelegate::PaintPinnedBlockLabel(QPainter* painter, const QModelIndex& source_index, QRectF paint_rectangle, const IsaProxyModel* proxy_model) const
{
    // Get the parent isa block index that is off screen from the original instruction index.
    const auto          parent_op_code_source_index = source_index.parent().siblingAtColumn(IsaItemModel::kOpCode);
    const auto          parent_row_type             = parent_op_code_source_index.data(IsaItemModel::UserRoles::kRowTypeRole).value<IsaItemModel::RowType>();
    const QFontMetricsF font_metrics(view_->font(), view_);

    AdjustPaintRectangle(paint_rectangle, parent_row_type, source_index, proxy_model, font_metrics, true);

    PaintText(painter, parent_op_code_source_index, paint_rectangle);
}

void IsaItemDelegate::PaintRowSelection(QPainter* painter, const QStyleOptionViewItem& option) const
{
    // If this row is selected or moused-over, render a highlight.

    if ((((option.state & QStyle::State_Selected) != 0) || ((option.state & QStyle::State_MouseOver) != 0)))
    {
        option.widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);
    }
}

QString IsaItemDelegate::GetIndexPlainText(const bool span_columns, const QModelIndex& source_index) const
{
    // Get the concatenated text of an index from its display role.

    QString display_role_text;

    if (span_columns)
    {
        // Text for rows that span columns (comments and isa labels) are stored in the op code column.

        const auto op_code_source_index = source_index.siblingAtColumn(IsaItemModel::kOpCode);
        display_role_text               = op_code_source_index.data(Qt::DisplayRole).toString();
    }
    else if (source_index.column() != IsaItemModel::Columns::kLineNumber)
    {
        // Allow text search highlighting for every column but line number.

        display_role_text = source_index.data(Qt::DisplayRole).toString();
    }

    return display_role_text;
}

void IsaItemDelegate::AdjustPaintRectangle(QRectF&                     paint_rectangle,
                                           const IsaItemModel::RowType row_type,
                                           const QModelIndex&          source_index,
                                           const IsaProxyModel*        proxy_model,
                                           const QFontMetricsF         font_metrics,
                                           const bool                  span_columns) const
{
    if (span_columns)
    {
        // Determine if a spanning row should start painting at the op code column or the line number column.
        // Extend the rectangle to the end of the view.

        const auto op_code_source_index = source_index.siblingAtColumn(IsaItemModel::kOpCode);
        const auto op_code_proxy_index  = proxy_model->mapFromSource(op_code_source_index);
        const bool is_comment           = row_type == IsaItemModel::RowType::kComment;
        auto       x_position           = GetColumnSpanStartPosition(is_comment, op_code_proxy_index);

        x_position -= view_->horizontalScrollBar()->value();

        paint_rectangle.setX(x_position);
        paint_rectangle.setWidth(view_->width() - paint_rectangle.x());
    }
    else if ((source_index.column() == IsaItemModel::kOpCode) && (row_type == IsaItemModel::RowType::kIsa) && (source_index.parent().isValid()))
    {
        // Advance the starting position of the text by a predefined indent for child instruction op codes not pinned to the top of the view.

        paint_rectangle.setX(paint_rectangle.x() + font_metrics.horizontalAdvance(IsaItemModel::kOpCodeColumnIndent));
    }
}
