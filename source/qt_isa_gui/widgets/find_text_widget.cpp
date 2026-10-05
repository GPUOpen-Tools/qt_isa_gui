//=============================================================================
// Copyright (c) 2020-2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for Find text widget.
//=============================================================================

#include "find_text_widget.h"

#include <cassert>
#include <memory>

#include <QLineEdit>
#include <QPushButton>

#include "qt_common/utils/qt_util.h"

#include "ui_find_text_widget.h"

/// @brief Initialize the qt_isa_gui resource file. Required for static libraries.
static void InitQtIsaGuiResources()
{
    Q_INIT_RESOURCE(qt_isa_gui_resources);
}

namespace
{
    const QString button_style =
        "QPushButton         { border: 1px solid palette(text); background: palette(button); }"
        "QPushButton:hover   { background-color: palette(alternate-base);                    }"
        "QPushButton:pressed { background-color: gray;                                       }"
        "QPushButton:checked { background-color: palette(highlight);                         }";
}  // namespace

FindTextWidget::FindTextWidget(QWidget* parent)
    : QWidget(parent)
    , ui_(std::make_unique<Ui::FindTextWidget>())
{
    InitQtIsaGuiResources();

    // Initialize the interface object.
    ui_->setupUi(this);

    ColorThemeType color_theme = QtCommon::QtUtils::ColorTheme::Get().GetColorTheme();

    // Add a Search magnifying glass to the search textbox.
    ui_->find_->setClearButtonEnabled(true);

    ui_->find_->setStyleSheet("QLineEdit { border: 1px solid gray; }");

    ui_->find_previous_->setStyleSheet(button_style);
    ui_->find_next_->setStyleSheet(button_style);
    ui_->match_case_->setStyleSheet(button_style);

    ui_->find_previous_->setIconSize(QSize(20, 18));
    ui_->find_next_->setIconSize(QSize(20, 18));
    ui_->match_case_->setIconSize(QSize(18, 18));

    ui_->find_previous_->setFixedSize(20, 20);
    ui_->find_next_->setFixedSize(20, 20);
    ui_->match_case_->setFixedSize(20, 20);

    if (color_theme == kColorThemeTypeDark)
    {
        ui_->find_previous_->setIcon(QIcon(":/icons/find_previous_icon_dark_mode.svg"));
        ui_->find_next_->setIcon(QIcon(":/icons/find_next_icon_dark_mode.svg"));
        ui_->find_->addAction(QIcon(":/icons/magnifying_glass_icon_dark_mode.svg"), QLineEdit::LeadingPosition);
    }
    else
    {
        ui_->find_->addAction(QIcon(":/icons/magnifying_glass_icon.svg"), QLineEdit::LeadingPosition);
    }

    // Create the keyboard actions associated with the Find widget.
    CreateActions();

    // Connect all internal signals to handler slots.
    ConnectSignals();

    // Set the cursor to pointing hand cursor.
    SetCursor();

    // Set focus proxies for filter and match case buttons to be the line edit,
    // so each time one of these buttons are clicked, the focus will remain with
    // the line edit.
    ui_->match_case_->setFocusProxy(ui_->find_);
}

FindTextWidget::~FindTextWidget() = default;

void FindTextWidget::SetFocused()
{
    // Focus on the line edit and select all existing text to make new searches quicker.
    ui_->find_->setFocus();
    ui_->find_->selectAll();

    // Update the search context options every time the search widget gets focus.
    UpdateSearchOptions();
}

void FindTextWidget::SetSearchContext(ISearchable* search_context)
{
    assert(search_context != nullptr);
    if (search_context != nullptr)
    {
        search_context_ = search_context;

        // Toggle the search option button visibility based on the searcher's supported options.
        uint32_t options = search_context_->GetSupportedOptions();
        ToggleOptionVisibility(options);
    }
}

void FindTextWidget::HideCloseButton()
{
    ui_->close_button_->hide();
}

void FindTextWidget::ToggleOptionVisibility(uint32_t options)
{
    ui_->find_previous_->setVisible((options & ISearchable::SupportedOptions::kFindPrevious) == ISearchable::SupportedOptions::kFindPrevious);
    ui_->find_next_->setVisible((options & ISearchable::SupportedOptions::kFindNext) == ISearchable::SupportedOptions::kFindNext);
    ui_->match_case_->setVisible((options & ISearchable::SupportedOptions::kMatchCase) == ISearchable::SupportedOptions::kMatchCase);
}

void FindTextWidget::SetSearchString(const std::string& search_string)
{
    // Insert the text into the search line edit.
    ui_->find_->setText(search_string.c_str());
}

void FindTextWidget::keyPressEvent(QKeyEvent* event)
{
    // If the user pressed the escape key, close the widget.
    if (event->key() == Qt::Key_Escape)
    {
        // Reset the search when the user closes the Find widget.
        if (search_context_ != nullptr)
        {
            search_context_->ResetSearch();
        }

        // Let the RgBuildView know that the Find widget has been closed.
        emit CloseWidgetSignal();
    }

    // Invoke the baseclass QWidget implementation of the key handler.
    QWidget::keyPressEvent(event);
}

void FindTextWidget::ConnectSignals()
{
    // Connect the find previous button to the associated action.
    assert(find_previous_action_ != nullptr);
    if (find_previous_action_ != nullptr)
    {
        // Connect the handler for the "Find previous" button.
        connect(ui_->find_previous_, &QPushButton::clicked, this, &FindTextWidget::HandleFindPreviousButtonClicked);

        // Add a hotkey action to the button.
        ui_->find_previous_->addAction(find_previous_action_);
    }

    // Connect the find next button to the associated action.
    assert(find_next_action_ != nullptr);
    if (find_next_action_ != nullptr)
    {
        // Connect the handler for the "Find next" button.
        connect(ui_->find_next_, &QPushButton::clicked, this, &FindTextWidget::HandleFindNextButtonClicked);

        // Add a hotkey action to the button.
        ui_->find_next_->addAction(find_next_action_);
    }

    // Connect the handler invoked when the user presses the "Enter" key while the search textbox is selected.
    connect(ui_->find_, &QLineEdit::returnPressed, this, &FindTextWidget::HandleReturnPressedOnSearch);

    connect(ui_->match_case_, &QPushButton::toggled, this, &FindTextWidget::HandleOptionButtonCheckChanged);

    connect(ui_->find_, &QLineEdit::textChanged, this, &FindTextWidget::HandleSearchTextChanged);

    // Connect the handler for the close button.
    connect(ui_->close_button_, &QPushButton::clicked, this, &FindTextWidget::HandleCloseButtonClicked);
}

void FindTextWidget::HandleCloseButtonClicked()
{
    // Reset the search results.
    if (search_context_ != nullptr)
    {
        search_context_->ResetSearch();
    }

    emit CloseWidgetSignal();
}

void FindTextWidget::HandleFindPreviousButtonClicked()
{
    // Use the search context to look for the previous match.
    assert(search_context_ != nullptr);
    if (search_context_ != nullptr)
    {
        search_context_->Find(ui_->find_->text(), ISearchable::SearchDirection::kPrevious);

        int current_match_index = 0;
        int total_matches       = 0;

        search_context_->GetSearchResults(current_match_index, total_matches);

        if (total_matches > 0)
        {
            ui_->find_results_->setText(QString("%1 of %2").arg(current_match_index).arg(total_matches));
        }
        else
        {
            ui_->find_results_->setText("No results");
        }
    }
}

void FindTextWidget::HandleFindNextButtonClicked()
{
    // Use the search context to look for the next match.
    assert(search_context_ != nullptr);
    if (search_context_ != nullptr)
    {
        search_context_->Find(ui_->find_->text(), ISearchable::SearchDirection::kNext);

        int current_match_index = 0;
        int total_matches       = 0;

        search_context_->GetSearchResults(current_match_index, total_matches);

        if (total_matches > 0)
        {
            ui_->find_results_->setText(QString("%1 of %2").arg(current_match_index).arg(total_matches));
        }
        else
        {
            ui_->find_results_->setText("No results");
        }
    }
}

void FindTextWidget::HandleReturnPressedOnSearch()
{
    // Return starts the next search.
    HandleFindNextButtonClicked();

    if (search_context_ != nullptr)
    {
        int current_match_index = 0;
        int total_matches       = 0;

        search_context_->GetSearchResults(current_match_index, total_matches);

        if (total_matches > 0)
        {
            ui_->find_results_->setText(QString("%1 of %2").arg(current_match_index).arg(total_matches));
        }
        else
        {
            ui_->find_results_->setText("No results");
        }
    }
}

void FindTextWidget::HandleOptionButtonCheckChanged(bool checked)
{
    Q_UNUSED(checked);

    // Update the search option flags in the search context.
    UpdateSearchOptions();

    if (search_context_ != nullptr)
    {
        int current_match_index = 0;
        int total_matches       = 0;

        search_context_->GetSearchResults(current_match_index, total_matches);

        if (total_matches > 0)
        {
            ui_->find_results_->setText(QString("%1 of %2").arg(current_match_index).arg(total_matches));
        }
        else
        {
            ui_->find_results_->setText("No results");
        }
    }
}

void FindTextWidget::HandleSearchTextChanged(const QString& updated_text)
{
    Q_UNUSED(updated_text);

    HandleReturnPressedOnSearch();
}

void FindTextWidget::CreateActions()
{
    // Create the "Find next" action.
    find_next_action_ = new QAction("Find next.", this);
    assert(find_next_action_ != nullptr);
    if (find_next_action_ != nullptr)
    {
        // Configure the hotkey for the Find next action.
        find_next_action_->setShortcut(QKeySequence("F3"));

        // Connect the handler for the "Find next" button.
        connect(find_next_action_, &QAction::triggered, this, &FindTextWidget::HandleFindNextButtonClicked);
    }

    // Create the "Find previous" action.
    find_previous_action_ = new QAction("Find previous", this);
    assert(find_previous_action_ != nullptr);
    if (find_previous_action_ != nullptr)
    {
        // Configure the hotkey for the Find previous action.
        find_previous_action_->setShortcut(QKeySequence("Shift+F3"));

        // Connect the handler for the "Find previous" button.
        connect(find_previous_action_, &QAction::triggered, this, &FindTextWidget::HandleFindPreviousButtonClicked);
    }
}

void FindTextWidget::SetCursor()
{
    // Set the hand pointers for the buttons.
    ui_->match_case_->setCursor(Qt::PointingHandCursor);
    ui_->find_next_->setCursor(Qt::PointingHandCursor);
    ui_->find_previous_->setCursor(Qt::PointingHandCursor);
}

void FindTextWidget::UpdateSearchOptions()
{
    assert(search_context_ != nullptr);
    if (search_context_ != nullptr)
    {
        // Update the search options by sending the option button states to the search context.
        search_context_->SetSearchOptions({false, ui_->match_case_->isChecked()});

        // Trigger the search.
        search_context_->ResetSearch();
        search_context_->Find(ui_->find_->text(), ISearchable::SearchDirection::kNext);
    }
}
