//=============================================================================
// Copyright (c) 2022-2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for an isa dictionary.
//=============================================================================

#ifndef QTISAGUI_UTILITY_ISA_DICTIONARY_H_
#define QTISAGUI_UTILITY_ISA_DICTIONARY_H_

#include <array>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include <QColor>
#include <QtGlobal>

#include "qt_common/utils/common_definitions.h"

#include "amdisa/isa_decoder.h"

static const int kFunctionalGroupCount    = 10;
static const int kFunctionalSubgroupCount = 15;

// Color groups used to categorize the functional groups for color coding.
enum IsaColorGroup
{
    kIsaColorGroupUnknown,
    kIsaColorGroupScalarAlu,
    kIsaColorGroupScalarMemory,
    kIsaColorGroupVectorAlu,
    kIsaColorGroupVectorMemory,
    kIsaColorGroupBranch,
    kIsaColorGroupOther,
    kIsaColorGroupCount
};

// Map functional groups in the isa decoder to their respective color groups.
static const std::array<IsaColorGroup, kFunctionalGroupCount> kFunctionalGroupToColorGroup = {
    kIsaColorGroupUnknown,       // kFunctionalGroupUnknown
    kIsaColorGroupScalarAlu,     // kFunctionalGroupSalu
    kIsaColorGroupScalarMemory,  // kFunctionalGroupSmem
    kIsaColorGroupVectorAlu,     // kFunctionalGroupValu
    kIsaColorGroupVectorMemory,  // kFunctionalGroupVmem
    kIsaColorGroupOther,         // kFunctionalGroupExport
    kIsaColorGroupBranch,        // kFunctionalGroupBranch
    kIsaColorGroupOther,         // kFunctionalGroupMessages
    kIsaColorGroupOther,         // kFunctionalGroupWaveControl
    kIsaColorGroupOther,         // kFunctionalGroupTrap
};

static const QColor kIsaColorBlue       = QColor(56, 80, 240);    ///< Blue.
static const QColor kIsaColorGreyBlue   = QColor(90, 110, 230);   ///< Grey Blue.
static const QColor kIsaColorLightBlue  = QColor(42, 142, 212);   ///< Light blue.
static const QColor kIsaColorDarkGreen  = QColor(20, 128, 20);    ///< Dark green.
static const QColor kIsaColorLightGreen = QColor(64, 164, 92);    ///< Light green.
static const QColor kIsaColorMagenta    = QColor(142, 64, 142);   ///< Magenta.
static const QColor kIsaColorPurple     = QColor(124, 90, 150);   ///< Purple.
static const QColor kIsaColorLightRed   = QColor(240, 80, 160);   ///< light Red.
static const QColor kIsaColorRed        = QColor(240, 40, 40);    ///< Red.
static const QColor kIsaColorDarkRed    = QColor(180, 0, 0);      ///< Dark Red.
static const QColor kIsaColorOrange     = QColor(255, 128, 0);    ///< Orange.
static const QColor kIsaColorLightPink  = QColor(255, 160, 255);  ///< Light pink.
static const QColor kIsaColorPink       = QColor(240, 64, 128);   ///< Pink.
static const QColor kIsaColorGrey       = QColor(128, 128, 128);  ///< Grey.

// Map of IsaColorGroups and subgroups to their respective colors.
static const std::map<std::pair<IsaColorGroup, amdisa::FunctionalSubgroups>, QColor> kFunctionalSubgroupColorMap = {
    std::pair{std::pair{kIsaColorGroupUnknown, amdisa::FunctionalSubgroups::kFunctionalSubgroupUnknown}, Qt::gray},
    std::pair{std::pair{kIsaColorGroupScalarAlu, amdisa::FunctionalSubgroups::kFunctionalSubgroupUnknown}, kIsaColorBlue},
    std::pair{std::pair{kIsaColorGroupScalarMemory, amdisa::FunctionalSubgroups::kFunctionalSubgroupUnknown}, kIsaColorLightRed},
    std::pair{std::pair{kIsaColorGroupVectorMemory, amdisa::FunctionalSubgroups::kFunctionalSubgroupUnknown}, kIsaColorRed},
    std::pair{std::pair{kIsaColorGroupVectorMemory, amdisa::FunctionalSubgroups::kFunctionalSubgroupBvh}, kIsaColorDarkRed},
    std::pair{std::pair{kIsaColorGroupVectorAlu, amdisa::FunctionalSubgroups::kFunctionalSubgroupUnknown}, kIsaColorDarkGreen},
    std::pair{std::pair{kIsaColorGroupBranch, amdisa::FunctionalSubgroups::kFunctionalSubgroupUnknown}, kIsaColorPurple},
    std::pair{std::pair{kIsaColorGroupOther, amdisa::FunctionalSubgroups::kFunctionalSubgroupUnknown}, Qt::gray}};

/// @brief Class defining a prefix tree node.
template <typename UserType>
class PrefixTreeNode
{
public:
    /// @brief Constructor.
    ///
    /// @param [in] data The character to store in this node.
    inline PrefixTreeNode(const char data)
        : data_(data)
        , is_leaf_node_(false)
    {
    }

    char                                                      data_;          ///< The character this node represents.
    bool                                                      is_leaf_node_;  ///< If current node is a leaf node.
    std::unordered_map<char, std::shared_ptr<PrefixTreeNode>> child_nodes_;   ///< Map to the child nodes.
    UserType                                                  user_data_;     ///< User defined data to record status of each prefix string.
};

/// @brief Class defining a prefix tree.
template <typename UserType>
class PrefixTree
{
public:
    /// @brief Constructor.
    inline PrefixTree()
    {
        root_ = std::make_shared<PrefixTreeNode<UserType>>('\0');
    }

    /// @brief Insert a string with user data into the tree.
    ///
    /// @param [in] str       String to insert.
    /// @param [in] user_data User defined data indicating the status of prefix string.
    inline void Insert(const std::string& str, const UserType& user_data)
    {
        if (!str.empty())
        {
            std::shared_ptr<PrefixTreeNode<UserType>> current_node = root_;

            Q_ASSERT_X(current_node != nullptr, "PrefixTree Insert", "tree node nullptr");

            if (current_node == nullptr)
            {
                return;
            }

            for (const auto& c : str)
            {
                if (current_node->child_nodes_.find(c) == current_node->child_nodes_.end())
                {
                    current_node->child_nodes_[c] = std::make_shared<PrefixTreeNode<UserType>>(c);
                    current_node                  = current_node->child_nodes_[c];
                }
                else
                {
                    current_node = current_node->child_nodes_[c];
                }

                Q_ASSERT_X(current_node != nullptr, "PrefixTree Insert", "tree node nullptr");

                if (current_node == nullptr)
                {
                    return;
                }
            }

            current_node->is_leaf_node_ = true;
            current_node->user_data_    = user_data;
        }
    }

    /// @brief Search a string in the tree and return its user-defined data type.
    ///
    /// The search is trying find an exact match of the input string in the tree.
    ///
    /// @param [in] str       Target string to search.
    /// @param [in] user_data Output user data that matches the user-defined string type.
    ///
    /// @return true if an exact match was found.
    inline bool Search(const std::string& str, UserType& user_data)
    {
        if (!str.empty())
        {
            std::shared_ptr<PrefixTreeNode<UserType>> current_node = root_;

            Q_ASSERT_X(current_node != nullptr, "PrefixTree Search", "tree node nullptr");

            if (current_node == nullptr)
            {
                return false;
            }

            for (const auto& c : str)
            {
                if (current_node->child_nodes_.find(c) == current_node->child_nodes_.end())
                {
                    return false;
                }
                else
                {
                    current_node = current_node->child_nodes_[c];
                }

                Q_ASSERT_X(current_node != nullptr, "PrefixTree Search", "tree node nullptr");

                if (current_node == nullptr)
                {
                    return false;
                }
            }

            if (current_node != nullptr && current_node->is_leaf_node_)
            {
                user_data = current_node->user_data_;
                return true;
            }
        }

        return false;
    }

    /// @brief Search the tree to find out if there is a string in the tree that can be the prefix of the given string.
    ///
    /// The search will return when the longest prefix was found and return its user-defined data.
    ///
    /// @param [in]  str       Target string to search.
    /// @param [in]  user_data Output user data that matches the user-defined string type.
    ///
    /// @return true if a prefix was found, if there are multiple matching prefix, it returns the longest one.
    inline bool PrefixFoundInTree(const std::string& str, UserType& user_data) const
    {
        if (!str.empty())
        {
            std::shared_ptr<PrefixTreeNode<UserType>> current_node   = root_;
            bool                                      seen_last_leaf = false;
            UserType                                  last_user_data;

            Q_ASSERT_X(current_node != nullptr, "PrefixTree PrefixFoundInTree", "tree node nullptr");

            if (current_node == nullptr)
            {
                return false;
            }

            for (const auto& c : str)
            {
                if (current_node->child_nodes_.find(c) == current_node->child_nodes_.end())
                {
                    // If we could not find a match of the char in child nodes,
                    // we should check if we have seen a leaf before, if a leaf is already seen,
                    // it means we have found a matching prefix, just record its user data and return.
                    if (seen_last_leaf)
                    {
                        user_data = last_user_data;
                        return true;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    current_node = current_node->child_nodes_[c];

                    Q_ASSERT_X(current_node != nullptr, "PrefixTree PrefixFoundInTree", "tree node nullptr");

                    if (current_node == nullptr)
                    {
                        return false;
                    }

                    if (current_node->is_leaf_node_)
                    {
                        seen_last_leaf = true;
                        last_user_data = current_node->user_data_;
                    }
                }
            }

            if (current_node != nullptr && current_node->is_leaf_node_)
            {
                user_data = current_node->user_data_;
                return true;
            }
        }

        return false;
    }

private:
    std::shared_ptr<PrefixTreeNode<UserType>> root_ = nullptr;  ///< The root node of the tree.
};

/// @brief ISA syntax highlighter keyword dictionary.
class IsaColorCodingDictionaryInstance
{
public:
    /// @brief Get current instance.
    ///
    /// @return The current instance.
    static IsaColorCodingDictionaryInstance& GetInstance();

    /// @brief Should the input string be highlighted, if so, record its color info.
    /// 
    /// For opcodes, check the functional group. For operands, functional group info is empty. 
    /// Check the prefix of the operands to determine if it should be highlighted and what color it should be highlighted with.
    ///
    /// @param [in] str                   The input string.
    /// @param [in] functional_group_info The functional group and subgroup information.
    /// @param [out] color                The color info that the string should be highlighted with.
    ///
    /// @return true if the string should be highlighted.
    bool ShouldHighlight(const std::string& str, const amdisa::FunctionalGroupSubgroupInfo& functional_group_info, QColor& color) const;

private:
    /// @brief Constructor.
    IsaColorCodingDictionaryInstance();

    /// @brief Destructor.
    ~IsaColorCodingDictionaryInstance() = default;

    /// @brief Disable copy constructor.
    IsaColorCodingDictionaryInstance(const IsaColorCodingDictionaryInstance&) = delete;

    /// @brief Disable assignment operator.
    IsaColorCodingDictionaryInstance& operator=(const IsaColorCodingDictionaryInstance&) = delete;

    PrefixTree<QColor> prefix_tree_;  ///< The prefix tree that is used as a dictionary to decide the coloring profile.
};

#endif  // QTISAGUI_UTILITY_ISA_DICTIONARY_H_
