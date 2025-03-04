//=============================================================================
// Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Header for an isa tree vertical scroll bar.
//=============================================================================

#ifndef QTISAGUI_ISA_VERTICAL_SCROLL_BAR_H_
#define QTISAGUI_ISA_VERTICAL_SCROLL_BAR_H_

#include <QScrollBar>

#include <set>

/// @brief IsaVerticalScrollBar is a scroll bar that custom paints the relative position
///        of hot spots and text search matches as red and purple rectangles, inside of the scroll bar.
class IsaVerticalScrollBar final : public QScrollBar
{
    Q_OBJECT

public:
    /// @brief Constructor.
    ///
    /// @param [in] parent The parent widget.
    explicit IsaVerticalScrollBar(QWidget* parent = nullptr);

    /// @brief Destructor.
    virtual ~IsaVerticalScrollBar();

    /// @brief Set the line #(s) of hot spots to paint a red rectangle.
    ///
    /// @param [in] line_number The line #(s) of the hot spots.
    void SetHotSpotLineNumbers(const std::set<int>& line_numbers);

    /// @brief Set the line #(s) of text search matches to paint a purple rectangle(s).
    ///
    /// @param [in] line_numbers The line #(s) of the text search matches.
    void SetSearchMatchLineNumbers(const std::set<int>& line_numbers);

protected:
    /// @brief Override paint to paint red hot spots and purple text search matches.
    ///
    /// @param [in] The paint event.
    void paintEvent(QPaintEvent* event) Q_DECL_OVERRIDE;

    std::set<int> hot_spot_line_numbers_;      ///< Line number(s) of hot spots.
    std::set<int> search_match_line_numbers_;  ///< Line number(s) of text search matches.
};

#endif  // QTISAGUI_ISA_VERTICAL_SCROLL_BAR_H_
