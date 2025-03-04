//=============================================================================
// Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for an isa item delegate.
//=============================================================================

#ifndef QTISAGUI_ISA_ITEM_DELEGATE_H_
#define QTISAGUI_ISA_ITEM_DELEGATE_H_

#include <QModelIndex>
#include <QPainter>
#include <QRectF>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTimer>

#include "isa_item_model.h"
#include "isa_proxy_model.h"
#include "isa_tooltip.h"
#include "isa_tree_view.h"

/// @brief IsaItemDelegate is a styled delegate to be used with the IsaTreeView.
///        It custom paints isa text and handles user interaction.
class IsaItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    /// @brief Constructor.
    ///
    /// @param [in] view   The corresponding tree view.
    /// @param [in] parent The parent object.
    IsaItemDelegate(IsaTreeView* view, QObject* parent = nullptr);

    /// @brief Destructor.
    ~IsaItemDelegate();

    /// @brief Override editor event in order to track mouse moves and mouse clicks over code block labels and selectable tokens.
    ///
    /// @param [in] event  The event that triggered editing.
    /// @param [in] model  The model.
    /// @param [in] option The option used to render the item.
    /// @param [in] index  The index of the item.
    ///
    /// @return true.
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) Q_DECL_OVERRIDE;

    /// @brief Override paint to custom render isa text.
    ///
    /// @param [in] painter     The painter.
    /// @param [in] option      The option used to render the item.
    /// @param [in] model_index The index to render.
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& model_index) const Q_DECL_OVERRIDE;

    /// @brief Override sizeHint to cache text width to improve performance.
    ///
    /// @param [in] option Qt style option parameter.
    /// @param [in] index  Qt model index parameter.
    ///
    /// @return The size hint of the provided model index.
    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const Q_DECL_OVERRIDE;

    /// @brief Hide the tooltip managed by this delegate.
    void HideTooltip() const;

    /// @brief Cache the current search text to assist highlighting search text matches.
    ///
    /// @param [in] search_text The search text.
    inline void SetSearchText(const QString search_text)
    {
        search_text_ = search_text;
    }

    /// @brief Cache the source index of the current search to assist highlighting the current text search match.
    ///
    /// @param [in] search_index The source index.
    inline void SetSearchIndex(const QModelIndex search_index)
    {
        search_source_index_ = search_index;
    }

public slots:
    /// @brief Connects the timer to show the tooltip after the mouse stays hovered over an opcode.
    ///
    /// @param [in] connect_timer Whether to connect or diconnect the timer. true to connect, false to disconnect.
    void ConnectTooltipTimerCallback(bool connect_timer);

protected:
    /// @brief Determines if the source model index is at the top of its corresponding tree viewport and is a child row.
    ///
    /// If the index is at the top and is a child, the parent label will be pinned there instead.
    ///
    /// @param [in]  source_model_index     The source model index.
    /// @param [in]  proxy_model_index      The proxy model index.
    /// @param [out] proxy_index_y_position The y position of the proxy model index.
    ///
    /// @return true if the index is a child index and at the top of the corresponding tree view, false if not.
    bool BlockLabelPinnedToTop(const QModelIndex& source_model_index, const QModelIndex& proxy_model_index, int& proxy_index_y_position) const;

    /// @brief Check if the text provided matches the current search text and paint a rectangle highlight over all matches.
    ///
    /// [in] @param painter                    The painter
    /// [in] @param rectangle                  The starting rectangle to paint into.
    /// [in] @param display_role_text          The text to search and potentially highlight.
    /// [in] @param fixed_font_character_width The view width of a single character.
    /// [in] @param source_index               The source index to paint for.
    void PaintSearchHighlight(QPainter*         painter,
                              const QRectF&     rectangle,
                              const QString     display_role_text,
                              const qreal       fixed_font_character_width,
                              const QModelIndex source_index) const;

    IsaTreeView* view_;  ///< The corresponding tree view.

private slots:

    /// @brief  A timer callback that runs when a delta of time has passed since there was a successful mouse position to isa token position collision.
    void TooltipTimerCallback();

private:
    /// @brief Help navigating from labels to branches by accomodating for column span when calculating x position relative to a column.
    ///
    /// @param [in]  index            The view index to check.
    /// @param [in]  proxy            The proxy model.
    /// @param [out] source_index     The source index to check; will be changed to the op code source index if the given index spans columns.
    /// @param [out] local_x_position The x position of a mouse event; will be changed if the given index spans columns to be relative to the op code column.
    void AdjustXPositionForSpannedColumns(const QModelIndex& index, const QSortFilterProxyModel* proxy, QModelIndex& source_index, qreal& local_x_position);

    /// @brief Helper to get the starting painting position of indicies that span across columns.
    ///
    /// @param [in] is_comment  true if the index belongs to a comment, false otherwise.
    /// @param [in] proxy_index The proxy index to check.
    ///
    /// @return The x position at which painting should start for the index's row that spans across all columns.
    int GetColumnSpanStartPosition(const bool is_comment, const QModelIndex proxy_index) const;

    /// @brief Helper function to help determine if a selectable token is under the mouse.
    ///
    /// @param [in]  source_index                The source model index that the mouse event occurred under.
    /// @param [in]  proxy_index                 The proxy model index that the mouse event occurred under.
    /// @param [in]  local_x_position            The local viewport x position, relative to the index, of the mouse move event.
    /// @param [out] isa_token_under_mouse       The token text that is underneath the mouse, if any.
    /// @param [out] isa_token_under_mouse_index The token index that is underneath the mouse, if any.
    /// @param [in]  offset                      The x offset from the edge of the tree to the starting position of the proxy_index's token.
    /// @param [out] isa_token_hit_box           The hit box of the token that is found under the mouse, if any.
    ///
    /// @return true if a token that is eligible to be selected was found underneath the mouse, false otherwise.
    bool SetSelectableTokenUnderMouse(const QModelIndex&   source_index,
                                      const QModelIndex&   proxy_index,
                                      const int            local_x_position,
                                      IsaItemModel::Token& isa_token_under_mouse,
                                      int&                 isa_token_under_mouse_index,
                                      const int            offset,
                                      QRectF&              isa_token_hit_box);

    /// @brief Helper function to help change the cursor when a branch label token is under the mouse.
    ///
    /// @param [in]  source_index     The source model index that the mouse move event occurred under.
    /// @param [in]  local_x_position The local viewport x position, relative to the index, of the mouse move event.
    bool SetBranchLabelTokenUnderMouse(const QModelIndex& source_index, const int local_x_position);

    /// @brief Helper function to help paint a highlight behind the token that is currently underneath the mouse cursor.
    ///        This function first checks if the requested token is underneath the mouse, and if it is, paints a highlight behind it.
    ///
    /// @param [in] token               The token to check.
    /// @param [in] isa_token_rectangle The token's rectangle.
    /// @param [in] painter             The painter.
    /// @param [in] font_metrics        The font metrics.
    /// @param [in] code_block_index    The token's code block index.
    /// @param [in] instruction_index   The token's instruction index.
    /// @param [in] token_index         The token's index.
    void PaintTokenHighlight(const IsaItemModel::Token& token,
                             const QRectF&              isa_token_rectangle,
                             QPainter*                  painter,
                             const QFontMetrics&        font_metrics,
                             int                        code_block_index,
                             int                        instruction_index,
                             int                        token_index) const;

    /// @brief Helper function to paint the text of a list of isa tokens or isa comments.
    ///
    /// @param [in] painter         The QPainter that will be used for painting.
    /// @param [in] option          The style option for determining font metrics.
    /// @param [in] source_index    The source model index to paint for.
    /// @param [in] token_rectangle The rectangle where the token text will be drawn.
    /// @param [in] tokens          The list of tokens to be painted.
    /// @param [in] token_index     The starting index of the token in the double vector of tokens. Zero if there is only a singe vector.
    /// @param [in] is_comment      true if painting comment text, false if painting an instruction's tokens.
    ///
    /// @return A pair of the final token index and text rectangle for when painting a double vector of tokens.
    std::pair<int, QRectF> PaintText(QPainter*                        painter,
                                     const QStyleOptionViewItem&      option,
                                     const QModelIndex&               source_index,
                                     QRectF                           token_rectangle,
                                     std::vector<IsaItemModel::Token> tokens,
                                     int                              token_index,
                                     bool                             is_comment) const;

    /// @brief Helper function to paint an isa opcode or isa comments in a spanned column.
    ///
    /// @param [in] painter    The QPainter that will be used for painting.
    /// @param [in] option     The style option for determining font metrics.
    /// @param [in] x_position The x position to start painting the spanning text.
    void PaintSpanned(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& source_index, int x_position) const;

    IsaItemModel::Token mouse_over_isa_token_;  ///< Track the token that the mouse is over.
    IsaItemModel::Token selected_isa_token_;    ///< Track the selected token.

    int mouse_over_code_block_index_;   ///< Track the code block index that the mouse is over.
    int mouse_over_instruction_index_;  ///< Track the instruction index that the mouse is over.
    int mouse_over_token_index_;        ///< Track the token index that the mouse is over.

    IsaTooltip* tooltip_;                        ///< A tooltip widget to show extra information  about isa.
    QTimer      tooltip_timer_;                  ///< A timer to help assist gently showing the tooltip.
    QModelIndex tooltip_timeout_source_index_;   ///< The latest source index at which the mouse position has collided with an isa token hit box, if any.
    QRectF      tooltip_timeout_token_hit_box_;  ///< The latest hit box of the isa token which has collided with the mouse, if any.

    QString     search_text_;          ///< Cache the current search text to assist highlighting text search matches.
    QModelIndex search_source_index_;  ///< Cache the current search source index to assist highlighting the current text search match.
};

#endif  // QTISAGUI_ISA_ITEM_DELEGATE_H_
