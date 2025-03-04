//=============================================================================
// Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Isa widget implementation.
//=============================================================================

#include "isa_widget.h"
#include "ui_isa_widget.h"

#include <QCheckBox>
#include <QLayoutItem>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QWidget>

#include "qt_isa_gui/widgets/isa_branch_label_navigation_widget.h"
#include "qt_isa_gui/widgets/isa_item_delegate.h"

static const int kSearchTimeout = 150;

/// @brief Compare 2 model indices in the ISA model; compare indices by their line # (row # but relative to entire shader, not just code block).
///
/// @param [in] lhs The left hand side model index.
/// @param [in] rhs The right hand side model index.
///
/// @return true if lhs is less than rhs, false otherwise.
static bool CompareModelIndices(const QModelIndex& lhs, const QModelIndex& rhs)
{
    const int lhs_line_number = lhs.siblingAtColumn(IsaItemModel::kLineNumber).data(Qt::DisplayRole).toInt();
    const int rhs_line_number = rhs.siblingAtColumn(IsaItemModel::kLineNumber).data(Qt::DisplayRole).toInt();

    return lhs_line_number < rhs_line_number;
}

IsaWidget::IsaWidget(QWidget* parent)
    : QWidget(parent)
    , ui_(new Ui::IsaWidget)
    , proxy_model_(nullptr)
    , go_to_line_validator_(nullptr)
    , viewing_options_visible_(false)
    , show_event_completed_(false)
    , search_all_columns_(false)
{
    ui_->setupUi(this);

    connect(ui_->search_, &QLineEdit::textChanged, this, &IsaWidget::SearchTextChanged);
    connect(ui_->search_, &QLineEdit::returnPressed, this, &IsaWidget::SearchEntered);
    connect(&search_timer_, &QTimer::timeout, this, &IsaWidget::Search);

    ui_->viewing_options_checkboxes_widget_->setVisible(false);

    ui_->viewing_options_combo_->InitSingleSelect(this, "Viewing Options", true);

    ui_->viewing_options_combo_->RemoveEventFilter();

    connect(ui_->viewing_options_combo_, &QPushButton::pressed, this, &IsaWidget::ToggleViewingOptions);

    go_to_line_validator_ = new LineValidator(ui_->go_to_line_);
    ui_->go_to_line_->setValidator(go_to_line_validator_);

    // Make the 'go to line' line edit's style sheet match the 'search' line edit's style sheet.
    ui_->go_to_line_->setStyleSheet("QLineEdit {border: 1px solid gray;}");

    // Set the 'go to line' line edit's width to match its text.
    QFontMetrics go_to_line_font_metrics(ui_->go_to_line_->font());
    int          go_to_line_width = go_to_line_font_metrics.horizontalAdvance(ui_->go_to_line_->placeholderText());
    ui_->go_to_line_->setFixedWidth(go_to_line_width + 10);

    // Try to make the controls widgets at the top look better by aligning them all together.
    for (int i = 0; i < ui_->controls_layout_->count(); i++)
    {
        auto* item = ui_->controls_layout_->itemAt(i);

        if (item->widget() != nullptr)
        {
            ui_->controls_layout_->setAlignment(item->widget(), Qt::AlignLeft | Qt::AlignCenter);
        }
    }

    connect(ui_->go_to_line_, &QLineEdit::returnPressed, this, &IsaWidget::GoToLineEntered);

    // Wait for the show event to complete and then force some widgets to be the same size.
    connect(this, &IsaWidget::ShowEventProcessing, this, &IsaWidget::ShowEventCompleted, Qt::ConnectionType::QueuedConnection);
}

IsaWidget::~IsaWidget()
{
    delete go_to_line_validator_;
}

void IsaWidget::SetModelAndView(QWidget* navigation_widget_parent, IsaItemModel* isa_item_model, IsaTreeView* isa_view, IsaProxyModel* proxy_model)
{
    if (isa_view != nullptr)
    {
        // Find the existing IsaTreeView and replace it.

        QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(this->layout());

        if (layout)
        {
            for (int i = 0; i < layout->count(); ++i)
            {
                QLayoutItem* layout_item = layout->itemAt(i);
                QWidget*     widget      = layout_item->widget();

                if (widget != nullptr && widget == ui_->isa_tree_view_)
                {
                    layout_item = layout->takeAt(i);
                    layout_item->widget()->deleteLater();
                    delete layout_item;

                    layout->insertWidget(i, isa_view, 1);
                    ui_->isa_tree_view_ = isa_view;
                    break;
                }
            }

            layout->invalidate();
        }
    }

    if (ui_->isa_tree_view_ != nullptr)
    {
        ui_->isa_tree_view_->RegisterIsaWidget(this);
    }

    // Attach a client's proxy or make the default one instead.
    if (proxy_model != nullptr)
    {
        proxy_model_ = proxy_model;
    }
    else
    {
        proxy_model_ = new IsaProxyModel;
    }

    proxy_model_->setSourceModel(isa_item_model);
    ui_->isa_tree_view_->setModel(proxy_model_);

    for (uint32_t column = IsaItemModel::kPcAddress; column < proxy_model_->GetNumberOfViewingOptions(); column++)
    {
        proxy_model_->CreateViewingOptionsCheckbox(column, ui_->viewing_options_checkboxes_widget_);
        auto checkbox = proxy_model_->GetViewingOptionsCheckbox(column);
        if (checkbox != nullptr)
        {
            connect(checkbox, &QAbstractButton::clicked, this, &IsaWidget::ShowHideColumnClicked);
        }
    }

    // Pass a pointer to the designated parent widget to the nav widget so it can render its combo box correctly.
    ui_->branch_label_navigation_->InitializeHistoryComboBox(navigation_widget_parent);

    // Connect the Signal for the Architecture changing in the model, with the delegate's slot to enable to tooltip timer.
    auto* isa_delegate = qobject_cast<IsaItemDelegate*>(ui_->isa_tree_view_->itemDelegate());
    if (isa_delegate != nullptr)
    {
        connect(isa_item_model, &IsaItemModel::ArchitectureChanged, isa_delegate, &IsaItemDelegate::ConnectTooltipTimerCallback);
    }

    // Connect the tree view to the nav widget to assist recording navigation history.

    connect(
        ui_->isa_tree_view_, &IsaTreeView::ScrolledToBranchOrLabel, ui_->branch_label_navigation_, &IsaBranchLabelNavigationWidget::AddBranchOrLabelToHistory);

    connect(ui_->branch_label_navigation_, &IsaBranchLabelNavigationWidget::Navigate, ui_->isa_tree_view_, &IsaTreeView::ReplayBranchOrLabelSelection);

    // Listen to isa tree expand/collapse of code block indicies to update the search match indices.
    connect(ui_->isa_tree_view_, &QTreeView::collapsed, this, &IsaWidget::RefreshSearchMatchLineNumbers);
    connect(ui_->isa_tree_view_, &QTreeView::expanded, this, &IsaWidget::RefreshSearchMatchLineNumbers);
}

void IsaWidget::ExpandCollapseAll(bool expand, bool resize_contents, std::deque<bool>* collapsed_blocks)
{
    // Disconnect to prevent duplicate updates.
    disconnect(ui_->isa_tree_view_, &QTreeView::collapsed, this, &IsaWidget::RefreshSearchMatchLineNumbers);
    disconnect(ui_->isa_tree_view_, &QTreeView::expanded, this, &IsaWidget::RefreshSearchMatchLineNumbers);

    if (expand)
    {
        if (collapsed_blocks == nullptr)
        {
            ui_->isa_tree_view_->expandAll();
        }
        else
        {
            IsaItemModel* source_model = nullptr;

            if (proxy_model_ != nullptr)
            {
                source_model = qobject_cast<IsaItemModel*>(proxy_model_->sourceModel());
            }
            else
            {
                source_model = qobject_cast<IsaItemModel*>(ui_->isa_tree_view_->model());
            }

            if (source_model != nullptr)
            {
                for (int i = 0; i < source_model->rowCount(); i++)
                {
                    const QModelIndex code_block_source_index = source_model->index(i, IsaItemModel::kLineNumber);
                    const QModelIndex code_block_proxy_index =
                        proxy_model_ != nullptr ? proxy_model_->mapFromSource(code_block_source_index) : code_block_source_index;

                    const bool is_block_collapsed = collapsed_blocks->at(i);

                    ui_->isa_tree_view_->setExpanded(code_block_proxy_index, !is_block_collapsed);
                }
            }
        }

        const QAbstractItemModel* model = ui_->isa_tree_view_->model();
        if (model != nullptr && resize_contents)
        {
            for (int i = 0; i < model->columnCount(); i++)
            {
                ui_->isa_tree_view_->resizeColumnToContents(i);
            }
        }
    }
    else
    {
        ui_->isa_tree_view_->collapseAll();
    }

    RefreshSearchMatchLineNumbers(QModelIndex());

    // Reconnect to resume normal updates.
    connect(ui_->isa_tree_view_, &QTreeView::collapsed, this, &IsaWidget::RefreshSearchMatchLineNumbers);
    connect(ui_->isa_tree_view_, &QTreeView::expanded, this, &IsaWidget::RefreshSearchMatchLineNumbers);
}

IsaWidget::ExpandCollapseState IsaWidget::SaveExpandState()
{
    ExpandCollapseState expand_collapse_state;

    if (proxy_model_ == nullptr)
    {
        return expand_collapse_state;
    }

    const IsaItemModel* source_model       = qobject_cast<IsaItemModel*>(proxy_model_->sourceModel());
    const int           number_code_blocks = source_model->rowCount();

    for (int i = 0; i < number_code_blocks; i++)
    {
        const QModelIndex code_block_line_number_source_index = source_model->index(i, IsaItemModel::kLineNumber);
        const QModelIndex code_block_line_number_proxy_index  = proxy_model_->mapFromSource(code_block_line_number_source_index);

        const bool is_code_block_expanded = ui_->isa_tree_view_->isExpanded(code_block_line_number_proxy_index);

        expand_collapse_state.emplace_back(is_code_block_expanded);
    }

    return expand_collapse_state;
}

void IsaWidget::RestoreExpandState(ExpandCollapseState expand_collapse_state)
{
    if (proxy_model_ == nullptr)
    {
        return;
    }

    const IsaItemModel* source_model       = qobject_cast<IsaItemModel*>(proxy_model_->sourceModel());
    const int           number_code_blocks = source_model->rowCount();

    if (number_code_blocks != static_cast<int>(expand_collapse_state.size()))
    {
        return;
    }

    // Disconnect to prevent duplicate updates.
    disconnect(ui_->isa_tree_view_, &QTreeView::collapsed, this, &IsaWidget::RefreshSearchMatchLineNumbers);
    disconnect(ui_->isa_tree_view_, &QTreeView::expanded, this, &IsaWidget::RefreshSearchMatchLineNumbers);

    int code_block_row = 0;

    for (const bool is_code_block_expanded : expand_collapse_state)
    {
        const QModelIndex code_block_line_number_source_index = source_model->index(code_block_row, IsaItemModel::kLineNumber);
        const QModelIndex code_block_line_number_proxy_index  = proxy_model_->mapFromSource(code_block_line_number_source_index);

        ui_->isa_tree_view_->setExpanded(code_block_line_number_proxy_index, is_code_block_expanded);

        code_block_row++;
    }

    RefreshSearchMatchLineNumbers(QModelIndex());

    // Reconnect to resume normal updates.
    connect(ui_->isa_tree_view_, &QTreeView::collapsed, this, &IsaWidget::RefreshSearchMatchLineNumbers);
    connect(ui_->isa_tree_view_, &QTreeView::expanded, this, &IsaWidget::RefreshSearchMatchLineNumbers);
}

void IsaWidget::UpdateSpannedColumns()
{
    if (proxy_model_ == nullptr)
    {
        return;
    }

    const QAbstractItemModel* source_model = proxy_model_->sourceModel();

    for (int i = 0; i < source_model->rowCount(); i++)
    {
        const int proxy_row = proxy_model_->mapFromSource(source_model->index(i, IsaItemModel::kOpCode)).row();

        // All parent labels (code blocks or comments) should span across columns.
        ui_->isa_tree_view_->setFirstColumnSpanned(proxy_row, QModelIndex(), true);

        const QModelIndex source_parent_index = source_model->index(i, IsaItemModel::kLineNumber);

        for (int j = 0; j < source_model->rowCount(source_parent_index); j++)
        {
            QModelIndex source_child_index = source_model->index(j, IsaItemModel::kOpCode, source_parent_index);
            const auto  row_type           = qvariant_cast<IsaItemModel::RowType>(source_child_index.data(IsaItemModel::kRowTypeRole));
            bool        spanned            = false;

            if (row_type == IsaItemModel::RowType::kComment)
            {
                spanned = true;
            }

            const int proxy_child_row = proxy_model_->mapFromSource(source_child_index).row();

            // Child comments should span across columns.
            ui_->isa_tree_view_->setFirstColumnSpanned(proxy_child_row, proxy_model_->mapFromSource(source_parent_index), spanned);
        }
    }
    ui_->isa_tree_view_->ClearLastPinnedndex();
}

void IsaWidget::ClearHistory()
{
    ui_->branch_label_navigation_->ClearHistory();
}

void IsaWidget::SetFocusOnGoToLineWidget()
{
    ui_->go_to_line_->setFocus();
}

void IsaWidget::SetFocusOnSearchWidget()
{
    ui_->search_->setFocus();
}

void IsaWidget::SetGoToLineValidatorLineCount(int line_count)
{
    ui_->go_to_line_->clear();
    go_to_line_validator_->SetLineCount(line_count);
}

void IsaWidget::Search()
{
    if (proxy_model_ == nullptr)
    {
        return;
    }

    ui_->search_results_->setText("No results");

    matches_.clear();

    const auto text = ui_->search_->text();

    std::set<QModelIndex> match_source_indices;
    ui_->isa_tree_view_->SetSearchMatchLineNumbers(text, match_source_indices);

    if (!text.isEmpty())
    {
        ui_->isa_tree_view_->selectionModel()->clearSelection();

        IsaItemModel* source_model = qobject_cast<IsaItemModel*>(proxy_model_->sourceModel());

        if (source_model != nullptr)
        {
            // Search by searching each column; don't search the line number column.
            for (int col = (int)IsaItemModel::kLineNumber + 1; col < proxy_model_->columnCount(); col++)
            {
                QModelIndex     column_index   = proxy_model_->index(0, col);
                QModelIndexList column_matches = proxy_model_->match(column_index, Qt::DisplayRole, text, -1, Qt::MatchContains | Qt::MatchRecursive);

                for (QModelIndex index : column_matches)
                {
                    const auto source_index = proxy_model_->mapToSource(index);

                    // Always count the match if it is a column from the IsaItemModel, otherwise count it if the application requested searching all columns.
                    if (source_index.column() < IsaItemModel::kColumnCount || search_all_columns_)
                    {
                        // Store into matches_; use the first column to avoid matching more than 1 index from any given row.
                        matches_ += index.siblingAtColumn(IsaItemModel::kLineNumber);
                    }
                }
            }

            // Sort and uniquify.
            std::sort(matches_.begin(), matches_.end(), CompareModelIndices);
            matches_.erase(std::unique(matches_.begin(), matches_.end()), matches_.end());

            if (!matches_.isEmpty())
            {
                find_index_ = 0;

                ui_->search_results_->setText(QString("%1 of %2").arg(find_index_ + 1).arg(matches_.size()));

                const QModelIndex& view_index   = matches_.at(find_index_);
                const QModelIndex  source_index = proxy_model_->mapToSource(view_index);
                auto*              delegate     = qobject_cast<IsaItemDelegate*>(ui_->isa_tree_view_->itemDelegate());

                if (delegate != nullptr)
                {
                    delegate->SetSearchIndex(source_index);
                }

                ui_->isa_tree_view_->ScrollToIndex(source_index, false, false);

                emit SearchMatchLineChanged(source_index);
            }

            for (const QModelIndex& match_view_index : matches_)
            {
                QModelIndex match_source_index = proxy_model_->mapToSource(match_view_index);
                match_source_indices.emplace(match_source_index);
            }

            ui_->isa_tree_view_->SetSearchMatchLineNumbers(text, match_source_indices);
        }
    }

    search_timer_.stop();

    // Update the isa tree and its scroll bar.
    ui_->isa_tree_view_->viewport()->update();
    ui_->isa_tree_view_->verticalScrollBar()->update();
}

void IsaWidget::keyPressEvent(QKeyEvent* event)
{
    bool event_handled = false;

    if (event->key() == Qt::Key_F && (event->modifiers() & Qt::ControlModifier))
    {
        ui_->search_->setFocus();

        event->accept();
        event_handled = true;
    }
    else if (event->key() == Qt::Key_G && (event->modifiers() & Qt::ControlModifier))
    {
        ui_->go_to_line_->setFocus();

        event->accept();
        event_handled = true;
    }
    else if (event->key() == Qt::Key_Escape)
    {
        ui_->search_->clear();
        ui_->search_->clearFocus();
        ui_->go_to_line_->clear();
        ui_->go_to_line_->clearFocus();

        ui_->isa_tree_view_->clearSelection();

        event->accept();
        event_handled = true;
    }

    // Handle Ctrl+Left/Ctrl+Right to expand/collapse code blocks.
    if ((event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) && (event->modifiers() & Qt::ControlModifier))
    {
        ui_->isa_tree_view_->blockSignals(true);

        bool expand_all = event->key() == Qt::Key_Right;

        ExpandCollapseAll(expand_all, false);

        ui_->isa_tree_view_->blockSignals(false);

        event->accept();
        event_handled = true;
    }

    if (!event_handled)
    {
        QWidget::keyPressEvent(event);
    }
}

void IsaWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (!show_event_completed_)
    {
        emit ShowEventProcessing();

        show_event_completed_ = true;
    }
}

void IsaWidget::ShowHideColumnClicked(bool checked)
{
    if (proxy_model_ == nullptr)
    {
        return;
    }

    const QObject* sender = this->sender();

    QHeaderView* header = ui_->isa_tree_view_->header();

    uint32_t source_column_index = proxy_model_->GetSourceColumnIndex(static_cast<const QCheckBox*>(sender));

    if (source_column_index != proxy_model_->GetNumberOfViewingOptions())
    {
        int proxy_index  = proxy_model_->mapFromSource(proxy_model_->sourceModel()->index(0, source_column_index)).column();
        int visual_index = header->visualIndex(proxy_index);

        proxy_model_->SetColumnVisibility(source_column_index, checked, header);

        if (checked)
        {
            ui_->isa_tree_view_->resizeColumnToContents(proxy_model_->mapFromSource(proxy_model_->sourceModel()->index(0, source_column_index)).column());
        }
        else
        {
            // If the last column was just removed, resize the column with the next logical index to prevent a bug where it gets too large.
            if (visual_index == proxy_model_->columnCount())
            {
                // Check if there was a next logical index.
                if (header->visualIndex(proxy_index) != -1)
                {
                    ui_->isa_tree_view_->resizeColumnToContents(proxy_index);
                }
            }
        }
    }

    // If there is only one column being shown, we need to disable the checkbox for that column.
    // Otherwise, the user could hide the last column, and the tree would not be useful.
    uint32_t                check_count  = 0;
    QCheckBox*              last_checked = nullptr;
    const QList<QCheckBox*> children     = ui_->viewing_options_checkboxes_widget_->findChildren<QCheckBox*>();

    for (auto& child : children)
    {
        if (child->isChecked())
        {
            check_count++;
            last_checked = child;
        }
        child->setEnabled(true);
    }

    if (check_count == 1 && nullptr != last_checked)
    {
        last_checked->setEnabled(false);
    }
}

bool IsaWidget::DoesIndexMatchSearch(QModelIndex index)
{
    QModelIndex sibling = index.siblingAtColumn(IsaItemModel::kLineNumber);
    return matches_.contains(sibling);
}

IsaTreeView* IsaWidget::GetTreeView() const
{
    return ui_->isa_tree_view_;
}

void IsaWidget::SearchTextChanged(const QString& text)
{
    Q_UNUSED(text);

    search_timer_.start(kSearchTimeout);
}

void IsaWidget::SearchEntered()
{
    if (proxy_model_ == nullptr)
    {
        return;
    }

    if (!matches_.isEmpty())
    {
        find_index_++;
        if (find_index_ >= matches_.size())
        {
            find_index_ = 0;
        }

        ui_->search_results_->setText(QString("%1 of %2").arg(find_index_ + 1).arg(matches_.size()));

        const QModelIndex& view_index   = matches_.at(find_index_);
        QModelIndex        source_index = proxy_model_->mapToSource(view_index);
        auto*              delegate     = qobject_cast<IsaItemDelegate*>(ui_->isa_tree_view_->itemDelegate());

        if (delegate != nullptr)
        {
            delegate->SetSearchIndex(source_index);
        }

        ui_->isa_tree_view_->ScrollToIndex(source_index, false, false);

        emit SearchMatchLineChanged(source_index);

        // Make sure the tree repaints.
        ui_->isa_tree_view_->viewport()->update();
    }
}

void IsaWidget::GoToLineEntered()
{
    if (proxy_model_ == nullptr)
    {
        return;
    }

    IsaItemModel* source_model = qobject_cast<IsaItemModel*>(proxy_model_->sourceModel());

    if (source_model == nullptr)
    {
        return;
    }

    const int go_to_line_number = ui_->go_to_line_->text().toInt();

    const QModelIndex source_go_to_index = source_model->GetLineNumberModelIndex(go_to_line_number);

    ui_->isa_tree_view_->ScrollToIndex(source_go_to_index, false, true);
}

void IsaWidget::ToggleViewingOptions()
{
    ui_->viewing_options_combo_->ToggleDirection();

    viewing_options_visible_ = !viewing_options_visible_;

    ui_->viewing_options_checkboxes_widget_->setVisible(viewing_options_visible_);
}

void IsaWidget::RefreshSearchMatchLineNumbers(const QModelIndex& index)
{
    if (proxy_model_ == nullptr)
    {
        return;
    }

    Q_UNUSED(index);

    std::set<QModelIndex> match_source_indices;

    for (const QModelIndex& match_proxy_index : matches_)
    {
        QModelIndex match_source_index = proxy_model_->mapToSource(match_proxy_index);
        match_source_indices.emplace(match_source_index);
    }

    ui_->isa_tree_view_->SetSearchMatchLineNumbers(ui_->search_->text(), match_source_indices);
}

void IsaWidget::BranchLabelNavigationForward()
{
    if (ui_->branch_label_navigation_->ForwardButton().isEnabled())
    {
        ui_->branch_label_navigation_->ForwardPressed();
    }
}

void IsaWidget::BranchLabelNavigationBack()
{
    if (ui_->branch_label_navigation_->BackButton().isEnabled())
    {
        ui_->branch_label_navigation_->BackPressed();
    }
}

void IsaWidget::ShowEventCompleted()
{
    // Force the go to line edit to be the exact height of the search line edit by taking its height.
    // The search line edit uses an icon which makes its height bigger.
    ui_->go_to_line_->setFixedHeight(ui_->search_->height());
}
