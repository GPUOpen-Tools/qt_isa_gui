//=============================================================================
// Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for an isa tooltip.
//=============================================================================

#ifndef QTISAGUI_ISA_TOOLTIP_H_
#define QTISAGUI_ISA_TOOLTIP_H_

#include <QGridLayout>
#include <QLabel>
#include <QWidget>

#include "qt_common/custom_widgets/tooltip_widget.h"

#include "amdisa/isa_decoder.h"

/// @brief IsaTooltip is a widget that intends to function as a tooltip to show
///        extra information about isa.
class IsaTooltip : public TooltipWidget
{
    Q_OBJECT

public:
    /// @brief Constructor; set flags to turn this widget into a tooltip.
    ///
    /// @param [in] parent           The parent widget.
    /// @param [in] container_widget The widget that this tooltip should be confined to.
    IsaTooltip(QWidget* parent, QWidget* container_widget);

    /// @brief Destructor.
    ~IsaTooltip();

    /// @brief Update the text shown in this tooltip.
    ///
    /// @param [in] decoded_info The decoder's instruction info to use to update this tooltip.
    void UpdateText(const amdisa::InstructionInfo& decoded_info);

private:
    QLabel*      instruction_;        ///< Color coded op code name and its functional group.
    QLabel*      description_label_;  ///< "Description" label.
    QLabel*      description_;        ///< Op code description.
    QLabel*      encodings_;          ///< Op code encodings.
    QGridLayout* layout_;             ///< The tooltip's layout.
};

#endif  // QTISAGUI_ISA_TOOLTIP_H_
