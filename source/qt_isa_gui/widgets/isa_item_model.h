//=============================================================================
// Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for an isa item model.
//=============================================================================

#ifndef QTISAGUI_ISA_ITEM_MODEL_H_
#define QTISAGUI_ISA_ITEM_MODEL_H_

#include <QAbstractItemModel>
#include <QColor>
#include <QFont>

#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "amdisa/isa_decoder.h"

class IsaTreeView;

/// @brief IsaItemModel is an item model that stores shader isa and comments, intended to be displayed in a tree view.
///
/// It supports 1 level of parenting hierarchy.
/// A parent row can be an isa instruction block or a comment block.
/// A child row can be an instruction or a comment.
class IsaItemModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    static const QString     kColumnPadding;              ///< Padding for columns.
    static const QString     kOpCodeColumnIndent;         ///< Indent for op codes column.
    static const QString     kOperandTokenSpace;          ///< Space in between tokens within the same operand.
    static const QString     kOperandDelimiter;           ///< Delimiter to separate operands.
    static const std::string kUnconditionalBranchString;  ///< Branch op code text.
    static const std::string kConditionalBranchString;    ///< Conditional branch op code text.

    /// @brief Predefined columns.
    enum Columns
    {
        kLineNumber = 0,
        kPcAddress,
        kOpCode,
        kOperands,
        kBinaryRepresentation,
        kColumnCount
    };

    static const std::array<std::string, kColumnCount> kColumnNames;  ///< Predefined column headers.

    /// @brief Extend user roles to store: branch instruction metadata, color coding flag,
    ///        and row type.
    enum UserRoles
    {
        kLabelBranchRole = Qt::UserRole + 1,  // true if the index is a code block label and the target of a branch instruction, otherwise false.
        kBranchIndexRole,                     // List of source model indices of the corresponding branch instruction or label.
        kLineEnabledRole,                     // true if the index is an instruction that should be color coded, false if it should not be color coded.
        kRowTypeRole,                         // Type of row the index is from.
        kDecodedIsa,
        kUserRolesCount
    };

    /// @brief Predefined row types.
    enum class RowType
    {
        kCode = 0,  ///< Parent code block or child instruction.
        kComment,   ///< Parent comment block or child comment.
        kRowCount
    };

    /// @brief Predefined types to assist formatting and user interaction with tokens.
    enum class TokenType
    {
        kLabelType = 0,       ///< Code block label in op code column.
        kBranchLabelType,     ///< Label targeted by a branch instruction in operands column.
        kScalarRegisterType,  ///< Scalar operand.
        kVectorRegisterType,  ///< Vector operand.
        kConstantType,        ///< Constant operand.
        kTypeCount
    };

    /// @brief Token is a convenience struct intended to act as a temporary representation of a single word of isa.
    ///        It assists color coding and user interaction, like selecting/highlighting.
    typedef struct Token
    {
        std::string token_text;            ///< The token's isa text.
        TokenType   type;                  ///< The type of this token.
        int         start_register_index;  ///< The starting register index if this token represents a register.
        int         end_register_index;    ///< The ending register index if this token represents a register.
        qreal       x_position_start;      ///< The token's starting x view position.
        qreal       x_position_end;        ///< The token's ending x view position.
        bool        is_selectable;         ///< true if the token can be selected, false otherwise.

        /// @brief Constructor; create an empty token.
        Token()
        {
            Clear();
        }

        /// @brief Clear this token so it is empty and does not represent anything.
        void Clear()
        {
            token_text           = "";
            type                 = TokenType::kTypeCount;
            start_register_index = -1;
            end_register_index   = -1;
            x_position_start     = -1;
            x_position_end       = -1;
            is_selectable        = false;
        }
    } Token;

    /// @brief Constructor.
    ///
    /// Allow clients to provide a pre-initialized pointer to the decode manager. If none is provided, this constructor will initialize one for the clients of this class.
    ///
    /// @param [in] parent         The parent object.
    /// @param [in] decode_manager The optional handle to manager of all the architectures.
    IsaItemModel(QObject* parent = nullptr, amdisa::DecodeManager* decode_manager = nullptr);

    /// @brief Destructor.
    ~IsaItemModel();

    /// @brief Override column count to provide a fixed number of shared columns.
    ///
    /// @param [in] parent The parent index.
    ///
    /// @return The number of columns for the given index.
    virtual int columnCount(const QModelIndex& parent = QModelIndex()) const Q_DECL_OVERRIDE;

    /// @brief Override row count to provide a dynamic number of rows.
    ///
    /// The number of top level rows in this model is equivalent to the number of code blocks + comment blocks in this model.
    /// The number of rows in any given block is the number of instructions + comments in that block.
    ///
    /// @param [in] parent The parent index.
    ///
    /// @return The number of rows for the given index.
    int rowCount(const QModelIndex& parent = QModelIndex()) const Q_DECL_OVERRIDE final;

    /// @brief Override index to generate the correct index for the given row, column, and parent index.
    ///
    /// Parent blocks are assigned an index with no internal data.
    /// Child rows are assigned an index with internal data that is a pointer to their parent block.
    ///
    /// @param [in] row    The row.
    /// @param [in] column The column.
    /// @param [in] parent The parent index.
    ///
    /// @return The generated index.
    virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const Q_DECL_OVERRIDE;

    /// @brief Override parent to assign parents.
    ///
    /// Blocks are parented to the root node.
    /// Instructions and comments are parented to their block.
    ///
    /// @param [in] index The index to generate a parent for.
    ///
    /// @return The parent index.
    virtual QModelIndex parent(const QModelIndex& index) const Q_DECL_OVERRIDE;

    /// @brief Override headerData to provide data to display in the header.
    ///
    /// Only defines display role text to display for a given column.
    ///
    /// @param [in] section     The section number.
    /// @param [in] orientation The orientation.
    /// @param [in] role        The role.
    ///
    /// @return The variant to use as header data.
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;

    /// @brief Override data to provide data for a given index.
    ///
    /// @param [in] index The index.
    /// @param [in] role  The role.
    ///
    /// @return The variant to use as data.
    virtual QVariant data(const QModelIndex& index, int role) const Q_DECL_OVERRIDE;

    /// @brief UpdateData is a pure virtual function intended to be overridden by child classes to update the internal state of this model.
    ///
    /// This is intended to be the primary interface function to update the text in this model.
    /// A (void*) parameter is supplied so that an application may pass in any type at their convenience.
    ///
    /// @param [in] data A pointer to application metadata.
    virtual void UpdateData(void* data) = 0;

    /// @brief Cache the sizes of shared columns by using the text in this model and the font being used by an isa tree.
    ///
    /// Also, map line numbers to their source code block indices.
    virtual void CacheSizeHints();

    /// @brief Get the cached column size for the requested column index and tree.
    ///
    /// @param [in] column_index The column index.
    /// @param [in] tree         The tree view.
    ///
    /// @return The cached column size.
    virtual QSize ColumnSizeHint(int column_index, IsaTreeView* tree) const;

    /// @brief Cache the fixed font being used by attached trees. Use it to efficiently measure isa text from here on out.
    ///
    /// @param [in] fixed_font The fixed font to use.
    /// @param [in] tree       The attached tree view; use it to achieve more accurate isa text measurement.
    void SetFixedFont(const QFont& fixed_font, IsaTreeView* tree);

    /// @brief Specify an architecture to be used by the decoder; used to provide op code tooltip.
    ///
    /// @param [in] architecture  The architecture version.
    /// @param [in] load_isa_spec true to load the isa spec in the decoder.
    void SetArchitecture(amdisa::GpuArchitecture architecture, bool load_isa_spec = true);

    /// @brief Get the source model index that corresponds to the provided line number.
    ///
    /// @param [in] line_number The line number.
    ///
    /// @return The source model index that corresponds to the provided line number.
    QModelIndex GetLineNumberModelIndex(int line_number);

    /// @brief Get the number of lines stored in this model; parent code blocks and child rows both count towards the line count.
    ///
    /// @return The number of lines in the model.
    inline int GetLineCount() const
    {
        return static_cast<int>(line_number_corresponding_indices_.size());
    }

    /// @brief Toggles the line_numbers_visible_ variable true and false. Used to know if line numbers should be drawn.
    void ToggleLineNumbers()
    {
        line_numbers_visible_ = !line_numbers_visible_;
    }

    /// @brief Gets whether line numbers are visible.
    ///
    /// @return true if line numbers are visible, false otherwise.
    inline bool LineNumbersVisible() const
    {
        return line_numbers_visible_;
    }

    /// @brief Get the width of a single character of the fixed font used by the isa tree and this model.
    ///
    /// @return The width of a single character of isa shown in an isa tree.
    inline qreal GetFixedFontCharacterWidth() const
    {
        return fixed_font_character_width_;
    }

    /// @brief Helper function to assist parsing an isa instruction.
    ///        Removes special characters from the input string.
    ///
    /// @param [in] input_string The string to trim.
    ///
    /// @return The trimmed string.
    static std::string TrimStr(std::string input_string);

    /// @brief Helper function to assist parsing an isa instruction.
    ///        Split the input string using the provided delimiter.
    ///
    /// @param line      [in]  The string to split.
    /// @param delimiter [in]  The delimiter to use to split the string.
    /// @param list      [out] The container of split string tokens.
    static void Split(std::string line, std::string delimiter, std::vector<std::string>& list);

signals:
    /// @brief Signal to notify the delegate that the gpu architecture has changed.
    ///
    /// @param [in] successful true if we were successful in getting the decoder for that architecutre, false otherwise.
    void ArchitectureChanged(bool successful);

protected:
    /// @brief Clear the existing branch instruction to label mapping for all blocks in this model.
    void ClearBranchInstructionMapping();

    /// @brief Map code blocks indices to corresponding jump instruction indices.
    virtual void MapBlocksToBranchInstructions();

    /// @brief Helper function to parse an isa instruction to find which tokens can be selected/highlighted.
    ///
    /// @param [in]  op_code               The op code string.
    /// @param [out] op_code_token         The op code's token, to be parsed and filled in.
    /// @param [in]  operands              The operand strings.
    /// @param [out] selectable_tokens     The operands' tokens, to be parsed and filled in.
    /// @param [in]  fixed_character_width The width of a single character from the fixed font being used by this model and view.
    void ParseSelectableTokens(const std::string&                             op_code,
                               IsaItemModel::Token&                           op_code_token,
                               const std::vector<std::string>&                operands,
                               std::vector<std::vector<IsaItemModel::Token>>& selectable_tokens,
                               qreal                                          fixed_character_width) const;

    /// @brief Row is an abstract class intended to serve as the interface for a single child row in this model.
    class Row
    {
    public:
        /// @brief Constructor.
        ///
        /// @param [in] type The type of the row.
        /// @param [in] line The line number.
        Row(RowType type, uint32_t line);

        /// @brief Pure destructor to force subclass implementations.
        virtual ~Row() = 0;

        RowType  row_type;     ///< The type of this row.
        uint32_t line_number;  ///< Line # relative to the entire shader.
    };

    /// @brief CommentRow is a convenience class meant to represent 1 line of comment, as a child row.
    class CommentRow final : public Row
    {
    public:
        /// @brief Constructor.
        ///
        /// @param [in] line    The line number.
        /// @param [in] comment The comment/text.
        CommentRow(uint32_t line, std::string comment);

        /// @brief Destructor.
        virtual ~CommentRow();

        std::string text;  ///< The text of this comment.
    };

    /// @brief InstructionRow is a convenience class meant to represent 1 line of instruction, as a child row.
    class InstructionRow final : public Row
    {
    public:
        /// @brief Constructor.
        ///
        /// @param [in] line           The line number.
        /// @param [in] op             The op code text of this instruction.
        /// @param [in] address        The pc_address text of this instruction.
        /// @param [in] representation The binary representation text of this instruction.
        InstructionRow(uint32_t line, std::string op, std::string address, std::string representation);

        /// @brief Destructor.
        virtual ~InstructionRow();

        Token                           op_code_token;   ///< This instruction's opcode's token.
        std::vector<std::vector<Token>> operand_tokens;  ///< This instruction's operands' tokens; tokens belonging to the same operand are grouped together.
        std::string                     pc_address;      ///< The pc address text of this instruction.
        std::string                     binary_representation;  ///< The binary representation text of this instruction.
        bool                            enabled;                ///< true if this instruction should be color coded, false otherwise.
    };

    /// @brief Block is an abstract class intended to serve as the interface for a single parent block in this model.
    class Block
    {
    public:
        /// @brief Constructor.
        ///
        /// @param [in] type               The type of this row.
        /// @param [in] block_position     This block's position in the current shader.
        /// @param [in] shader_line_number This blocks line number relative to the entire shader.
        Block(RowType type, int block_position, uint32_t shader_line_number);

        Block()                        = delete;
        Block(const Block&)            = delete;
        Block(Block&&)                 = delete;
        Block& operator=(const Block&) = delete;
        Block& operator=(Block&&)      = delete;

        /// @brief Pure destructor to force subclass implementations.
        virtual ~Block() = 0;

        RowType                           row_type;           ///< The type of this row.
        int                               position;           ///< This code block's index into this model's data structure.
        uint32_t                          line_number;        ///< Line # relative to the entire shader.
        std::vector<std::shared_ptr<Row>> instruction_lines;  ///< All instruction lines that belong to this code block.
    };

    /// @brief CommentBlock is a convenience class meant to represent a block of comments.
    ///
    /// CommentBlock is intended to hold only comment rows.
    class CommentBlock final : public Block
    {
    public:
        /// @brief Constructor.
        ///
        /// @param [in] block_position     This block's position in the current shader.
        /// @param [in] shader_line_number This blocks line number relative to the entire shader.
        /// @param [in] comment_text       This comment block's label.
        CommentBlock(int block_position, uint32_t shader_line_number, std::string comment_text);

        CommentBlock()                               = delete;
        CommentBlock(const CommentBlock&)            = delete;
        CommentBlock(CommentBlock&&)                 = delete;
        CommentBlock& operator=(const CommentBlock&) = delete;
        CommentBlock& operator=(CommentBlock&&)      = delete;

        /// @brief Destructor.
        virtual ~CommentBlock();

        std::string text;  ///< The text of this comment block.
    };

    /// @brief InstructionBlock is a convenience class meant to represent an isa code block.
    ///
    /// InstructionBlock is intended to hold both comment rows and instruction rows.
    class InstructionBlock final : public Block
    {
    public:
        /// @brief Constructor.
        ///
        /// @param [in] block_position     This block's position in the current shader.
        /// @param [in] shader_line_number This blocks line number relative to the entire shader.
        /// @param [in] block_label        This instruction block's label.
        InstructionBlock(int block_position, uint32_t shader_line_number, std::string block_label);

        InstructionBlock()                                   = delete;
        InstructionBlock(const InstructionBlock&)            = delete;
        InstructionBlock(InstructionBlock&&)                 = delete;
        InstructionBlock& operator=(const InstructionBlock&) = delete;
        InstructionBlock& operator=(InstructionBlock&&)      = delete;

        /// @brief Destructor.
        ~InstructionBlock();

        Token token;  ///< This block's label's token.

        std::vector<std::pair<uint32_t, uint32_t>> mapped_branch_instructions;  ///< Map this block to corresponding jump instruction indices.
    };

    std::vector<std::shared_ptr<Block>> blocks_;  ///< Isa stored in this model as a container of a convenience data structure.

    std::unordered_map<std::string, int> code_block_label_to_index_;  ///< Map code block label names to their index into all blocks.

    QFont fixed_font_;                  ///< A fixed font set by an application to assist caching size hints for columns.
    qreal fixed_font_character_width_;  ///< Cache the width of a single character of the fixed font.
    bool  line_numbers_visible_;        ///< Whether the line numbers are to be shown.

private:
    /// @brief Load isa spec file for a given architecture.
    ///
    /// @param [in] architecture The architecture enum for which the individual isa spec is to be loaded.
    void LoadIsaSpec(amdisa::GpuArchitecture architecture = amdisa::GpuArchitecture::kUnknown);

    std::array<uint32_t, kColumnCount> column_widths_ = {0, 0, 0, 0, 0};  ///< Cached column widths.

    std::vector<std::pair<uint32_t, uint32_t>>
        line_number_corresponding_indices_;  ///< Map line numbers to their corresponding source model indices; Line number to <parent row, child row>.

    amdisa::DecodeManager* decode_manager_;  ///< The handle to manager of all the architectures.
};

Q_DECLARE_METATYPE(IsaItemModel::RowType);
Q_DECLARE_METATYPE(IsaItemModel::Token);
Q_DECLARE_METATYPE(std::string);
Q_DECLARE_METATYPE(amdisa::InstructionInfoBundle);

#endif  // QTISAGUI_ISA_ITEM_MODEL_H_
