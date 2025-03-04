//=============================================================================
/// Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation of an isa branch label navigation widget.
//=============================================================================

#include "isa_branch_label_navigation_widget.h"

#include "qt_common/utils/common_definitions.h"
#include "qt_common/utils/qt_util.h"

#include "isa_item_model.h"

const QString kIsaBranchLabelBackNormalResource_      = ":/Resources/assets/browse_back_disabled.svg";
const QString kIsaBranchLabelBackDisabledResource_    = ":/Resources/assets/browse_back_normal.svg";
const QString kIsaBranchLabelForwardNormalResource_   = ":/Resources/assets/browse_fwd_disabled.svg";
const QString kIsaBranchLabelForwardDisabledResource_ = ":/Resources/assets/browse_fwd_normal.svg";

IsaBranchLabelNavigationWidget::IsaBranchLabelNavigationWidget(QWidget* parent)
    : NavigationBar(parent)
    , branch_label_history_combo_(nullptr)
{
    branch_label_history_combo_ = new ArrowIconComboBox(this);

    layout_.insertWidget(1, branch_label_history_combo_);

    connect(&browse_back_button_, &QAbstractButton::pressed, this, &IsaBranchLabelNavigationWidget::BackPressed);

    connect(&browse_forward_button_, &QAbstractButton::pressed, this, &IsaBranchLabelNavigationWidget::ForwardPressed);

    connect(branch_label_history_combo_, &ArrowIconComboBox::SelectedItem, this, &IsaBranchLabelNavigationWidget::HistoryEntrySelected);

    connect(&QtCommon::QtUtils::ColorTheme::Get(), &QtCommon::QtUtils::ColorTheme::ColorThemeUpdated, this, &IsaBranchLabelNavigationWidget::SetButtonIcons);

    ClearHistory();

    // Override the style set in the base class.
    SetButtonIcons();

    browse_back_button_.setStyleSheet("");
    browse_forward_button_.setStyleSheet("");

    layout_.setAlignment(Qt::AlignTop);
    layout_.setContentsMargins(0, 0, 0, 0);
}

IsaBranchLabelNavigationWidget::~IsaBranchLabelNavigationWidget()
{
}

void IsaBranchLabelNavigationWidget::InitializeHistoryComboBox(QWidget* combo_box_parent)
{
    branch_label_history_combo_->InitSingleSelect(combo_box_parent, "", true);
}

void IsaBranchLabelNavigationWidget::ClearHistory()
{
    history_index_ = 0;

    branch_label_history_combo_->ClearItems();

    EnableBackButton(false);
    EnableForwardButton(false);
}

void IsaBranchLabelNavigationWidget::AddBranchOrLabelToHistory(QModelIndex branch_label_source_index)
{
    if (branch_label_history_combo_->RowCount() > 0)
    {
        int index_to_check = history_index_;

        if (index_to_check == branch_label_history_combo_->RowCount())
        {
            index_to_check--;
        }

        const QModelIndex previous_source_index = branch_label_history_combo_->ItemData(index_to_check, Qt::UserRole).toModelIndex();

        if (previous_source_index == branch_label_source_index)
        {
            // Prevent consecutive duplicates.
            return;
        }
    }

    TrimHistory();

    QString line_number_text;
    QString branch_or_label_text;

    line_number_text     = branch_label_source_index.siblingAtColumn(IsaItemModel::Columns::kLineNumber).data(Qt::DisplayRole).toString();
    branch_or_label_text = branch_label_source_index.siblingAtColumn(IsaItemModel::kOpCode).data(Qt::DisplayRole).toString();

    // Add a new entry to history, set the current index to 1 after the end, and clear the selection/highlight.

    branch_label_history_combo_->AddItem(line_number_text + ": " + branch_or_label_text, branch_label_source_index);

    history_index_ = branch_label_history_combo_->RowCount();

    branch_label_history_combo_->ClearSelectedRow();

    EnableBackButton(true);
}

void IsaBranchLabelNavigationWidget::BackPressed()
{
    history_index_--;

    branch_label_history_combo_->SetSelectedRow(history_index_);

    const QModelIndex previous_source_index = branch_label_history_combo_->ItemData(history_index_, Qt::UserRole).toModelIndex();

    emit Navigate(previous_source_index);

    if (history_index_ < branch_label_history_combo_->RowCount() - 1)
    {
        EnableForwardButton(true);
    }

    if (history_index_ == 0)
    {
        EnableBackButton(false);
    }
}

void IsaBranchLabelNavigationWidget::ForwardPressed()
{
    history_index_++;

    branch_label_history_combo_->SetSelectedRow(history_index_);

    const QModelIndex next_source_index = branch_label_history_combo_->ItemData(history_index_, Qt::UserRole).toModelIndex();

    emit Navigate(next_source_index);

    EnableBackButton(true);

    if (history_index_ == branch_label_history_combo_->RowCount() - 1)
    {
        EnableForwardButton(false);
    }
}

void IsaBranchLabelNavigationWidget::HistoryEntrySelected(QListWidgetItem* item)
{
    Q_UNUSED(item);

    history_index_ = branch_label_history_combo_->CurrentRow();

    const QModelIndex selected_entry_source_index = branch_label_history_combo_->ItemData(history_index_, Qt::UserRole).toModelIndex();

    emit Navigate(selected_entry_source_index);

    const bool enable_back_button = (history_index_ == 0) ? false : true;

    EnableBackButton(enable_back_button);

    const bool enable_forward_button = (history_index_ == branch_label_history_combo_->RowCount() - 1) ? false : true;

    EnableForwardButton(enable_forward_button);
}

void IsaBranchLabelNavigationWidget::TrimHistory()
{
    const int row_count = branch_label_history_combo_->RowCount();
    for (int i = row_count; i > history_index_; i--)
    {
        branch_label_history_combo_->RemoveItem(i);

        EnableForwardButton(false);
    }
}

void IsaBranchLabelNavigationWidget::SetButtonIcons()
{
    if (QtCommon::QtUtils::ColorTheme::Get().GetColorTheme() == kColorThemeTypeLight)
    {
        browse_back_button_.SetNormalIcon(QIcon(kIsaBranchLabelBackNormalResource_));
        browse_back_button_.SetHoverIcon(QIcon(kIsaBranchLabelBackNormalResource_));
        browse_back_button_.SetDisabledIcon(QIcon(kIsaBranchLabelBackDisabledResource_));

        browse_forward_button_.SetNormalIcon(QIcon(kIsaBranchLabelForwardNormalResource_));
        browse_forward_button_.SetHoverIcon(QIcon(kIsaBranchLabelForwardNormalResource_));
        browse_forward_button_.SetDisabledIcon(QIcon(kIsaBranchLabelForwardDisabledResource_));
    }
    else
    {
        browse_back_button_.SetNormalIcon(QIcon(kIsaBranchLabelBackDisabledResource_));
        browse_back_button_.SetHoverIcon(QIcon(kIsaBranchLabelBackDisabledResource_));
        browse_back_button_.SetDisabledIcon(QIcon(kIsaBranchLabelBackNormalResource_));

        browse_forward_button_.SetNormalIcon(QIcon(kIsaBranchLabelForwardDisabledResource_));
        browse_forward_button_.SetHoverIcon(QIcon(kIsaBranchLabelForwardDisabledResource_));
        browse_forward_button_.SetDisabledIcon(QIcon(kIsaBranchLabelForwardNormalResource_));
    }
}
