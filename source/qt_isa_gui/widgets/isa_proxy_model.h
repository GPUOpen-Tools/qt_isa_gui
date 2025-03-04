//=============================================================================
// Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Header for an isa proxy model.
//=============================================================================

#ifndef QTISAGUI_ISA_PROXY_MODEL_H_
#define QTISAGUI_ISA_PROXY_MODEL_H_

#include <array>

#include <QCheckBox>
#include <QHeaderView>
#include <QSortFilterProxyModel>

#include "isa_item_model.h"

/// @brief IsaProxyModel is a filter model meant to filter default columns for an IsaItemModel.
///
/// It filters out IsaItemModel columns set to be invisible via the gui.
class IsaProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    /// @brief Constructor; default all columns to visible.
    ///
    /// @param [in] parent The parent object.
    /// @param [in] columns_visiblity Vector to specify column visibilty. kPcAddress and kBinaryRepresentation are hidden by default.
    explicit IsaProxyModel(QObject* parent = nullptr, std::vector<bool> columns_visiblity = {true, false, true, true, false});

    /// @brief Destructor.
    virtual ~IsaProxyModel();

    /// @brief Change the visibility of a column and invalidate this model.
    ///
    /// @param [in] column     The IsaItemModel column to change.
    /// @param [in] visibility true to make the column visible, false to make it invisible.
    virtual void SetColumnVisibility(uint32_t column, bool visibility, QHeaderView* header = nullptr);

    // @brief Create the visibility checkbox related to a column.
    ///
    /// @param [in] column The IsaItemModel column.
    /// @param [in] parent The parent widget for the created checkbox.
    virtual void CreateViewingOptionsCheckbox(uint32_t column, QWidget* parent = nullptr);

    /// @brief Get the visibility checkbox related to a column.
    ///
    /// @param [in] column The IsaItemModel column.
    ///
    /// @return visibility checkbox related to a column, nullptr otherwise.
    virtual const QCheckBox* GetViewingOptionsCheckbox(uint32_t column);

    /// @brief Get the source column index related to the checkbox.
    ///
    /// @param [in] checkbox The IsaItemModel column viewing options checkbox.
    ///
    /// @return source column index for the checkbox, GetNumberOfViewingOptions() otherwise.
    virtual uint32_t GetSourceColumnIndex(const QCheckBox* checkbox);

    /// @brief Get the number of checkboxes in this model.
    ///
    /// @return Total number of columns in this model.
    virtual uint32_t GetNumberOfViewingOptions();

protected:
    /// @brief Override filterAcceptsColumn to filter columns set to be invisible.
    ///
    /// @param [in] source_column The source column.
    /// @param [in] source_parent The source parent index.
    ///
    /// @return true if the column is marked as visible, false otherwise.
    virtual bool filterAcceptsColumn(int source_column, const QModelIndex& source_parent) const Q_DECL_OVERRIDE;

private:
    std::array<bool, IsaItemModel::kColumnCount>       visible_columns_;             ///< Keep track of which columns should be visible.
    std::array<QCheckBox*, IsaItemModel::kColumnCount> viewing_options_checkboxes_;  ///< Corresponding checkboxes to each column.
    int column_order_[IsaItemModel::kColumnCount];  ///< Keep track of where a hidden column should be placed when it is reshown.
};

#endif  // QTISAGUI_ISA_PROXY_MODEL_H_
