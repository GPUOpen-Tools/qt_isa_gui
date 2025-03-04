//=============================================================================
// Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for an isa tooltip.
//=============================================================================

#include "isa_tooltip.h"

#ifdef Q_OS_LINUX
#include <QApplication>
#endif
#include <QGuiApplication>
#ifdef Q_OS_LINUX
#include <QMainWindow>
#endif
#include <QScreen>
#include <QSizePolicy>
#include <QString>

#include "qt_common/utils/qt_util.h"

#include "qt_isa_gui/utility/isa_dictionary.h"
#include "qt_isa_gui/widgets/isa_tree_view.h"

namespace
{
    static const int     kMousePositionBuffer = 15;   ///< Position tooltips in front of the mouse.
    static const int     kMaxTooltipWidth     = 500;  ///< Maximum pixel width for the tooltip label.
    static const int     kTooltipBorderWidth  = 1;    ///< Border around the tooltip to distinguish it from what's behind it.
    static const int     kTooltipMargin       = 2;    ///< Margin for the tooltip to give some space between its contents and the border.
    static const QString kTooltipStylesheet   = QString("IsaTooltip > QWidget#background_widget_ { border: %1px solid palette(text); }")
                                                  .arg(QString::number(kTooltipBorderWidth));  ///< Make border match color theme.
}  // namespace

IsaTooltip::IsaTooltip(QWidget* parent)
    : QWidget(parent)
    , instruction_(nullptr)
    , description_label_(nullptr)
    , description_(nullptr)
    , encodings_(nullptr)
    , background_widget_(nullptr)
    , layout_(nullptr)
{
    // Tell the tooltip to paint the background for widgets contained inside it. The tooltip is sometimes transparent otherwise.
    setAutoFillBackground(true);

    // Containerize the contents of the tooltip in a dummy widget to allow manipulating the style of the tooltip.
    QVBoxLayout* background_layout = new QVBoxLayout;
    background_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(background_layout);

    background_widget_ = new QWidget(this);
    background_widget_->setObjectName("background_widget_");
    background_widget_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    background_layout->addWidget(background_widget_);

    layout_ = new QGridLayout;
    layout_->setContentsMargins(kTooltipMargin, kTooltipMargin, kTooltipMargin, kTooltipMargin);
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

    // This widget's size should not change.
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Only show this widget when mouse is over an op code.
    hide();

    // This widget should not get focus.
    setFocusPolicy(Qt::NoFocus);

    // This widget should behave like a tooltip.
    setWindowFlag(Qt::ToolTip, true);

    // This widget should never be the active window; the main window should remain the active window.
    setAttribute(Qt::WA_ShowWithoutActivating);

    connect(&QtCommon::QtUtils::ColorTheme::Get(), &QtCommon::QtUtils::ColorTheme::ColorThemeUpdated, this, [this]() { setStyleSheet(kTooltipStylesheet); });

    setStyleSheet(kTooltipStylesheet);
}

IsaTooltip::~IsaTooltip()
{
}

void IsaTooltip::UpdatePosition(QPoint new_global_position)
{
    // Check if we need to modify the position so this widget stays visible.
    bool           clipped          = false;
    const auto     tooltip_geometry = frameGeometry();
    const auto     tooltip_end_x    = new_global_position.x() + tooltip_geometry.width();
    const auto     tooltip_end_y    = new_global_position.y() + tooltip_geometry.height();
    const QScreen* current_screen   = QGuiApplication::screenAt(new_global_position);

    if (current_screen != nullptr)
    {
        const QRect screen_geometry = current_screen->availableGeometry();

        const auto screen_end_x = screen_geometry.x() + screen_geometry.width();
        const auto screen_end_y = screen_geometry.y() + screen_geometry.height();

        if (tooltip_end_x > screen_end_x)
        {
            // Cursor too close to right edge; prevent this widget from being clipped.
            const auto adjusted_x = new_global_position.x() - tooltip_geometry.width();
            new_global_position.setX(adjusted_x);
            clipped = true;
        }

        if (tooltip_end_y > screen_end_y)
        {
            // Cursor too close to bottom edge; prevent this widget from being clipped.
            const auto adjusted_y = new_global_position.y() - tooltip_geometry.height();
            new_global_position.setY(adjusted_y);
        }
    }

    const auto local_position  = this->mapFromGlobal(new_global_position);
    auto       parent_position = this->mapToParent(local_position);

    parent_position.setX(clipped ? (parent_position.x() - kMousePositionBuffer) : (parent_position.x() + kMousePositionBuffer));

    move(parent_position);
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
    description_label_width += layout_->horizontalSpacing() + (kTooltipBorderWidth * 2) + (kTooltipMargin * 2);

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

void IsaTooltip::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);

    IsaTreeView* tree = qobject_cast<IsaTreeView*>(parent());  // Rely on tree being parent.

    if (tree != nullptr)
    {
        const auto tree_local_position = tree->mapFromGlobal(QCursor::pos());
        const auto tree_geometry       = tree->viewport()->geometry();

        if (!tree_geometry.contains(tree_local_position))
        {
            hide();
        }
    }
}