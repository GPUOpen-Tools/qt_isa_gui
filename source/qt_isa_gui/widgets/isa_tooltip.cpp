//=============================================================================
// Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for an isa tooltip.
//=============================================================================

#include "isa_tooltip.h"

#include <QSizePolicy>
#include <QString>

#include "qt_common/utils/qt_util.h"

#include "qt_isa_gui/utility/isa_dictionary.h"
#include "qt_isa_gui/widgets/isa_tree_view.h"

namespace
{
    static const int kMaxTooltipWidth = 500;  ///< Maximum pixel width for the tooltip label.
}  // namespace

IsaTooltip::IsaTooltip(QWidget* parent, QWidget* container_widget)
    : TooltipWidget(parent, false, container_widget)
    , instruction_(nullptr)
    , description_label_(nullptr)
    , description_(nullptr)
    , encodings_(nullptr)
    , layout_(nullptr)
{
    layout_ = new QGridLayout;
    layout_->setContentsMargins(TooltipWidget::kTooltipMargin, TooltipWidget::kTooltipMargin, TooltipWidget::kTooltipMargin, TooltipWidget::kTooltipMargin);
    background_widget_->setLayout(layout_);

    QLabel* instruction_label = new QLabel("Instruction:", background_widget_);
    auto    font              = instruction_label->font();
    font.setBold(true);
    instruction_label->setFont(font);
    instruction_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout_->addWidget(instruction_label, 0, 0, Qt::AlignmentFlag::AlignLeft | Qt::AlignmentFlag::AlignTop);

    description_label_ = new QLabel("Description:", background_widget_);
    font               = description_label_->font();
    font.setBold(true);
    description_label_->setFont(font);
    description_label_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout_->addWidget(description_label_, 1, 0, Qt::AlignmentFlag::AlignLeft | Qt::AlignmentFlag::AlignTop);

    QLabel* encodings_label = new QLabel("Encodings:", background_widget_);
    font                    = encodings_label->font();
    font.setBold(true);
    encodings_label->setFont(font);
    encodings_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout_->addWidget(encodings_label, 2, 0, Qt::AlignmentFlag::AlignLeft | Qt::AlignmentFlag::AlignTop);

    instruction_ = new QLabel;
    instruction_->setTextFormat(Qt::RichText);
    layout_->addWidget(instruction_, 0, 1);

    description_ = new QLabel(background_widget_);
    description_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout_->addWidget(description_, 1, 1, Qt::AlignmentFlag::AlignLeft | Qt::AlignmentFlag::AlignTop);

    encodings_ = new QLabel(background_widget_);
    encodings_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout_->addWidget(encodings_, 2, 1, Qt::AlignmentFlag::AlignLeft | Qt::AlignmentFlag::AlignTop);
}

IsaTooltip::~IsaTooltip()
{
}

void IsaTooltip::UpdateText(const amdisa::InstructionInfo& decoded_info)
{
    const auto    op_code               = QString(decoded_info.instruction_name.c_str()).toLower().toStdString();
    const auto    functional_group      = decoded_info.functional_group_subgroup_info.IsaFunctionalGroup;
    const QString functional_group_name = amdisa::kFunctionalGroupName[static_cast<int>(functional_group)];
    const QString description           = decoded_info.instruction_description.c_str();
    const QString encodings             = decoded_info.encoding_name.c_str();

    // Color code the op code.
    QColor op_code_color;
    IsaColorCodingDictionaryInstance::GetInstance().ShouldHighlight(op_code, op_code_color);

    const auto    r_g_b             = QString("rgb(%1, %2, %3)").arg(op_code_color.red()).arg(op_code_color.green()).arg(op_code_color.blue());
    const QString rich_text_op_code = QString("<font style='color:%1'>" + QString(op_code.c_str())).arg(r_g_b) + "</font> (" + functional_group_name + ")";
    const QString op_code_qstring   = QString(op_code.c_str()) + " (" + functional_group_name + ")";

    // Make a reasonable effort to fit the tooltip to its text content without exceeding a maximum width.
    int largest_width = description_->fontMetrics().horizontalAdvance(QString(description));
    largest_width     = std::max(largest_width, instruction_->fontMetrics().horizontalAdvance(op_code_qstring));
    largest_width     = std::max(largest_width, instruction_->fontMetrics().horizontalAdvance(encodings));

    // Get the width of the description label and spacing.
    int description_label_width = description_label_->fontMetrics().horizontalAdvance(description_label_->text());
    description_label_width += layout_->horizontalSpacing() + (TooltipWidget::kTooltipBorder * 2) + (kTooltipMargin * 2);

    const bool word_wrap        = largest_width > kMaxTooltipWidth;
    const int  fixed_width      = word_wrap ? kMaxTooltipWidth : QWIDGETSIZE_MAX;
    int        background_width = description_label_width;

    background_width += word_wrap ? kMaxTooltipWidth : largest_width;

    instruction_->setText(rich_text_op_code);

    description_->setText(description);
    description_->setWordWrap(word_wrap);
    description_->setFixedWidth(fixed_width);

    background_widget_->setFixedWidth(background_width);

    encodings_->setText(encodings);

    background_widget_->adjustSize();
    adjustSize();
}