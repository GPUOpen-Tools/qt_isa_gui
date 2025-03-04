//=============================================================================
// Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Header for an isa tree view.
//=============================================================================

#ifndef QTISAGUI_ISA_TREE_VIEW_H_
#define QTISAGUI_ISA_TREE_VIEW_H_

#include <QKeyEvent>
#include <QTreeView>
#include <QWheelEvent>

#include "isa_item_model.h"
#include "isa_vertical_scroll_bar.h"

// Forward declaration to prevent a circular dependency.
class IsaWidget;

/// @brief IsaTreeView is a tree view intended to be attached to a IsaItemModel
///        to display isa in a tree structure. It instantiates and uses an IsaItemDelegate
///        to do any custom rendering and handle user interaction.
class IsaTreeView : public QTreeView
{
    Q_OBJECT

public:
    /// @brief Constructor; set default properties on tree and header, create delegate and scroll bar.
    ///
    /// @param [in] parent The parent widget.
    explicit IsaTreeView(QWidget* parent = nullptr);

    /// @brief Destructor.
    virtual ~IsaTreeView();

    /// @brief Set the line #(s) of hot spot(s) to paint a red rectangle in the scroll bar.
    ///        Use an instruction's source index to get its relative line number.
    ///
    /// @param [in] source_indices The source indices of the hot spot instruction(s).
    void SetHotSpotLineNumbers(const std::set<QModelIndex>& source_indices);

    /// @brief Set the line #(s) of text search matches to paint a purple rectangle(s) in the scroll bar.
    ///        Use instructions' source indices to get their relative line numbers.
    ///
    /// @param [in] search_text    While setting the line # matches into the scroll bar, provide the delegate with the search text too.
    /// @param [in] source_indices The source indices of the text search matches.
    void SetSearchMatchLineNumbers(const QString search_text, const std::set<QModelIndex>& source_indices);

    /// @brief Show a popup menu that scrolls to a branch label instruction after pressing a menu action.
    ///
    /// @param source_indices  [in] Source model indices of branch instructions.
    /// @param global_position [in] The global position to show the menu at.
    void ShowBranchInstructionsMenu(QVector<QModelIndex> source_indices, QPoint global_position);

    /// @brief Respond to a request and scroll this tree to the given source model index.
    ///
    /// @param [in] source_index The source model index to scroll to.
    /// @param [in] record       true to record the scroll into navigation history, false to skip recording.
    /// @param [in] select_row   true to select the row of the index, false to not select it.
    virtual void ScrollToIndex(const QModelIndex source_index, bool record, bool select_row);

    /// @brief Toggles the copy_line_numbers_ variable true and false. Used to know if line number text should be included when copying isa text.
    void ToggleCopyLineNumbers();

    /// @brief Saves a link (pointer) to the ISA widget for use later.
    ///
    /// @param [in] widget The pointer to the IsaWidget.
    void RegisterIsaWidget(IsaWidget* widget);

    /// @breif Invalidates the index that keeps track of the last row to have a code block pinned to it when switching events, shader stages, or profiles.
    void ClearLastPinnedndex()
    {
        last_pinned_row_ = std::pair<int, int>(-1, -1);
    }

    /// @brief Turns on or off painting the column separators.
    ///
    /// @param [in] paint true to paint the column separators, false otherwise.
    void PaintColumnSeparators(bool paint)
    {
        paint_column_separators_ = paint;
    }

public slots:

    /// @brief Scroll to a branch or label but do not re-record the entry into history.
    ///
    /// @param [in] branch_label_source_index The source model index to scroll to.
    void ReplayBranchOrLabelSelection(QModelIndex branch_label_source_index);

signals:
    /// @brief Listeners can use this signal to respond to a branch or label that was scrolled to.
    ///
    /// @param [in] branch_label_source_index The source model index that was scrolled to.
    void ScrolledToBranchOrLabel(QModelIndex branch_label_source_index);

protected:
    /// @brief Override drawRow to manually paint alternating background colors to assist painting labels and comments across columns.
    ///
    /// In order to paint code block labels and comments such that they span multiple columns, we have to manually paint the alternating background color in the tree.
    /// If we let Qt paint the background color, it will paint over our attempt to span multiple columns.
    ///
    /// @param [in] painter The painter.
    /// @param [in] option  The style options.
    /// @param [in] index   The index.
    virtual void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const Q_DECL_OVERRIDE;

    /// @brief Override key press to support copy and paste.
    ///
    /// @param [in] event The key event.
    virtual void keyPressEvent(QKeyEvent* event) Q_DECL_OVERRIDE;

    /// @brief Override leave to make sure the isa tooltip is not visible if the mouse leaves this tree view.
    ///
    /// @param [in] event The leave event.
    virtual void leaveEvent(QEvent* event) Q_DECL_OVERRIDE;

    /// @brief Copy selected rows to clipboard.
    void CopyRowsToClipboard();

private slots:

    /// @brief Force hide the tooltip when any index is expanded or collapsed.
    ///
    /// @param [in] index The model index that was changed.
    void IndexExpandedOrCollapsed(const QModelIndex index);

private:
    /// @brief SetAutoScrollObject is a helper class to help temporarily enable a QTreeView's autoScroll property.
    class SetAutoScrollObject
    {
    public:
        /// @brief Explicit constructor.
        ///
        /// @param [in] tree_view The tree view to modify autoScroll for.
        explicit SetAutoScrollObject(QTreeView* tree_view)
        {
            auto_scroll_enabled_ = false;
            tree_view_           = tree_view;
        }

        /// @brief Destructor; turn autoScroll off again if this class enabled it.
        ~SetAutoScrollObject()
        {
            if (auto_scroll_enabled_ && tree_view_ != nullptr)
            {
                tree_view_->setAutoScroll(!auto_scroll_enabled_);
            }
        }

        /// @brief Temporarily enable autoScroll for the attached QTreeView.
        void EnableAutoScroll()
        {
            auto_scroll_enabled_ = true;

            if (tree_view_ != nullptr)
            {
                tree_view_->setAutoScroll(auto_scroll_enabled_);
            }
        }

    private:
        QTreeView* tree_view_;            ///< The attached QTreeView.
        bool       auto_scroll_enabled_;  ///< autoScroll status.

        // Remove constructors.
        SetAutoScrollObject(SetAutoScrollObject& rhs)  = delete;
        SetAutoScrollObject(SetAutoScrollObject&& rhs) = delete;

        // Remove assignment operators.
        SetAutoScrollObject& operator=(SetAutoScrollObject& rhs)  = delete;
        SetAutoScrollObject& operator=(SetAutoScrollObject&& rhs) = delete;
    };

    /// @brief Emit a model data changed signal when the scroll bar is scrolled
    ///        in order to custom render the 0th row in this tree when necessary.
    ///
    /// @param [in] value The new value of the scroll bar.
    void ScrollBarScrolled(int value);

    IsaWidget*            isa_widget_;               ///< The Isa widget.
    IsaVerticalScrollBar* isa_scroll_bar_;           ///< Scroll bar to paint red and purple rectangles corresponding to hot spots and text search matches.
    bool                  copy_line_numbers_;        ///< Whether the line number text is to be included when copying isa text. True by default.
    std::pair<int, int>   last_pinned_row_;          ///< The code block and instruction rows of the last index that was pinned.
    bool                  paint_column_separators_;  ///< Whether or not to paint the the column separators.
};

#endif  // QTISAGUI_ISA_TREE_VIEW_H_
