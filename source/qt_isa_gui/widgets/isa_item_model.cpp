//=============================================================================
// Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for an isa item model.
//=============================================================================

#include "isa_item_model.h"

#include <cctype>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_map>

#include <QColor>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QStringList>

#include "qt_common/utils/common_definitions.h"
#include "qt_common/utils/qt_util.h"

#include "qt_isa_gui/utility/isa_dictionary.h"

#include "isa_tree_view.h"

const QString                                             IsaItemModel::kColumnPadding             = " ";           // Pad columns by 1 character.
const QString                                             IsaItemModel::kOpCodeColumnIndent        = "     ";       // Indent op code column by 5 characters.
const QString                                             IsaItemModel::kOperandTokenSpace         = " ";           // Separate tokens within the same operand.
const QString                                             IsaItemModel::kOperandDelimiter          = ", ";          // Separate operands by a comma and a space.
const std::string                                         IsaItemModel::kUnconditionalBranchString = "s_branch";    // Branch op code text.
const std::string                                         IsaItemModel::kConditionalBranchString   = "s_cbranch_";  // Conditional branch op code text.
const std::array<std::string, IsaItemModel::kColumnCount> IsaItemModel::kColumnNames               = {"",
                                                                                                      "PC address",
                                                                                                      "Opcode",
                                                                                                      "Operands",
                                                                                                      "Binary representation"};  ///< Predefined column headers.

namespace
{
    // The active decoder for the active architecture.
    std::shared_ptr<amdisa::IsaDecoder> isa_decoder;

    // The manager of all the architectures.
    amdisa::DecodeManager decode_manager;

    // Isa decoder initialization status.
    bool is_decoder_initialized = false;

    // The individual isa spec names.
    const std::unordered_map<amdisa::GpuArchitecture, std::string> kIsaSpecNameMap = {{amdisa::GpuArchitecture::kRdna1, "amdgpu_isa_rdna1.xml"},
                                                                                      {amdisa::GpuArchitecture::kRdna2, "amdgpu_isa_rdna2.xml"},
                                                                                      {amdisa::GpuArchitecture::kRdna3, "amdgpu_isa_rdna3.xml"},
                                                                                      {amdisa::GpuArchitecture::kRdna3_5, "amdgpu_isa_rdna3_5.xml"},
                                                                                      {amdisa::GpuArchitecture::kRdna4, "amdgpu_isa_rdna4.xml"},
                                                                                      {amdisa::GpuArchitecture::kCdna1, "amdgpu_isa_mi100.xml"},
                                                                                      {amdisa::GpuArchitecture::kCdna2, "amdgpu_isa_mi200.xml"},
                                                                                      {amdisa::GpuArchitecture::kCdna3, "amdgpu_isa_mi300.xml"}};

    // Avoid repetitive string conversions.
    const std::string kOperandTokenSpaceStdString = IsaItemModel::kOperandTokenSpace.toStdString();
    const std::string kOperandDelimiterStdString  = IsaItemModel::kOperandDelimiter.toStdString();

    // Single register operands; match negative value or absolute value too. Ex) s0 or -s0 or |s0|
    const QRegularExpression kScalarRegisterExpression(QString("(-?)(") + QRegularExpression::escape("|") + QString("?)(s[0-9]+)(\\2)"));
    const QRegularExpression kVectorRegisterExpression(QString("(-?)(") + QRegularExpression::escape("|") + QString("?)(v[0-9]+)(\\2)"));

    // The start of a pair of single register operands. Ex) [s0
    const QRegularExpression kScalarPairStartRegisterExpression(QRegularExpression::escape("[") + QString("(-?)(") + QRegularExpression::escape("|") +
                                                                QString("?)(s[0-9]+)(\\2)"));
    const QRegularExpression kVectorPairStartRegisterExpression(QRegularExpression::escape("[") + QString("(-?)(") + QRegularExpression::escape("|") +
                                                                QString("?)(v[0-9]+)(\\2)"));

    // The end of a pair of single register operands. Ex) s0]
    const QRegularExpression kScalarPairEndRegisterExpression(QString("(-?)(") + QRegularExpression::escape("|") + QString("?)(s[0-9]+)(\\2)") +
                                                              QRegularExpression::escape("]"));
    const QRegularExpression kVectorPairEndRegisterExpression(QString("(-?)(") + QRegularExpression::escape("|") + QString("?)(v[0-9]+)(\\2)") +
                                                              QRegularExpression::escape("]"));

    // Register range operands. Ex) s[0:1]
    const QRegularExpression kScalarRegisterRangeExpression(QString("s") + QRegularExpression::escape("[") + QString("[0-9]+:[0-9]+") +
                                                            QRegularExpression::escape("]"));
    const QRegularExpression kVectorRegisterRangeExpression(QString("v") + QRegularExpression::escape("[") + QString("[0-9]+:[0-9]+") +
                                                            QRegularExpression::escape("]"));

    // Constant operands. Ex) 0 or 1.0 or 0x01
    const QRegularExpression kConstantExpression("-?[0-9].*");

    // Private local helper variable to assist setting branch label hit boxes.
    qreal fixed_font_character_width = 0;

}  // namespace

IsaItemModel::IsaItemModel(QObject* parent, amdisa::DecodeManager* decode_manager_ptr)
    : QAbstractItemModel(parent)
    , fixed_font_character_width_(0)
    , line_numbers_visible_(true)
    , decode_manager_((decode_manager_ptr != nullptr) ? decode_manager_ptr : &decode_manager)
{
}

IsaItemModel::~IsaItemModel()
{
}

int IsaItemModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);

    return Columns::kColumnCount;
}

int IsaItemModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0)
    {
        // Restrict row count to anything but the 0th column.
        // Child nodes should only be parented to the 0th column.
        return 0;
    }

    if (!parent.isValid())
    {
        // The number of top-level nodes is the number of code blocks.
        return static_cast<int>(blocks_.size());
    }

    if (!parent.parent().isValid())
    {
        // The number of rows underneath a Code Block is the number of instructions in that Code Block.
        return static_cast<int>(blocks_.at(parent.row())->instruction_lines.size());
    }

    // Instructions should not have any rows underneath them.
    return 0;
}

QModelIndex IsaItemModel::index(int row, int column, const QModelIndex& parent) const
{
    QModelIndex tree_index;

    if (!hasIndex(row, column, parent))
    {
        return tree_index;
    }

    // Code blocks are top level nodes; create an index with no internal data.
    if (!parent.isValid())
    {
        return createIndex(row, column, nullptr);
    }

    // Individual instruction lines are child nodes; attach parent row index as internal data.
    return createIndex(row, column, (void*)blocks_[parent.row()].get());
}

QModelIndex IsaItemModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return QModelIndex();
    }

    const InstructionBlock* code_block = static_cast<InstructionBlock*>(index.internalPointer());

    if (code_block != nullptr)
    {
        return createIndex(code_block->position, 0);
    }

    return QModelIndex();
}

QVariant IsaItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QVariant header_data;

    if (orientation == Qt::Vertical || role != Qt::DisplayRole || section < 0 || section >= static_cast<int>(kColumnNames.size()))
    {
        return header_data;
    }

    header_data = kColumnNames[section].c_str();

    return header_data;
}

QVariant IsaItemModel::data(const QModelIndex& index, int role) const
{
    QVariant data;

    switch (role)
    {
    case Qt::FontRole:
    {
        data.setValue(fixed_font_);
        break;
    }
    case Qt::TextAlignmentRole:
    {
        int alignment_flags = 0;

        if (index.column() == kLineNumber)
        {
            alignment_flags = Qt::AlignRight | Qt::AlignTop;
        }
        else
        {
            alignment_flags = Qt::AlignLeft | Qt::AlignTop;
        }

        data.setValue(alignment_flags);
        break;
    }
    case Qt::ForegroundRole:
    {
        // This role is not useful for columns with multi-coloring in the same column.

        // Default to color theme's text color.
        data.setValue(QtCommon::QtUtils::ColorTheme::Get().GetCurrentThemeColors().graphics_scene_text_color);

        if (!index.parent().isValid() && index.column() == kOpCode)
        {
            // Provide a different starting color for code block comments and code block labels with matching branching instructions.

            const auto block = blocks_.at(index.row());

            if (block->row_type == RowType::kComment)
            {
                QColor comment_color;
                if (QtCommon::QtUtils::ColorTheme::Get().GetColorTheme() == kColorThemeTypeLight)
                {
                    comment_color = kIsaLightThemeColorLightBlue;
                }
                else
                {
                    comment_color = kIsaDarkThemeColorLightBlue;
                }

                // This is a code block comment; provide light blue as its text color.
                data.setValue(comment_color);
            }
            else if (block->row_type == RowType::kCode)
            {
                const auto code_block = std::static_pointer_cast<InstructionBlock>(block);

                if (!code_block->mapped_branch_instructions.empty())
                {
                    // This is a code block label that is called by a branch instruction(s); provide purple as its text color.
                    QColor label_color;

                    if (QtCommon::QtUtils::ColorTheme::Get().GetColorTheme() == kColorThemeTypeLight)
                    {
                        label_color = kIsaLightThemeColorDarkMagenta;
                    }
                    else
                    {
                        label_color = kIsaDarkThemeColorDarkMagenta;
                    }

                    data.setValue(label_color);
                }
            }
        }
        else if (index.parent().isValid() && index.column() != kLineNumber)
        {
            // Provide a different starting color for child row comments

            const auto row = blocks_.at(index.parent().row())->instruction_lines.at(index.row());

            if (row->row_type == RowType::kComment)
            {
                QColor comment_color;

                if (QtCommon::QtUtils::ColorTheme::Get().GetColorTheme() == kColorThemeTypeLight)
                {
                    comment_color = kIsaLightThemeColorLightBlue;
                }
                else
                {
                    comment_color = kIsaDarkThemeColorLightBlue;
                }

                data.setValue(comment_color);
            }
        }

        break;
    }
    case Qt::DisplayRole:
    {
        const QModelIndex parent_index = index.parent();

        switch (index.column())
        {
        case kLineNumber:
        {
            int line_number = 0;

            if (!parent_index.isValid())
            {
                // Code block.
                line_number = blocks_.at(index.row())->line_number;
            }
            else
            {
                // Instruction line.
                line_number = blocks_.at(parent_index.row())->instruction_lines.at(index.row())->line_number;
            }

            data.setValue(line_number);
            break;
        }
        case kOpCode:
        {
            if (!index.parent().isValid())
            {
                const auto block = blocks_.at(index.row());

                if (block->row_type == RowType::kComment)
                {
                    const auto comment_block = std::static_pointer_cast<CommentBlock>(block);

                    data.setValue(QString(comment_block->text.c_str()));
                }
                else if (block->row_type == RowType::kCode)
                {
                    const auto code_block = std::static_pointer_cast<InstructionBlock>(block);

                    data.setValue(QString(code_block->token.token_text.c_str()));
                }
            }
            else
            {
                const auto row = blocks_.at(index.parent().row())->instruction_lines.at(index.row());

                if (row->row_type == RowType::kCode)
                {
                    const auto instruction = std::static_pointer_cast<InstructionRow>(row);

                    data.setValue(QString(instruction->op_code_token.token_text.c_str()));
                }
                else if (row->row_type == RowType::kComment)
                {
                    const auto comment = std::static_pointer_cast<CommentRow>(row);

                    data.setValue(QString(comment->text.c_str()));
                }
            }

            break;
        }
        case kOperands:
        {
            if (index.parent().isValid() && (blocks_.at(index.parent().row())->instruction_lines.at(index.row())->row_type == RowType::kCode))
            {
                const auto  instruction = std::static_pointer_cast<InstructionRow>(blocks_.at(index.parent().row())->instruction_lines.at(index.row()));
                const auto& operand_token_groups = instruction->operand_tokens;
                std::string operands_string      = "";

                for (size_t i = 0; i < operand_token_groups.size(); i++)
                {
                    const auto& operand_token_group = operand_token_groups.at(i);

                    for (size_t j = 0; j < operand_token_group.size(); j++)
                    {
                        const std::string& arg = operand_token_group[j].token_text;
                        operands_string += arg;

                        if (j != operand_token_group.size() - 1)
                        {
                            operands_string += kOperandTokenSpaceStdString;
                        }
                    }

                    if (i != operand_token_groups.size() - 1)
                    {
                        operands_string += kOperandDelimiterStdString;
                    }
                }

                data.setValue(QString(operands_string.c_str()));
            }
            break;
        }
        case kPcAddress:
        {
            if (index.parent().isValid() && (blocks_.at(index.parent().row())->instruction_lines.at(index.row())->row_type == RowType::kCode))
            {
                // Instruction line.

                const auto instruction = std::static_pointer_cast<InstructionRow>(blocks_.at(index.parent().row())->instruction_lines.at(index.row()));

                data.setValue(QString(instruction->pc_address.c_str()));
            }

            break;
        }
        case kBinaryRepresentation:
        {
            if (index.parent().isValid() && (blocks_.at(index.parent().row())->instruction_lines.at(index.row())->row_type == RowType::kCode))
            {
                // Instruction line.

                const auto instruction = std::static_pointer_cast<InstructionRow>(blocks_.at(index.parent().row())->instruction_lines.at(index.row()));

                data.setValue(QString(instruction->binary_representation.c_str()));
            }

            break;
        }
        default:
        {
            break;
        }
        }
        break;
    }
    case Qt::UserRole:
    {
        // Use UserRole to store data needed for delegates to custom paint instruction rows.
        //
        // Store tokens for instructions.
        // Store nothing for comments.

        if (!index.parent().isValid())
        {
            std::vector<Token> tokens;

            const auto block = blocks_.at(index.row());

            if (block->row_type == RowType::kCode)
            {
                const auto code_block = std::static_pointer_cast<InstructionBlock>(block);

                tokens.push_back(code_block->token);
            }

            data.setValue(tokens);
        }
        else
        {
            const auto row = blocks_.at(index.parent().row())->instruction_lines.at(index.row());

            if (row->row_type == RowType::kCode)
            {
                const auto instruction = std::static_pointer_cast<InstructionRow>(row);

                if ((index.column() == kOpCode))
                {
                    std::vector<Token> tokens;

                    tokens.push_back(instruction->op_code_token);

                    data.setValue(tokens);
                }
                else if ((index.column() == kOperands))
                {
                    data.setValue(instruction->operand_tokens);
                }
            }
        }

        break;
    }
    case kLabelBranchRole:
    {
        bool is_code_block_label_branch_target = false;

        if (!index.parent().isValid())
        {
            const auto block = blocks_.at(index.row());

            if (block->row_type == RowType::kCode)
            {
                const auto code_block             = std::static_pointer_cast<InstructionBlock>(block);
                is_code_block_label_branch_target = !code_block->mapped_branch_instructions.empty();
            }
        }

        data.setValue(is_code_block_label_branch_target);
        break;
    }
    case kBranchIndexRole:
    {
        QVector<QModelIndex> branch_label_indices;

        if (index.column() == kOpCode)
        {
            if (!index.parent().isValid())
            {
                const auto block = blocks_.at(index.row());

                if (block->row_type == RowType::kCode)
                {
                    const auto code_block = std::static_pointer_cast<InstructionBlock>(block);

                    for (const auto& mapped_branch_instruction : code_block->mapped_branch_instructions)
                    {
                        const int         label_code_block_index  = mapped_branch_instruction.first;
                        const int         label_instruction_index = mapped_branch_instruction.second;
                        const QModelIndex branch_label_index      = this->index(label_instruction_index, 0, this->index(label_code_block_index, 0));
                        branch_label_indices.push_back(branch_label_index);
                    }
                }
            }
        }
        else if (index.column() == kOperands)
        {
            if (index.parent().isValid())
            {
                // Instruction line.

                const auto row = blocks_.at(index.parent().row())->instruction_lines.at(index.row());

                if (row->row_type == RowType::kCode)
                {
                    const auto instruction = std::static_pointer_cast<const InstructionRow>(row);

                    if (!instruction->operand_tokens.empty() && !instruction->operand_tokens.front().empty())
                    {
                        // Check the first token in the first operand.

                        const auto& token = instruction->operand_tokens.front().front();

                        if (token.type == TokenType::kBranchLabelType && token.start_register_index != -1)
                        {
                            const QModelIndex label_index = this->index(token.start_register_index, 0);
                            branch_label_indices.push_back(label_index);
                        }
                    }
                }
            }
        }

        data.setValue(branch_label_indices);
        break;
    }
    case kLineEnabledRole:
    {
        bool line_enabled = true;

        if (index.parent().isValid())
        {
            const auto row = blocks_.at(index.parent().row())->instruction_lines.at(index.row());

            if (row->row_type == RowType::kCode)
            {
                const auto instruction = std::static_pointer_cast<InstructionRow>(row);
                line_enabled           = instruction->enabled;
            }
        }

        data.setValue(line_enabled);
        break;
    }
    case kRowTypeRole:
    {
        RowType row_type = RowType::kRowCount;

        if (!index.parent().isValid())
        {
            const auto block = blocks_.at(index.row());
            row_type         = block->row_type;
        }
        else
        {
            const auto row = blocks_.at(index.parent().row())->instruction_lines.at(index.row());
            row_type       = row->row_type;
        }

        data.setValue(row_type);
        break;
    }
    case kDecodedIsa:
    {
        if (index.column() != IsaItemModel::Columns::kOpCode)
        {
            return data;
        }

        const auto        binary_isa_string = index.siblingAtColumn(IsaItemModel::Columns::kBinaryRepresentation).data().toString().toStdString();
        uint64_t          binary_isa        = 0;
        std::stringstream string_stream;

        string_stream << binary_isa_string;
        string_stream >> std::hex >> binary_isa;

        amdisa::InstructionInfoBundle instruction_info_bundle;
        std::string                   decode_error_message;

        const bool instruction_decoded = (isa_decoder != nullptr) && isa_decoder->DecodeInstruction(binary_isa, instruction_info_bundle, decode_error_message);

        if (!instruction_decoded)
        {
            return data;
        }
        else if (!instruction_info_bundle.bundle.empty())
        {
            const auto instruction_info = instruction_info_bundle.bundle.front();

            data.setValue(instruction_info);

            return data;
        }
    }
    default:
    {
        break;
    }
    }

    return data;
}

void IsaItemModel::CacheSizeHints()
{
    column_widths_.fill(0);
    line_number_corresponding_indices_.clear();

    const uint32_t line_number = (!blocks_.empty() && !blocks_.back()->instruction_lines.empty()) ? blocks_.back()->instruction_lines.back()->line_number : 0;
    const QString  max_line_number                  = QString::number(line_number);
    const qreal    padding_length                   = static_cast<qreal>(kColumnPadding.size());
    const qreal    max_line_number_length           = padding_length + static_cast<qreal>(max_line_number.size());
    qreal          max_pc_address_length            = 0;
    qreal          max_op_code_length               = 0;
    qreal          max_operand_length               = 0;
    qreal          max_binary_representation_length = 0;

    if (blocks_.empty())
    {
        return;
    }

    int code_block_index = 0;

    for (auto& code_block : blocks_)
    {
        line_number_corresponding_indices_.emplace_back(-1, code_block_index);

        int instruction_index = 0;

        for (auto instruction : code_block->instruction_lines)
        {
            line_number_corresponding_indices_.emplace_back(code_block_index, instruction_index++);

            if (instruction->row_type == RowType::kComment)
            {
                // Don't force comments to fit in the op code column.
                continue;
            }

            const auto instruction_line = std::static_pointer_cast<InstructionRow>(instruction);

            max_op_code_length    = std::max(max_op_code_length, static_cast<qreal>(instruction_line->op_code_token.token_text.size()));
            max_pc_address_length = std::max(max_pc_address_length, static_cast<qreal>(instruction_line->pc_address.length()));

            std::string operands;

            const auto number_operand_groups = instruction_line->operand_tokens.size();

            for (size_t i = 0; i < number_operand_groups; i++)
            {
                const auto& operand_group            = instruction_line->operand_tokens.at(i);
                const auto  number_operands_in_group = operand_group.size();

                for (size_t j = 0; j < number_operands_in_group; j++)
                {
                    const auto& operand = operand_group.at(j);

                    operands += operand.token_text;

                    if (j < number_operands_in_group - 1)
                    {
                        operands += kOperandTokenSpaceStdString;
                    }
                }

                if (i < number_operand_groups - 1)
                {
                    operands += kOperandDelimiterStdString;
                }
            }

            max_operand_length               = std::max(max_operand_length, static_cast<qreal>(operands.size()));
            max_binary_representation_length = std::max(max_binary_representation_length, static_cast<qreal>(instruction_line->binary_representation.size()));
        }

        code_block_index++;
    }

    max_pc_address_length += padding_length;
    max_op_code_length += padding_length + static_cast<qreal>(kOpCodeColumnIndent.size());
    max_operand_length += padding_length;
    max_binary_representation_length += padding_length;

    const uint32_t line_number_width           = static_cast<uint32_t>(std::ceil(max_line_number_length * fixed_font_character_width_));
    const uint32_t pc_address_width            = static_cast<uint32_t>(std::ceil(max_pc_address_length * fixed_font_character_width_));
    const uint32_t op_code_width               = static_cast<uint32_t>(std::ceil(max_op_code_length * fixed_font_character_width_));
    const uint32_t operand_width               = static_cast<uint32_t>(std::ceil(max_operand_length * fixed_font_character_width_));
    const uint32_t binary_representation_width = static_cast<uint32_t>(std::ceil(max_binary_representation_length * fixed_font_character_width_));

    column_widths_[kLineNumber]           = line_number_width;
    column_widths_[kPcAddress]            = pc_address_width;
    column_widths_[kOpCode]               = op_code_width;
    column_widths_[kOperands]             = operand_width;
    column_widths_[kBinaryRepresentation] = binary_representation_width;
}

QSize IsaItemModel::ColumnSizeHint(int column_index, IsaTreeView* tree) const
{
    QSize size_hint(0, 0);

    if (column_index < 0 || column_index >= kColumnCount)
    {
        return size_hint;
    }

    size_hint.setHeight(QFontMetricsF(fixed_font_, tree).height() + 2.0);  // Add arbitrary spacing found in Qt.
    size_hint.setWidth(column_widths_[column_index]);

    return size_hint;
}

void IsaItemModel::SetFixedFont(const QFont& fixed_font, IsaTreeView* tree)
{
    fixed_font_ = fixed_font;

    QFontMetricsF font_metrics(fixed_font_, tree);

    fixed_font_character_width_ = font_metrics.horizontalAdvance('T');
    fixed_font_character_width  = fixed_font_character_width_;
}

void IsaItemModel::SetArchitecture(amdisa::GpuArchitecture architecture, bool load_isa_spec)
{
    if (load_isa_spec)
    {
        LoadIsaSpec(architecture);

        if (!is_decoder_initialized)
        {
            return;
        }
    }

    if (decode_manager_ == nullptr)
    {
        return;
    }

    isa_decoder = decode_manager_->GetDecoder(architecture);

    emit ArchitectureChanged(isa_decoder != nullptr);
}

QModelIndex IsaItemModel::GetLineNumberModelIndex(int line_number)
{
    if (line_number < 0 || line_number >= static_cast<int>(line_number_corresponding_indices_.size()))
    {
        return QModelIndex();
    }

    const auto& row_pair   = line_number_corresponding_indices_.at(line_number);
    const auto& parent_row = row_pair.first;
    const auto& child_row  = row_pair.second;

    const QModelIndex parent_index = index(parent_row, 0);

    return index(child_row, 0, parent_index);
}

std::string IsaItemModel::TrimStr(std::string input_string)
{
    static const char* kSpecialChars = " \t\n\r\f\v";

    input_string.erase(input_string.find_last_not_of(kSpecialChars) + 1);
    input_string.erase(0, input_string.find_first_not_of(kSpecialChars));

    return input_string;
}

void IsaItemModel::Split(std::string line, std::string delimiter, std::vector<std::string>& list)
{
    size_t      pos = 0;
    std::string token;

    while ((pos = line.find(delimiter)) != std::string::npos)
    {
        token = line.substr(0, pos);
        list.push_back(TrimStr(token));
        line.erase(0, pos + delimiter.length());
    }

    list.push_back(TrimStr(line));
}

void IsaItemModel::ClearBranchInstructionMapping()
{
    for (auto block : blocks_)
    {
        if (block->row_type != RowType::kCode)
        {
            continue;
        }

        auto code_block = std::static_pointer_cast<InstructionBlock>(block);

        code_block->mapped_branch_instructions.clear();
    }
}

void IsaItemModel::MapBlocksToBranchInstructions()
{
    if (blocks_.empty())
    {
        return;
    }

    ClearBranchInstructionMapping();

    code_block_label_to_index_.clear();

    // Build map of code block label -> code block index.

    for (size_t block_index = 0; block_index < blocks_.size(); block_index++)
    {
        const auto block = blocks_.at(block_index);

        if (block->row_type != RowType::kCode)
        {
            continue;
        }

        const auto code_block                                    = std::static_pointer_cast<const InstructionBlock>(block);
        code_block_label_to_index_[code_block->token.token_text] = code_block->position;
    }

    // Use map to assign block's their corresponding branch instructions and vice versa.

    for (size_t block_index = 0; block_index < blocks_.size(); block_index++)
    {
        const auto block = blocks_.at(block_index);

        if (block->row_type != RowType::kCode)
        {
            continue;
        }

        const auto code_block = std::static_pointer_cast<InstructionBlock>(block);

        for (size_t instruction_index = 0; instruction_index < code_block->instruction_lines.size(); instruction_index++)
        {
            const auto row = code_block->instruction_lines.at(instruction_index);

            if (row->row_type != RowType::kCode)
            {
                continue;
            }

            auto       instruction  = std::static_pointer_cast<InstructionRow>(row);
            const auto op_code_text = instruction->op_code_token.token_text;
            const bool is_branch =
                ((op_code_text.find(kUnconditionalBranchString) != std::string::npos) || (op_code_text.find(kConditionalBranchString) != std::string::npos));

            if (is_branch && (!instruction->operand_tokens.empty()) && (!instruction->operand_tokens.front().empty()))
            {
                // Assume branch target is first operand of first operand group.

                const auto map_iter = code_block_label_to_index_.find(instruction->operand_tokens.front().front().token_text);

                if (map_iter != code_block_label_to_index_.end())
                {
                    const auto branch_target_block_index = map_iter->second;
                    const auto branch_target_block       = blocks_.at(branch_target_block_index);
                    auto       branch_target_code_block  = std::static_pointer_cast<IsaItemModel::InstructionBlock>(blocks_.at(branch_target_block_index));

                    // Code block remembers which branch instruction targeted it.
                    branch_target_code_block->mapped_branch_instructions.emplace_back(
                        std::make_pair(static_cast<uint32_t>(block_index), static_cast<uint32_t>(instruction_index)));

                    // Branch instruction remembers which code block is its target.
                    instruction->operand_tokens.front().front().start_register_index = branch_target_block_index;
                }
            }
        }
    }

    code_block_label_to_index_.clear();
}

void IsaItemModel::ParseSelectableTokens(const std::string&                             op_code,
                                         IsaItemModel::Token&                           op_code_token,
                                         const std::vector<std::string>&                operands,
                                         std::vector<std::vector<IsaItemModel::Token>>& selectable_tokens,
                                         qreal                                          fixed_character_width) const
{
    // Op code; should be a single token and always selectable.

    qreal token_width = fixed_character_width * static_cast<qreal>(op_code.size());

    op_code_token.type             = IsaItemModel::TokenType::kTypeCount;
    op_code_token.is_selectable    = true;
    op_code_token.token_text       = op_code;
    op_code_token.x_position_start = fixed_character_width * static_cast<qreal>(IsaItemModel::kOpCodeColumnIndent.size());
    op_code_token.x_position_end   = op_code_token.x_position_start + token_width;

    bool is_branch_instruction = false;

    if ((op_code.find(IsaItemModel::kUnconditionalBranchString) != std::string::npos) ||
        (op_code.find(IsaItemModel::kConditionalBranchString) != std::string::npos))
    {
        is_branch_instruction = true;
    }

    // Operands; determine which tokens can be selected.

    qreal token_start_x = 0;
    qreal token_end_x   = 0;

    for (const auto& operand : operands)
    {
        std::vector<std::string>         tokens;
        std::vector<IsaItemModel::Token> operand_tokens;

        Split(operand, " ", tokens);

        for (const auto& token : tokens)
        {
            IsaItemModel::Token selectable_token;
            selectable_token.is_selectable = false;
            selectable_token.token_text    = token;

            selectable_token.type = IsaItemModel::TokenType::kTypeCount;

            if (is_branch_instruction)
            {
                // Identify this operand token simply as the target of a branch instruction.

                selectable_token.type = IsaItemModel::TokenType::kBranchLabelType;
            }
            else
            {
                const QString token_string      = token.c_str();
                const int     token_string_size = static_cast<int>(token.size());

                // Single registers.
                const auto s_register_expression_match = kScalarRegisterExpression.match(token_string);
                const auto v_register_expression_match = kVectorRegisterExpression.match(token_string);

                // Single register that is the start of a pair of registers.
                const auto s_register_pair_start_expression_match = kScalarPairStartRegisterExpression.match(token_string);
                const auto v_register_pair_start_expression_match = kVectorPairStartRegisterExpression.match(token_string);

                // Single register that is the end of a pair of registers.
                const auto s_register_pair_end_expression_match = kScalarPairEndRegisterExpression.match(token_string);
                const auto v_register_pair_end_expression_match = kVectorPairEndRegisterExpression.match(token_string);

                // Range of registers.
                const auto s_register_range_expression_match = kScalarRegisterRangeExpression.match(token_string);
                const auto v_register_range_expression_match = kVectorRegisterRangeExpression.match(token_string);

                // Constants.
                const auto constant_expression_match = kConstantExpression.match(token_string);

                // Attempt to exact match single registers.
                bool is_scalar_register = s_register_expression_match.hasMatch() && s_register_expression_match.capturedStart() == 0 &&
                                          s_register_expression_match.capturedEnd() == token_string_size;

                bool is_vector_register = v_register_expression_match.hasMatch() && v_register_expression_match.capturedStart() == 0 &&
                                          v_register_expression_match.capturedEnd() == token_string_size;

                if (!is_scalar_register)
                {
                    is_scalar_register |= s_register_pair_start_expression_match.hasMatch() && s_register_pair_start_expression_match.capturedStart() == 0 &&
                                          s_register_pair_start_expression_match.capturedEnd() == token_string_size;
                }

                if (!is_vector_register)
                {
                    is_vector_register |= v_register_pair_start_expression_match.hasMatch() && v_register_pair_start_expression_match.capturedStart() == 0 &&
                                          v_register_pair_start_expression_match.capturedEnd() == token_string_size;
                }

                if (!is_scalar_register)
                {
                    is_scalar_register |= s_register_pair_end_expression_match.hasMatch() && s_register_pair_end_expression_match.capturedStart() == 0 &&
                                          s_register_pair_end_expression_match.capturedEnd() == token_string_size;
                }

                if (!is_vector_register)
                {
                    is_vector_register |= v_register_pair_end_expression_match.hasMatch() && v_register_pair_end_expression_match.capturedStart() == 0 &&
                                          v_register_pair_end_expression_match.capturedEnd() == token_string_size;
                }

                // Attempt to exact match register ranges.
                const bool is_scalar_register_range = s_register_range_expression_match.hasMatch() && s_register_range_expression_match.capturedStart() == 0 &&
                                                      s_register_range_expression_match.capturedEnd() == token_string_size;

                const bool is_vector_register_range = v_register_range_expression_match.hasMatch() && v_register_range_expression_match.capturedStart() == 0 &&
                                                      v_register_range_expression_match.capturedEnd() == token_string_size;

                // Attempt to exact match constants.
                const bool is_constant = constant_expression_match.hasMatch() && constant_expression_match.capturedStart() == 0 &&
                                         constant_expression_match.capturedEnd() == token_string_size;

                if (is_scalar_register || is_vector_register || is_scalar_register_range || is_vector_register_range || is_constant)
                {
                    // Single register, range of registers, or a constant.

                    selectable_token.is_selectable = true;
                }

                if (is_scalar_register || is_vector_register)
                {
                    // Single register.

                    std::string register_string = token;

                    if (is_scalar_register)
                    {
                        // Remove 's', '-s', '|s', or '[s'.
                        const auto register_character_position = register_string.find("s");
                        register_string                        = register_string.substr(register_character_position + 1);
                    }
                    else
                    {
                        // Remove 'v', '-v', '|v', or '[v'.
                        const auto register_character_position = register_string.find("v");
                        register_string                        = register_string.substr(register_character_position + 1);
                    }

                    selectable_token.start_register_index = -1;
                    selectable_token.end_register_index   = -1;

                    if (!register_string.empty() && std::isdigit(register_string.front()))
                    {
                        const auto register_number = std::stoi(register_string);

                        selectable_token.start_register_index = register_number;

                        selectable_token.type =
                            is_scalar_register ? IsaItemModel::TokenType::kScalarRegisterType : IsaItemModel::TokenType::kVectorRegisterType;
                    }
                    else
                    {
                        selectable_token.is_selectable = false;
                    }
                }
                else if (is_scalar_register_range || is_vector_register_range)
                {
                    // Range of registers.

                    selectable_token.type =
                        is_scalar_register_range ? IsaItemModel::TokenType::kScalarRegisterType : IsaItemModel::TokenType::kVectorRegisterType;

                    std::string register_range_string = token;

                    register_range_string.pop_back();   // remove ']''
                    register_range_string.erase(0, 2);  // remove 's[' or 'v['

                    std::vector<std::string> register_indices;

                    Split(register_range_string, ":", register_indices);

                    const auto register_start = std::stoi(register_indices[0]);
                    const auto register_end   = std::stoi(register_indices[1]);

                    selectable_token.start_register_index = register_start;
                    selectable_token.end_register_index   = register_end;
                }
                else if (is_constant)
                {
                    // Constant.

                    selectable_token.type = IsaItemModel::TokenType::kConstantType;
                }
            }

            operand_tokens.emplace_back(selectable_token);
        }

        // Set the tokens' hit boxes and increment the next hit box.

        size_t i = 0;

        for (auto& selectable_token : operand_tokens)
        {
            token_width = fixed_character_width * static_cast<qreal>(selectable_token.token_text.size());
            token_end_x += token_width;

            selectable_token.x_position_start = token_start_x;
            selectable_token.x_position_end   = token_end_x;

            if (i < operand_tokens.size() - 1)
            {
                token_start_x = token_start_x + token_width + fixed_character_width;  // Add whitespace width too.
                token_end_x   = token_end_x + fixed_character_width;                  // Add whitespace width too.
            }

            i++;
        }

        selectable_tokens.emplace_back(operand_tokens);

        token_start_x = token_start_x + token_width + (fixed_character_width * static_cast<qreal>(QString(", ").size()));  // Add delimiter width too.
        token_end_x   = token_end_x + (fixed_character_width * static_cast<qreal>(QString(", ").size()));                  // Add delimiter width too.
    }
}

IsaItemModel::Row::Row(RowType type, uint32_t line)
    : row_type(type)
    , line_number(line)
{
}

IsaItemModel::Row::~Row()
{
}

IsaItemModel::CommentRow::CommentRow(uint32_t line, std::string comment)
    : Row(RowType::kComment, line)
    , text(comment)
{
}

IsaItemModel::CommentRow::~CommentRow()
{
}

IsaItemModel::InstructionRow::InstructionRow(uint32_t line, std::string op, std::string address, std::string representation)
    : Row(RowType::kCode, line)
    , pc_address(address)
    , binary_representation(representation)
    , enabled(true)
{
    op_code_token.token_text = op;
}

IsaItemModel::InstructionRow::~InstructionRow()
{
}

IsaItemModel::Block::Block(RowType type, int block_position, uint32_t shader_line_number)
    : row_type(type)
    , position(block_position)
    , line_number(shader_line_number)
{
}

IsaItemModel::Block::~Block()
{
}

IsaItemModel::CommentBlock::CommentBlock(int block_position, uint32_t shader_line_number, std::string comment_text)
    : Block(RowType::kComment, block_position, shader_line_number)
    , text(comment_text)
{
}

IsaItemModel::CommentBlock::~CommentBlock()
{
}

IsaItemModel::InstructionBlock::InstructionBlock(int block_position, uint32_t shader_line_number, std::string block_label)
    : Block(RowType::kCode, block_position, shader_line_number)
{
    token.token_text       = block_label;
    token.type             = IsaItemModel::TokenType::kLabelType;
    token.x_position_start = 0;
    token.x_position_end   = fixed_font_character_width * block_label.size();
}

IsaItemModel::InstructionBlock::~InstructionBlock()
{
}

void IsaItemModel::LoadIsaSpec(amdisa::GpuArchitecture architecture)
{
    const std::string application_dir = qApp->applicationDirPath().toStdString();

    std::filesystem::path isa_spec_dir_path(application_dir);
    isa_spec_dir_path /= "utils";
    isa_spec_dir_path /= "isa_spec";
    isa_spec_dir_path.make_preferred();

    if (std::filesystem::exists(isa_spec_dir_path) && std::filesystem::is_directory(isa_spec_dir_path))
    {
        std::string              initialize_error_message;
        std::vector<std::string> xml_file_paths;

        if (kIsaSpecNameMap.find(architecture) != kIsaSpecNameMap.end())
        {
            const auto isa_spec_name = kIsaSpecNameMap.at(architecture);

            std::filesystem::path isa_spec_path(isa_spec_dir_path);

            isa_spec_path /= isa_spec_name;

            isa_spec_path.make_preferred();

            xml_file_paths.emplace_back(isa_spec_path.string());
        }

        is_decoder_initialized = decode_manager.Initialize(xml_file_paths, initialize_error_message);
    }
}
