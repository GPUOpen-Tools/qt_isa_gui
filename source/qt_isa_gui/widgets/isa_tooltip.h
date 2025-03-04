//=============================================================================
// Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for an isa tooltip.
//=============================================================================

#ifndef QTISAGUI_ISA_TOOLTIP_H_
#define QTISAGUI_ISA_TOOLTIP_H_

#include <string>

#include <QGridLayout>
#include <QLabel>
#include <QPoint>
#include <QWidget>

#include "amdisa/isa_decoder.h"

/// @brief IsaTooltip is a widget that intends to function as a tooltip to show
///        extra information about isa.
class IsaTooltip : public QWidget
{
    Q_OBJECT

public:
    static const int kTooltipDelayMs = 600;  ///< Delay showing the tooltip after a mouse position to isa token collision.

    /// @brief Constructor; set flags to turn this widget into a tooltip.
    ///
    /// @param [in] parent The parent widget.
    IsaTooltip(QWidget* parent = nullptr);

    /// @brief Destructor.
    ~IsaTooltip();

    /// @brief Move this tooltip to a new position.
    ///
    /// @param [in] new_global_position The global position to move to.
    void UpdatePosition(QPoint new_global_position);

    /// @brief Update the text shown in this tooltip.
    ///
    /// @param [in] decoded_info The decoder's instruction info to use to update this tooltip.
    void UpdateText(const amdisa::InstructionInfo& decoded_info);

protected:
    /// @brief Override leave to make sure this tooltip is not visible if the mouse leaves its tree view.
    ///
    /// @param [in] event The leave event.
    virtual void leaveEvent(QEvent* event) Q_DECL_OVERRIDE;

private:
    QLabel*      instruction_;        ///< Color coded op code name and its functional group.
    QLabel*      description_label_;  ///< "Description" label.
    QLabel*      description_;        ///< Op code description.
    QLabel*      encodings_;          ///< Op code encodings.
    QWidget*     background_widget_;  ///< The container widget to ensure background is not transparent.
    QGridLayout* layout_;             ///< The tooltip's layout.
};

#endif  // QTISAGUI_ISA_TOOLTIP_H_
