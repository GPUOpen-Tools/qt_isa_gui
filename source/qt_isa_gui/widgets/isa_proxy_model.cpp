//=============================================================================
// Copyright (c)2022-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Isa proxy model implementation.
//=============================================================================

#include "isa_proxy_model.h"

#include <QGridLayout>

#include "isa_item_model.h"

IsaProxyModel::IsaProxyModel(QObject* parent, std::vector<bool> columns_visiblity)
    : QSortFilterProxyModel(parent)
{
    Q_ASSERT(columns_visiblity.size() >= IsaItemModel::kColumnCount);
    for (int i = 0; i < IsaItemModel::kColumnCount; i++)
    {
        visible_columns_[i] = columns_visiblity[i];

        column_order_[i] = i;
    }
}

IsaProxyModel::~IsaProxyModel()
{
}

void IsaProxyModel::SetColumnVisibility(uint32_t column, bool visibility, QHeaderView* header)
{
    if (column >= visible_columns_.size())
    {
        return;
    }

    visible_columns_[column] = visibility;

    if (!visibility && header != nullptr)
    {
        int proxy_column = mapFromSource(sourceModel()->index(0, column)).column();
        int visual_index = header->visualIndex(proxy_column);

        column_order_[column] = visual_index;
    }

    invalidateFilter();

    if (visibility && header != nullptr)
    {
        int proxy_column = mapFromSource(sourceModel()->index(0, column)).column();
        if (column_order_[column] >= columnCount())
        {
            column_order_[column] = columnCount() - 1;
        }
        header->moveSection(proxy_column, column_order_[column]);
    }
}

void IsaProxyModel::CreateViewingOptionsCheckbox(uint32_t column, QWidget* parent)
{
    if (column > IsaItemModel::kLineNumber && column < IsaItemModel::kColumnCount)
    {
        auto source_model = sourceModel();
        if (source_model)
        {
            QString column_name                 = source_model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();
            viewing_options_checkboxes_[column] = new QCheckBox(column_name, parent);
            QCheckBox* checkbox                 = viewing_options_checkboxes_[column];
            if (checkbox)
            {
                QSizePolicy size_policy(QSizePolicy::Policy::Fixed, QSizePolicy::Policy::Fixed);
                checkbox->setSizePolicy(size_policy);

                if (parent && parent->layout())
                {
                    QGridLayout* grid_layout = qobject_cast<QGridLayout*>(parent->layout());
                    if (grid_layout)
                    {
                        switch (column)
                        {
                        case IsaItemModel::kPcAddress:
                            grid_layout->addWidget(checkbox, 1, 0);
                            break;
                        case IsaItemModel::kOpCode:
                            grid_layout->addWidget(checkbox, 1, 1);
                            break;
                        case IsaItemModel::kOperands:
                            grid_layout->addWidget(checkbox, 2, 1);
                            break;
                        case IsaItemModel::kBinaryRepresentation:
                            grid_layout->addWidget(checkbox, 2, 0);
                            break;
                        default:
                            break;
                        }
                    }
                }

                checkbox->setCursor(Qt::PointingHandCursor);

                checkbox->setChecked(visible_columns_[column]);
            }
        }
    }
}

const QCheckBox* IsaProxyModel::GetViewingOptionsCheckbox(uint32_t column)
{
    if (column < IsaItemModel::kColumnCount)
    {
        return viewing_options_checkboxes_[column];
    }
    return nullptr;
}

uint32_t IsaProxyModel::GetSourceColumnIndex(const QCheckBox* checkbox)
{
    uint32_t source_column_index = IsaItemModel::kColumnCount;

    for (uint32_t column = 0; column < IsaItemModel::kColumnCount; column++)
    {
        if (checkbox == GetViewingOptionsCheckbox(column))
        {
            source_column_index = column;
        }
    }

    return source_column_index;
}

uint32_t IsaProxyModel::GetNumberOfViewingOptions()
{
    return IsaItemModel::kColumnCount;
}

bool IsaProxyModel::filterAcceptsColumn(int source_column, const QModelIndex& source_parent) const
{
    Q_UNUSED(source_parent);

    if (source_column >= static_cast<int>(visible_columns_.size()))
    {
        return true;
    }

    return visible_columns_[source_column];
}