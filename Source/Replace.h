#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "LineDiff.h"
#include "Pattern.h"
#include "Stdio.h"
#include "TextDocument.h"

/** @brief Implements the `saw replace` subcommand for literal and regex search-and-replace.
 */
class Replace
{
public:
    /** @brief Parses the argument list of replace, with its flags and value options.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (juce::Span<const std::string_view> argumentList);

    /** @brief Runs replace on parsed arguments.
     *
     *  A failed parse is returned as a failure. The replacement inputs and the paths are then
     *  validated, and the replacement is previewed, or applied with `--apply`.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The standard input, the null device, or a supplied text.
     *
     *  @return The exit code, the output documents, and the failures.
     */
    static Stdio::Output run (const Arguments& arguments, const Input::Source& input);

private:
    /** @brief The document of one file, its diff, and its number of replacements. */
    using Replacement = std::tuple<juce::File, std::unique_ptr<TextDocument>, LineDiff, int>;

    /** @brief Binds the pattern, the replacement source, and the line range of one replace command.
     *
     *  @param arguments              The parsed arguments. The regex flag is read here.
     *  @param newPattern             The compiled search pattern.
     *  @param newReplacementDocument The document that owns the replacement bytes when they come from --with-file, otherwise nullptr.
     *  @param newReplacementInput    The input that owns the replacement bytes when they come from --with-stdin, otherwise nullptr.
     *  @param newReplacementText     The replacement text, a view of one of the owners or of the arguments.
     *  @param newReplacementTokens   The tokens of the replacement text, used when the pattern is a regex.
     *  @param newLineRange           One-based half-open range of lines that the command covers.
     */
    Replace (const Arguments& arguments, std::unique_ptr<Pattern> newPattern, std::unique_ptr<TextDocument> newReplacementDocument, std::unique_ptr<Input> newReplacementInput, std::string_view newReplacementText, jam::Array<std::pair<int, juce::Range<int>>> newReplacementTokens, juce::Range<int> newLineRange);

    /** @brief Diffs standard input as one document, with no target file.
     *
     *  Standard input has no file to write. `--apply` is refused before any diff is built.
     *
     *  @param arguments The parsed arguments of the command.
     *  @param input     The standard input, the null device, or a supplied text.
     *
     *  @return The exit code, the output documents, and the failures.
     */
    Stdio::Output runStandardInput (const Arguments& arguments, const Input::Source& input) const;

    /** @brief Reads and diffs each file in its own job, then stages the replacements in file order.
     *
     *  The first failure is reported before any file is staged, so a failed read or diff stages nothing.
     *
     *  @param arguments The parsed arguments of the command.
     *  @param rawPaths  The paths that the user named.
     *  @param source    The source of the input, passed to TextDocument::getInput for each file.
     *
     *  @return The exit code, the output documents, and the failures.
     */
    Stdio::Output runFiles (const Arguments& arguments, juce::Span<const std::string_view> rawPaths, const Input::Source& source) const;

    /** @brief Reads one file, and builds its diff only when the pattern has a candidate in the file.
     *
     *  A binary file gives the binary status. A file without a candidate gives no replacement.
     *
     *  @param targetFile  The file to read.
     *  @param displayPath The display path of the file.
     *  @param isExplicit  True when the file was named on the command line.
     *  @param source    The source of the input, passed to TextDocument::getInput.
     *
     *  @return The result, and the replacement when the file has one.
     */
    std::pair<juce::Result, std::optional<Replacement>> getReplacedFile (juce::File targetFile, std::string_view displayPath, bool isExplicit, const Input::Source& source) const;

    /** @brief Builds one change for each line that has a match, within the line range.
     *
     *  The candidate search skips the lines that cannot match. Each replaced line is one change of one line.
     *  The loop stops at the first candidate past the end of the line range.
     *
     *  @param document       The document to diff.
     *  @param firstCandidate Byte offset of the first candidate line of the document, found by the caller.
     *  @param automaton      Matcher state that the caller owns.
     *
     *  @return The result, the diff, and the number of replacements.
     */
    std::tuple<juce::Result, LineDiff, int> getDocumentDiff (const TextDocument& document, size_t firstCandidate, jam::RegExpAutomaton& automaton) const;

    /** @brief Writes a line with each match replaced, and reports the number of replacements.
     *
     *  After an empty match, the search moves forward by one code point, so a character is never split
     *  and the same position is not matched again.
     *
     *  @param output The text that receives the replaced line.
     *  @param line   The line to replace, without its line ending.
     *
     *  @return The result, and the number of matches that were replaced.
     */
    std::pair<juce::Result, int> appendReplacedLine (std::string& output, std::string_view line) const;

    /** @brief Appends the replacement of one match: the replacement text, or the replacement tokens with the captured groups.
     *
     *  @param output   The text that receives the replacement.
     *  @param captures The captured groups of the match. Group 0 is the whole match.
     */
    void appendExpandedReplacement (std::string& output, const Pattern::Captures& captures) const;

    /** @brief The compiled search pattern. */
    std::unique_ptr<Pattern> pattern;

    /** @brief Owns the replacement bytes when they come from --with-file, and is null otherwise. */
    std::unique_ptr<TextDocument> replacementDocument;

    /** @brief Owns the replacement bytes when they come from --with-stdin, and is null otherwise. */
    std::unique_ptr<Input> replacementInput;

    /** @brief The replacement text. It is a view of replacementDocument, replacementInput, or the arguments. */
    std::string_view replacementText;

    /** @brief Tokens of the replacement text: a group index with its byte range, or noCapture with the literal bytes. */
    jam::Array<std::pair<int, juce::Range<int>>> replacementTokens;

    /** @brief One-based half-open range of lines that the replacement covers. */
    juce::Range<int> lineRange;

    /** @brief True when the pattern is a regular expression, so that the replacement tokens expand the captures. */
    bool isRegex { false };
};
