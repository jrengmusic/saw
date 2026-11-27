#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Pattern.h"
#include "Stdio.h"

/** @brief Implements the `saw fields` subcommand for extracting delimited columns from lines.
 */
class Fields
{
public:
    /** @brief Parses the argument list of fields, with its flags and value options.
     *
     *  @param argumentList The argument list, with the verb name at index 0.
     *
     *  @return The parsed arguments. Their status holds any parse failure.
     */
    static Arguments getArguments (juce::Span<const std::string_view> argumentList);

    /** @brief Runs fields on parsed arguments.
     *
     *  The first positional is the field specification, and the others are paths. The field settings and the filter settings are checked first,
     *  and a failed check is returned as a failure. Without paths, standard input is read. With paths, each file is read.
     *
     *  @param arguments The parsed arguments from getArguments.
     *  @param input     The standard input, the null device, or a supplied text.
     *
     *  @return The exit code, the output documents, and the failures.
     */
    static Stdio::Output run (const Arguments& arguments, const Input::Source& input);

private:
    /** @brief Binds the parsed field settings and filter settings of one fields command.
     *
     *  @param arguments       The parsed arguments. The delimiter, the output delimiter, and the flags are read here.
     *  @param newFieldIndices The one-based field indexes that the output selects, in output order. A negative index counts from the end.
     *  @param newLineRange    One-based half-open range of lines that the command covers.
     *  @param newAfterLine    Lines up to and including this line are not matched or printed.
     *  @param newMatchPattern The pattern that a line must match, or nullptr when every line is printed.
     */
    Fields (const Arguments& arguments, jam::Array<int> newFieldIndices, juce::Range<int> newLineRange, int newAfterLine, std::unique_ptr<Pattern> newMatchPattern);

    /** @brief Reads standard input as one text, and refuses a terminal.
     *
     *  Only the process standard input can be a terminal. A terminal is refused, so the command does not wait for typed text.
     *
     *  @param input The standard input, the null device, or a supplied text.
     *
     *  @return The exit code, the output documents, and the failures.
     */
    Stdio::Output runStandardInput (const Input::Source& input) const;

    /** @brief Reads each file in its own job, and writes the output of each file into its own slot.
     *
     *  The slots keep the order of the sorted file entries. No lock is needed, because each job writes one slot.
     *
     *  @param rawPaths The paths that the user named.
     *  @param source   The source of the input, passed to TextDocument::getInput for each file.
     *
     *  @return The exit code, one document group for each file in file order, and the failures.
     */
    Stdio::Output runFiles (juce::Span<const std::string_view> rawPaths, const Input::Source& source) const;

    /** @brief Reads one file and returns its output, whether any line matched, and the read failure.
     *
     *  @param file        The file to read.
     *  @param displayPath The display path of the file.
     *  @param isExplicit  True when the file was named on the command line.
     *  @param source      The source of the input, passed to TextDocument::getInput.
     *
     *  @return The status, whether any line matched, and the output documents.
     */
    std::tuple<juce::Result, bool, Stdio::Documents> getFileOutput (const juce::File& file, std::string_view displayPath, bool isExplicit, const Input::Source& source) const;

    /** @brief Formats the selected lines of the bytes. A binary input gives no output unless --text is set.
     *
     *  The NUL check covers only the bytes that the line loop read. With isFirstOnly, the loop stops at the
     *  first matching line, so a NUL byte after that line is not checked.
     *
     *  @param bytes       The bytes to format.
     *  @param displayPath The display path of the input.
     *  @param isExplicit  True when the input was named as a file on the command line.
     *
     *  @return The result, whether any line matched, and the output documents.
     */
    std::tuple<juce::Result, bool, Stdio::Documents> getBytesOutput (std::string_view bytes, std::string_view displayPath, bool isExplicit) const;

    /** @brief Matches one line, and when it matches, splits and formats it.
     *
     *  @param output     The text that receives the formatted line.
     *  @param tokens     Scratch array that receives the fields of the line. Its previous content is replaced.
     *  @param bytes      The bytes of the whole input, which give the line ending of the line.
     *  @param line       The line bytes, without the line ending.
     *  @param lineNumber The one-based number of the line.
     *  @param displayPath The display path of the input, used in the failure text.
     *  @param automaton  Matcher state that the caller owns.
     *
     *  @return The status, and whether the line was matched and formatted.
     */
    std::pair<juce::Result, bool> appendFields (std::string& output, jam::Array<std::string_view>& tokens, std::string_view bytes, std::string_view line, int lineNumber, std::string_view displayPath, jam::RegExpAutomaton& automaton) const;

    /** @brief Splits a line into fields, by the delimiter, or by runs of whitespace when no delimiter is given.
     *
     *  @param tokens Receives the fields. It is cleared first, and an empty line gives no fields.
     *  @param line   The line bytes, without the line ending.
     */
    void setLineTokens (jam::Array<std::string_view>& tokens, std::string_view line) const;

    /** @brief Appends the selected fields, joined by the output delimiter, and the line ending.
     *
     *  An empty line is printed as its line ending alone, unless strict mode is set. A field outside the line is a failure in strict mode.
     *
     *  @param output      The text that receives the line.
     *  @param tokens      The fields of the line.
     *  @param displayPath The display path of the input, used in the failure text.
     *  @param lineNumber  The one-based number of the line, used in the failure text.
     *  @param ending     The line ending that ends the output line.
     *
     *  @return Ok when every selected field was written, otherwise the failure.
     */
    juce::Result appendFormattedLine (std::string& output, const jam::Array<std::string_view>& tokens, std::string_view displayPath, int lineNumber, std::string_view ending) const;

    /** @brief Returns the field at a one-based index, or at a negative index counted from the end.
     *
     *  A field outside the line is a failure only in strict mode. Otherwise the field is empty.
     *
     *  @param requested   The requested index. Zero is not a field.
     *  @param tokens      The fields of the line.
     *
     *  @return The field text, an empty view outside the line when not strict, or nullopt outside the line when strict.
     */
    std::optional<std::string_view> getSelectedField (int requested, const jam::Array<std::string_view>& tokens) const;

    /** @brief One-based field indexes that the output selects, in output order. */
    jam::Array<int> fieldIndices;

    /** @brief The delimiter of the input. Nullopt selects splitting on runs of whitespace. */
    std::optional<std::string_view> delimiter;

    /** @brief The text written between two selected fields. */
    std::string_view outputDelimiter;

    /** @brief One-based half-open range of lines that the command covers. */
    juce::Range<int> lineRange;

    /** @brief Lines up to and including this line are not matched or printed. */
    int afterLine { 0 };

    /** @brief The pattern that a line must match, or nullptr when every line is printed. */
    std::unique_ptr<Pattern> matchPattern;

    /** @brief True when the search stops at the first matching line. */
    bool isFirstOnly { false };

    /** @brief True when a field outside the line is a failure rather than an empty field. */
    bool isStrict { false };

    /** @brief True when binary input is formatted rather than reported. */
    bool isText { false };
};
