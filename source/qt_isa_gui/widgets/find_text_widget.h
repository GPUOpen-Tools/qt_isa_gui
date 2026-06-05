//=============================================================================
// Copyright (c) 2020-2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Header for Find text widget.
//=============================================================================

#ifndef QTISAGUI_WIDGETS_FIND_TEXT_WIDGET_H_
#define QTISAGUI_WIDGETS_FIND_TEXT_WIDGET_H_

#include <memory>

#include <QAction>
#include <QKeyEvent>
#include <QWidget>

namespace Ui
{
    class FindTextWidget;
}

/// @brief An interface that the FindTextWidget can use to search multiple sources.
class ISearchable
{
public:
    ISearchable()          = default;
    virtual ~ISearchable() = default;

    /// @brief The direction to search in.
    enum class SearchDirection
    {
        kPrevious,
        kNext
    };

    /// @brief A flag enumeration of supported search options.
    enum SupportedOptions : char
    {
        kFindPrevious = 1 << 0,
        kFindNext     = 1 << 1,
        kFilterTree   = 1 << 2,
        kMatchCase    = 1 << 3,
        kUseRegex     = 1 << 4,
    };

    /// @brief A structure of search option flags.
    struct SearchOptions
    {
        bool filter_tree = false;
        bool match_case  = false;
        bool use_regex   = false;
    };

    /// @brief Get the supported search option flags.
    ///
    /// @return The supported search option flags.
    virtual uint32_t GetSupportedOptions() = 0;

    /// @brief Get the search results.
    ///
    /// @param [out] match_index   The current match index.
    /// @param [out] total_matches The total number of matches.
    virtual void GetSearchResults(int& match_index, int& total_matches) = 0;

    /// @brief Find the given string in the search target. After finding the first result, the direction is taken into account.
    ///
    /// @param [in] search_string The string to search for.
    /// @param [in] direction     The direction to search in.
    ///
    /// @return true if the string was found, false otherwise.
    virtual bool Find(const QString& search_string, SearchDirection direction) = 0;

    /// @brief Reset the current search results.
    virtual void ResetSearch() = 0;

    /// @brief Select the current results.
    virtual void SelectResults() = 0;

    /// @brief Set the search options.
    ///
    /// @param [in] options The search options to set.
    virtual void SetSearchOptions(const SearchOptions& options) = 0;
};

/// @brief A widget used to search for text in a searchable view.
class FindTextWidget : public QWidget
{
    Q_OBJECT

public:
    /// @brief Constructor.
    ///
    /// @param [in] parent The parent widget.
    explicit FindTextWidget(QWidget* parent = nullptr);

    /// @brief Destructor.
    virtual ~FindTextWidget();

    /// @brief A custom keypress handler used to close the widget.
    ///
    /// @param [in] event The key press event.
    virtual void keyPressEvent(QKeyEvent* event) override;

    /// @brief Set the focus to the search textbox within the FindTextWidget.
    void SetFocused();

    /// @brief Set the search content for the find widget.
    ///
    /// @param [in] search_context The search context to use.
    void SetSearchContext(ISearchable* search_context);

    /// @brief Set the text to search for.
    ///
    /// @param [in] search_string The text to search for.
    void SetSearchString(const std::string& search_string);

    /// @brief Update the visibility of the search options buttons, based on the operations supported by the current searcher implementation.
    ///
    /// @param [in] options The supported search options.
    void ToggleOptionVisibility(uint32_t options);

    /// @brief Hide the close button.
    void HideCloseButton();

signals:
    /// @brief A signal emitted when the widget should be hidden from view.
    void CloseWidgetSignal();

public slots:
    /// @brief Handler invoked when the user clicks the "Find previous" button.
    void HandleFindPreviousButtonClicked();

    /// @brief Handler invoked when the user clicks the "Find next" button.
    void HandleFindNextButtonClicked();

    /// @brief Handler invoked when the user clicks the Close button.
    void HandleCloseButtonClicked();

    /// @brief Handler invoked when the user pressed the "Enter" key with the search textbox selected.
    void HandleReturnPressedOnSearch();

    /// @brief Handler invoked when the user changes the check state of the "filter rows" option button.
    ///
    /// @param [in] checked The new checked state.
    void HandleOptionButtonCheckChanged(bool checked);

    /// @brief Handler invoked whenever the search box text changes.
    ///
    /// @param [in] updated_text The new text in the search box.
    void HandleSearchTextChanged(const QString& updated_text);

private:
    /// @brief Connect signals within the widget.
    void ConnectSignals();

    /// @brief Create the keyboard shortcut actions.
    void CreateActions();

    /// @brief Set the cursor to pointing hand cursor.
    void SetCursor();

    /// @brief Update the options in the search context.
    void UpdateSearchOptions();

    ISearchable*                        search_context_       = nullptr;  ///< An interface used to search a target widget.
    QAction*                            find_previous_action_ = nullptr;  ///< The action used to find the previous search result.
    QAction*                            find_next_action_     = nullptr;  ///< The action used to find the next search result.
    std::unique_ptr<Ui::FindTextWidget> ui_;                              ///< The generated find widget view object.
};
#endif  // QTISAGUI_WIDGETS_FIND_TEXT_WIDGET_H_
