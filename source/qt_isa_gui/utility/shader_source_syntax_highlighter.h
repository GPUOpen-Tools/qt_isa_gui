//=============================================================================
// Copyright (c) 2025-2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Definition of a shader source syntax highlighter.
//=============================================================================

#ifndef QTISAGUI_UTILITY_SHADER_SOURCE_SYNTAX_HIGHLIGHTER_H_
#define QTISAGUI_UTILITY_SHADER_SOURCE_SYNTAX_HIGHLIGHTER_H_

// C++
#include <unordered_map>
#include <vector>

// Qt
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

/// @brief Supported textual shader source languages.
enum class ShaderSourceLanguage
{
    kOpenCL,      ///< OpenCL C kernel source.
    kGLSL,        ///< GLSL shader source.
    kHLSL,        ///< HLSL shader source.
    kSPIRV_Text,  ///< SPIR-V text assembly.

    Unknown  ///< Unknown or unspecified language.
};

// Shader Source Syntax Highlighter
//
// The Syntax Highlighter uses single-pass parsing of the text, very similar to lexical parsing.
// When parsing the text, the Highlighter divides it into lexical tokens and tries to match these tokens
// with ones from the "Token Table".
// The Token Table contains language-defined identifiers that must be highlighted (keywords, types, etc.).
// To accelerate matching tokens, the Token Table is organized as a hash-map addressed by the first symbol of the token.
// Each element of the Token Table contains an identifier string and corresponding font style.
//
// Example (OpenCL):
//
// -----
// | a |
// |---|
// | b |--> "bool" --> "bool2" --> "bool3" --> ...
// |---|
// | c |--> "const" --> "case" --> "continue" --> "char" --> ...
// |---|
// | d |--> "do" --> "default" --> "double" --> ...
// |---|
//   .
//   .
//   .
//

/// @brief Represents a single word in a shader source line with its highlight color.
struct ShaderLineWord
{
    int    start_char_index = 0;  ///< Character index of the start of the word within the line.
    int    length           = 0;  ///< Length of the word in characters.
    QColor color;                 ///< Highlight color to apply to the word.
};

/// @brief A list of highlighted words in a shader source line.
using ShaderLineWords = std::vector<ShaderLineWord>;

class ShaderSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    // -- Types --

    /// @brief Syntax highlighting style defining colors for each token category.
    struct Style
    {
        QTextCharFormat keywords;   ///< Format for language keywords.
        QTextCharFormat types;      ///< Format for language types.
        QTextCharFormat functions;  ///< Format for function and macro names.
        QTextCharFormat comments;   ///< Format for comments.
        QTextCharFormat strings;    ///< Format for string literals.
        QTextCharFormat preproc;    ///< Format for preprocessor directives.
    };

    /// @brief A language descriptor containing the token lists and comment specifiers for a language.
    struct LangDesc
    {
        // List of language keywords.
        std::vector<QString> keywords;

        // List of keyword prefixes. All tokens that start with these prefixes will be highlighted as keywords.
        std::vector<QString> keyword_prefixes;

        // List of language types.
        std::vector<QString> types;

        // List of preprocessor directives.
        std::vector<QString> preproc;

        /// @brief Comment delimiter strings for the language.
        struct
        {
            QString single_line_start;  ///< Token that starts a single-line comment (e.g. "//").
            QString multi_line_start;   ///< Token that starts a multi-line comment (e.g. "/*").
            QString multi_line_end;     ///< Token that ends a multi-line comment (e.g. "*/").
        } comments;
    };

    // -- Methods --

    /// @brief Default constructor; uses GLSL language and the default style.
    ShaderSyntaxHighlighter()
        : ShaderSyntaxHighlighter(nullptr, ShaderSourceLanguage::kGLSL, GetDefaultStyle())
    {
    }

    /// @brief Constructor; uses the default style.
    ///
    /// @param [in] pDoc The text document to attach the highlighter to.
    /// @param [in] lang The shader source language to use for highlighting.
    ShaderSyntaxHighlighter(QTextDocument* pDoc, ShaderSourceLanguage lang)
        : ShaderSyntaxHighlighter(pDoc, lang, GetDefaultStyle())
    {
    }

    /// @brief Constructor.
    ///
    /// @param [in] pDoc  The text document to attach the highlighter to.
    /// @param [in] lang  The shader source language to use for highlighting.
    /// @param [in] style The syntax highlighting style to apply.
    ShaderSyntaxHighlighter(QTextDocument* pDoc, ShaderSourceLanguage lang, const Style& style);

    /// @brief Destructor.
    ~ShaderSyntaxHighlighter() = default;

    /// @brief Replace the current syntax highlighting style.
    ///
    /// @param [in] style The new style to apply.
    void SetStyle(const Style& style);

    /// @brief Parse a single line of shader source text and return its highlighted words.
    ///
    /// @param [in] text The line of shader source text to parse.
    ///
    /// @return A list of words with their start index, length, and highlight color.
    std::vector<ShaderLineWord> ParseShaderLine(const QString& text);

private:
    // -- Methods --

    // This function is called by the Qt framework when a fragment of text needs to be reformatted.
    void highlightBlock(const QString& text) override final;

    // Return the default style.
    static Style GetDefaultStyle();

    // Process string. Highlights a string enclosed in quotes (') or double quotes (").
    // Returns offset of first symbol after the closing quote.
    int ProcessString(const QString& text, int offset, bool double_quote);

    // Process a function or macro name. Returns "true" and highlight the token if it's a function or macro name.
    // Returns "false" otherwise.
    // "endOffset" is the offset of the last symbol in the token.
    bool ProcessFuncName(const QString& text, int begin_offset, int end_offset, ShaderLineWords& highlighted_words);

    // Process comment. Highlights a single-line comment or a part of multi-line comment belonging to the current block.
    // Returns offset of first symbol after the comment end.
    int ProcessComment(const QString& text, int offset, bool is_multi_line);

    // Process general token. Highlights the token between "begin" and "end" offsets if it matches with any
    // token from the Token Table.
    // Returns "true" if the token was recognized and highlighted.
    bool ProcessToken(const QString& text, int begin_offset, int end_offset, ShaderLineWords& highlighted_words);

    // Initialize the Token Table with language elements from corresponding LangDesc and Style structures.
    void InitTokens(ShaderSourceLanguage lang);

    // -- Types & Data --

    /// @brief An element of the token table: a lexical token string and its corresponding text format.
    struct Token
    {
        QString         token;   ///< The token string to match.
        QTextCharFormat format;  ///< The text format to apply when the token is matched.
    };

    /// @brief Hash map from a token's first character to the list of tokens starting with that character.
    typedef std::unordered_map<QChar, std::vector<Token>> TokenTable;

    LangDesc   lang_desc_;    ///< The language descriptor for the current language.
    TokenTable token_table_;  ///< The token table used for fast token lookup during highlighting.
    Style      style_;        ///< The current syntax highlighting style.
};
#endif  // QTISAGUI_UTILITY_SHADER_SOURCE_SYNTAX_HIGHLIGHTER_H_
