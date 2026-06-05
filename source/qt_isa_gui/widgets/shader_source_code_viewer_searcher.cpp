//=============================================================================
// Copyright (c) 2020-2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for searching a shader source code viewer using a FindTextWidget.
//=============================================================================

// C++.
#include <cassert>

#include <QSignalBlocker>

// Local.
#include "shader_source_code_viewer.h"
#include "shader_source_code_viewer_searcher.h"

// The value used to represent the state where a search doesn't return any results.
static const int kInvalidSearchLocation = -1;

ShaderSourceCodeViewerSearcher::ShaderSourceCodeViewerSearcher()
{
}

uint32_t ShaderSourceCodeViewerSearcher::GetSupportedOptions()
{
    return SupportedOptions::kFindNext | SupportedOptions::kFindPrevious | SupportedOptions::kMatchCase;
}

bool ShaderSourceCodeViewerSearcher::Find(const QString& search_string, SearchDirection direction)
{
    bool ret = false;

    if (target_editor_ == nullptr)
    {
        return false;
    }

    if (search_string.isEmpty())
    {
        ResetSearch();
    }
    else
    {
        // The search string has changed since the last search. Search with the new string.
        if (search_string.compare(last_search_string_.c_str()) != 0)
        {
            // Reset the search results.
            ResetSearch();

            // Attempt to find new results.
            ret = FindResults(search_string);
        }
        else
        {
            ret = !result_indices_.empty();
        }
    }

    // Are there results to step through?
    if (ret && !result_indices_.empty())
    {
        if (direction == ISearchable::SearchDirection::kNext)
        {
            // Step to the next result. Loop back around to the first result if necessary.
            last_found_position_++;
            if (last_found_position_ >= static_cast<int>(result_indices_.size()))
            {
                last_found_position_ = 0;
            }
        }
        else if (direction == ISearchable::SearchDirection::kPrevious)
        {
            // Step to the previous result. Loop back around to the last result if necessary.
            last_found_position_--;
            if (last_found_position_ < 0)
            {
                last_found_position_ = static_cast<int>(result_indices_.size()) - 1;
            }
        }
        else
        {
            assert(false);
        }

        // Verify that the result index is valid.
        bool is_valid_result_index = last_found_position_ >= 0 && last_found_position_ < static_cast<int>(result_indices_.size());
        assert(is_valid_result_index);
        if (is_valid_result_index)
        {
            // Find the location of the current result.
            size_t occurrence_position = result_indices_[last_found_position_];

            // Select the result in the editor.
            QTextCursor current_cursor = target_editor_->textCursor();
            current_cursor.setPosition(static_cast<int>(occurrence_position));
            current_cursor.setPosition(static_cast<int>(occurrence_position) + static_cast<int>(search_string.size()), QTextCursor::KeepAnchor);
            SetCodeEditorCursor(current_cursor);
        }
    }

    return ret;
}

void ShaderSourceCodeViewerSearcher::ResetSearch()
{
    // Reset the last found position.
    last_found_position_ = kInvalidSearchLocation;

    // Reset the last search string.
    last_search_string_.clear();

    // Clear the result indices.
    result_indices_.clear();

    // Clear the current selection, since the search is over.
    if (target_editor_ != nullptr)
    {
        QTextCursor cleared_selection = target_editor_->textCursor();
        cleared_selection.clearSelection();
        SetCodeEditorCursor(cleared_selection);
    }
}

void ShaderSourceCodeViewerSearcher::SetSearchOptions(const SearchOptions& options)
{
    search_options_ = options;
}

void ShaderSourceCodeViewerSearcher::SetTargetEditor(ShaderSourceCodeViewer* target_editor)
{
    // If the target editor is changing, reset any stale search state from the previous editor.
    if (target_editor_ != target_editor)
    {
        ResetSearch();
    }

    // Set the target editor that will be searched.
    target_editor_ = target_editor;
}

void ShaderSourceCodeViewerSearcher::GetSearchResults(int& match_index, int& total_matches)
{
    match_index   = last_found_position_ + 1;  // Start at 1 not 0.
    total_matches = static_cast<int>(result_indices_.size());
}

bool ShaderSourceCodeViewerSearcher::FindResults(const QString& search_string)
{
    bool ret = false;

    assert(target_editor_ != nullptr);
    if (target_editor_ != nullptr)
    {
        // Search the target source editor for the search text.
        if (!search_string.isEmpty())
        {
            const QString& text = target_editor_->toPlainText();

            // Create a search predicate that search using the case sensitivity option.
            if (!search_options_.match_case)
            {
                // When case insensitive, convert the search string and document text to lowercase.
                QString lowercase_text = text;
                lowercase_text         = lowercase_text.toLower();

                QString lowercase_text_to_find = search_string;
                lowercase_text_to_find         = lowercase_text_to_find.toLower();

                // Perform the search on the converted text.
                ShaderSourceCodeViewer::FindSearchResultIndices(lowercase_text, lowercase_text_to_find, result_indices_);
            }
            else
            {
                // Search the text for the given search string.
                ShaderSourceCodeViewer::FindSearchResultIndices(text, search_string, result_indices_);
            }

            // If any results were found, store the search string.
            if (!result_indices_.empty())
            {
                last_search_string_ = search_string.toStdString();
                ret                 = true;
            }
        }
    }

    return ret;
}

void ShaderSourceCodeViewerSearcher::SetCodeEditorCursor(const QTextCursor& cursor)
{
    assert(target_editor_ != nullptr);
    if (target_editor_ != nullptr)
    {
        // Disable signals emitted by the Code Editor to prevent switching kernel/correlation context.
        // The signal block will be released right after getting out of scope of this function.
        QSignalBlocker blocker(target_editor_);

        // Set cursor for the current Code Editor.
        target_editor_->setTextCursor(cursor);
    }
}