//=============================================================================
// Copyright (c) 2020-2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Header for searching a shader source code viewer using a FindTextWidget.
//=============================================================================

#ifndef QTISAGUI_WIDGETS_SHADER_SOURCE_CODE_VIEWER_SEARCHER_H_
#define QTISAGUI_WIDGETS_SHADER_SOURCE_CODE_VIEWER_SEARCHER_H_

#include <string>
#include <vector>

#include <QTextCursor>

#include "find_text_widget.h"
#include "shader_source_code_viewer.h"

/// @brief An object responsible for searching a shader source code viewer using a FindTextWidget.
class ShaderSourceCodeViewerSearcher : public ISearchable
{
public:
    /// @brief Constructor.
    ShaderSourceCodeViewerSearcher();

    /// @brief Destructor.
    virtual ~ShaderSourceCodeViewerSearcher() = default;

    /// @brief Get the supported search option flags.
    ///
    /// @return The supported search option flags.
    virtual uint32_t GetSupportedOptions() override;

    /// @brief Find the given string in the search target view. After finding the first result, the direction is taken into account.
    ///
    /// @param [in] search_string The string to search for.
    /// @param [in] direction     The direction to search in.
    ///
    /// @return true if the string was found, false otherwise.
    virtual bool Find(const QString& search_string, SearchDirection direction) override;

    /// @brief Reset the current search.
    virtual void ResetSearch() override;

    /// @brief Select the current search result in the source editor.
    virtual void SelectResults() override
    {
    }

    /// @brief Set the search options.
    ///
    /// @param [in] options The search options to set.
    virtual void SetSearchOptions(const SearchOptions& options) override;

    /// @brief Set the target source editor being searched.
    ///
    /// @param [in] target_editor The source editor to search.
    void SetTargetEditor(ShaderSourceCodeViewer* target_editor);

    /// @brief Get the search results.
    ///
    /// @param [out] match_index   The current match index.
    /// @param [out] total_matches The total number of matches.
    void GetSearchResults(int& match_index, int& total_matches);

private:
    /// @brief Find all instances of the search string.
    ///
    /// @param [in] search_string The string to search for.
    ///
    /// @return true if results were found, false otherwise.
    bool FindResults(const QString& search_string);

    /// @brief Set cursor for the current editor without changing kernel/correlation context.
    ///
    /// @param [in] cursor The text cursor to set.
    void SetCodeEditorCursor(const QTextCursor& cursor);

    std::vector<size_t>        result_indices_;                 ///< A vector containing the character indices of the search results.
    std::string                last_search_string_;             ///< The last search string.
    int                        last_found_position_ = -1;       ///< The location in the document of the last search result.
    ISearchable::SearchOptions search_options_      = {};       ///< Search option flags used to alter search criteria.
    ShaderSourceCodeViewer*    target_editor_       = nullptr;  ///< The source editor instance whose text is being searched.
};

#endif  // QTISAGUI_WIDGETS_SHADER_SOURCE_CODE_VIEWER_SEARCHER_H_
