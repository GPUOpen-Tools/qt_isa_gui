//=============================================================================
// Copyright (c) 2022-2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation of an isa branch label navigation widget.
//=============================================================================

#include "isa_branch_label_navigation_widget.h"

#include <QPushButton>

#include "qt_common/utils/common_definitions.h"
#include "qt_common/utils/qt_util.h"

#include "isa_item_model.h"

/// @brief Initialize the qt_isa_gui resource file. Required for static libraries.
static void InitQtIsaGuiResources()
{
    Q_INIT_RESOURCE(qt_isa_gui_resources);
}

namespace
{
    const QString button_style =
        "QPushButton:enabled  { border: 1px solid palette(text); background: palette(button); }"
        "QPushButton:disabled { border: 1px solid gray; background: palette(button);          }"
        "QPushButton:hover    { background-color: palette(alternate-base);                    }"
        "QPushButton:pressed  { background-color: gray;                                       }";
}

IsaBranchLabelNavigationWidget::IsaBranchLabelNavigationWidget(QWidget* parent)
    : QWidget(parent)
    , branch_label_history_combo_(nullptr)
{
    InitQtIsaGuiResources();

    branch_label_history_combo_ = new ArrowIconComboBox(this);
    previous_button_            = new QPushButton(this);
    next_button_                = new QPushButton(this);

    previous_button_->setStyleSheet(button_style);
    previous_button_->setIconSize(QSize(20, 18));
    previous_button_->setFixedSize(20, 20);
    previous_button_->setCursor(Qt::PointingHandCursor);

    next_button_->setStyleSheet(button_style);
    next_button_->setIconSize(QSize(20, 18));
    next_button_->setFixedSize(20, 20);
    next_button_->setCursor(Qt::PointingHandCursor);

    layout_ = new QHBoxLayout(this);

    layout_->addWidget(previous_button_);
    layout_->addWidget(branch_label_history_combo_);
    layout_->addWidget(next_button_);

    connect(previous_button_, &QPushButton::pressed, this, &IsaBranchLabelNavigationWidget::BackPressed);
    connect(next_button_, &QPushButton::pressed, this, &IsaBranchLabelNavigationWidget::ForwardPressed);
    connect(branch_label_history_combo_, &ArrowIconComboBox::SelectedItem, this, &IsaBranchLabelNavigationWidget::HistoryEntrySelected);
    connect(&QtCommon::QtUtils::ColorTheme::Get(), &QtCommon::QtUtils::ColorTheme::ColorThemeUpdated, this, &IsaBranchLabelNavigationWidget::SetButtonIcons);

    ClearHistory();

    SetButtonIcons();

    layout_->setAlignment(Qt::AlignTop);
    layout_->setContentsMargins(0, 0, 0, 0);
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

    previous_button_->setEnabled(false);
    next_button_->setEnabled(false);
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

    previous_button_->setEnabled(true);
}

bool IsaBranchLabelNavigationWidget::CanNavigateBack() const
{
    return previous_button_->isEnabled();
}

bool IsaBranchLabelNavigationWidget::CanNavigateForward() const
{
    return next_button_->isEnabled();
}

void IsaBranchLabelNavigationWidget::BackPressed()
{
    if (history_index_ <= 0)
    {
        return;
    }

    history_index_--;

    branch_label_history_combo_->SetSelectedRow(history_index_);

    const QModelIndex previous_source_index = branch_label_history_combo_->ItemData(history_index_, Qt::UserRole).toModelIndex();

    emit Navigate(previous_source_index);

    next_button_->setEnabled(true);
    previous_button_->setEnabled(history_index_ > 0);
}

void IsaBranchLabelNavigationWidget::ForwardPressed()
{
    if (history_index_ >= branch_label_history_combo_->RowCount() - 1)
    {
        return;
    }

    history_index_++;

    branch_label_history_combo_->SetSelectedRow(history_index_);

    const QModelIndex next_source_index = branch_label_history_combo_->ItemData(history_index_, Qt::UserRole).toModelIndex();

    emit Navigate(next_source_index);

    previous_button_->setEnabled(true);
    next_button_->setEnabled(history_index_ < branch_label_history_combo_->RowCount() - 1);
}

void IsaBranchLabelNavigationWidget::HistoryEntrySelected(QListWidgetItem* item)
{
    Q_UNUSED(item);

    history_index_ = branch_label_history_combo_->CurrentRow();

    const QModelIndex selected_entry_source_index = branch_label_history_combo_->ItemData(history_index_, Qt::UserRole).toModelIndex();

    emit Navigate(selected_entry_source_index);

    previous_button_->setEnabled(history_index_ > 0);
    next_button_->setEnabled(history_index_ < branch_label_history_combo_->RowCount() - 1);
}

void IsaBranchLabelNavigationWidget::TrimHistory()
{
    const int row_count = branch_label_history_combo_->RowCount();
    for (int i = row_count - 1; i > history_index_; i--)
    {
        branch_label_history_combo_->RemoveItem(i);
    }

    next_button_->setEnabled(false);
}

void IsaBranchLabelNavigationWidget::SetButtonIcons()
{
    if (QtCommon::QtUtils::ColorTheme::Get().GetColorTheme() == kColorThemeTypeDark)
    {
        previous_button_->setIcon(QIcon(":/icons/find_previous_icon_dark_mode.svg"));
        next_button_->setIcon(QIcon(":/icons/find_next_icon_dark_mode.svg"));
    }
    else
    {
        previous_button_->setIcon(QIcon(":/icons/find_previous_icon.svg"));
        next_button_->setIcon(QIcon(":/icons/find_next_icon.svg"));
    }
}
