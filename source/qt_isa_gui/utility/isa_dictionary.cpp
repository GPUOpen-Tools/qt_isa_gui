//=============================================================================
// Copyright (c) 2022-2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for an isa dictionary.
//=============================================================================

#include "isa_dictionary.h"

#include "qt_common/utils/common_definitions.h"
#include "qt_common/utils/qt_util.h"

IsaColorCodingDictionaryInstance& IsaColorCodingDictionaryInstance::GetInstance()
{
    static IsaColorCodingDictionaryInstance instance;
    return instance;
}

bool IsaColorCodingDictionaryInstance::ShouldHighlight(const std::string&                         str,
                                                       const amdisa::FunctionalGroupSubgroupInfo& functional_group_info,
                                                       QColor&                                    color) const
{
    color = QtCommon::QtUtils::ColorTheme::Get().GetCurrentThemeColors().graphics_scene_text_color;

    QColor        tree_color;
    IsaColorGroup color_group = kIsaColorGroupUnknown;

    // Sanity check before indexing into function group to color group array.
    if (static_cast<int>(functional_group_info.isa_functional_group) >= 0 &&
        static_cast<int>(functional_group_info.isa_functional_group) < kFunctionalGroupCount)
    {
        color_group = kFunctionalGroupToColorGroup[static_cast<int>(functional_group_info.isa_functional_group)];
    }

    bool should_highlight = false;

    // Functional group info is empty for operand text.
    if (color_group == kIsaColorGroupUnknown)
    {
        // Check the prefix tree to determine if the operand should be highlighted and what color it should be highlighted with based on its prefix.
        should_highlight = prefix_tree_.PrefixFoundInTree(str, tree_color);
    }
    else if (color_group != kIsaColorGroupOther)
    {
        amdisa::FunctionalSubgroups subgroup = amdisa::FunctionalSubgroups::kFunctionalSubgroupUnknown;

        // Check if any of the functional subgroups for this instruction have a defined color, if so, use that subgroup for coloring instead of the "unknown" subgroup. This allows instructions with known subgroups to be colored differently than those with unknown subgroups within the same functional group.
        for (size_t i = 0; i < functional_group_info.isa_functional_subgroups.size(); i++)
        {
            std::pair<IsaColorGroup, amdisa::FunctionalSubgroups> key{color_group, functional_group_info.isa_functional_subgroups.at(i)};

            if (kFunctionalSubgroupColorMap.find(key) != kFunctionalSubgroupColorMap.end())
            {
                subgroup = functional_group_info.isa_functional_subgroups.at(i);
                break;
            }
        }

        std::pair<IsaColorGroup, amdisa::FunctionalSubgroups> key{color_group, subgroup};

        if (kFunctionalSubgroupColorMap.find(key) != kFunctionalSubgroupColorMap.end())
        {
            should_highlight = true;
            tree_color       = kFunctionalSubgroupColorMap.at(key);
        }
    }

    if (should_highlight)
    {
        color = tree_color;
    }

    return should_highlight;
}

IsaColorCodingDictionaryInstance::IsaColorCodingDictionaryInstance()
{
    prefix_tree_.Insert("idxen", kIsaColorPurple);
    prefix_tree_.Insert("s_", kIsaColorGreyBlue);
    prefix_tree_.Insert("s[", kIsaColorGreyBlue);  // Scalar register.
    prefix_tree_.Insert("[s", kIsaColorGreyBlue);  // Scalar register range.
    prefix_tree_.Insert("|s", kIsaColorGreyBlue);  // Scalar register absolute value.
    prefix_tree_.Insert("-s", kIsaColorGreyBlue);  // Scalar register negative value.
    prefix_tree_.Insert("v_", kIsaColorLightGreen);
    prefix_tree_.Insert("v[", kIsaColorLightGreen);  // Vector register.
    prefix_tree_.Insert("[v", kIsaColorLightGreen);  // Vector register range.
    prefix_tree_.Insert("|v", kIsaColorLightGreen);  // Vector register absolute value.
    prefix_tree_.Insert("-v", kIsaColorLightGreen);  // Vector register negative value.
    prefix_tree_.Insert("//", kIsaColorLightBlue);   // Comments.
    prefix_tree_.Insert("-", kIsaColorGrey);         // Negative constants (negative registers use longer prefixes -s, -v).
    prefix_tree_.Insert("null", kIsaColorGrey);      // Constants.
    prefix_tree_.Insert("src_", kIsaColorGrey);      // Special constant operands (SRC_SCC, SRC_SHARED_BASE, etc).
    prefix_tree_.Insert("SRC_", kIsaColorGrey);
    prefix_tree_.Insert("vcc", kIsaColorGrey);  // Vector condition code.
    prefix_tree_.Insert("VCC", kIsaColorGrey);
    prefix_tree_.Insert("exec", kIsaColorGrey);  // Execute mask.
    prefix_tree_.Insert("EXEC", kIsaColorGrey);
    prefix_tree_.Insert("m0", kIsaColorGrey);  // M0 special register.
    prefix_tree_.Insert("M0", kIsaColorGrey);
    prefix_tree_.Insert("ttmp", kIsaColorGrey);  // Trap handler temps.
    prefix_tree_.Insert("TTMP", kIsaColorGrey);

    for (size_t i = 0; i <= 9; ++i)
    {
        prefix_tree_.Insert(std::to_string(i), kIsaColorGrey);  // Numeric constants.
        prefix_tree_.Insert("s" + std::to_string(i), kIsaColorGreyBlue);
        prefix_tree_.Insert("v" + std::to_string(i), kIsaColorLightGreen);
    }
}
