#pragma once
#include <JuceHeader.h>
#include "TextDocument.h"

/** @brief Validates JSON patch schema, entry shapes, file bounds, line hashes, and edit conflicts.
 */
struct PatchValidator : jam::JsonValidator
{
    /** @brief Registers base JSON rules and the patch array schema rule in @p rules.
     *
     *  @param rules Validator rule map populated for patch documents.
     */
    static void addRules (Rules& rules);

    /** @brief Returns the cached validator rule map used by `jam::JsonValidator::isValid()`.
     *
     *  @return Reference to the static patch validation rules.
     */
    const Rules& getRules() const override;

    /** @brief Checks bounds, optional line hashes, and intra-file conflicts for one target file.
     *
     *  @param document       Schema-validated patch JSON document.
     *  @param entryIndices   Zero-based entry indices in the root array targeting @p targetDocument.
     *  @param targetDocument Loaded target text document against which entries are checked.
     *
     *  @return `juce::Result::ok()` when all entries apply cleanly, or failure describing the conflict.
     */
    static juce::Result isValidForFile (const jam::JsonDocument& document,
                                        const jam::Array<int>& entryIndices,
                                        const TextDocument& targetDocument);

    /** @brief Returns the integer value of a number member of an entry.
     *
     *  The member must already be validated by the schema. This call does not check its type.
     *
     *  @param document The schema-validated patch document.
     *  @param object   The entry object that holds the member.
     *  @param key      The member name.
     *
     *  @return The integer value of the member.
     */
    static int getIntMember (const jam::JsonDocument& document,
                             const jam::Document::Element& object,
                             std::string_view key);

    /** @brief Returns the one-based line range that an entry changes, as a half-open range.
     *
     *  An entry with `line` changes that single line. An entry with `startLine` and `endLine`
     *  changes the lines from the first to the last, both included. The end is exclusive.
     *
     *  @param document The schema-validated patch document.
     *  @param object   The entry object.
     *
     *  @return The half-open range `[start, end)` of one-based line numbers.
     */
    static juce::Range<int> getEntryLineRange (const jam::JsonDocument& document,
                                               const jam::Document::Element& object);

    /** @brief Returns the key that names the shape of an entry.
     *
     *  A deletion entry has the key `delete`, and no column or insert key. Otherwise the shape
     *  is the one key among `startCol`, `startLine`, `insertAfter`, and `insertBefore`.
     *
     *  @param document The schema-validated patch document.
     *  @param object   The entry object.
     *
     *  @return The shape key, or a null Identifier when no single shape fits the entry.
     */
    static juce::Identifier getEntryShapeKey (const jam::JsonDocument& document,
                                              const jam::Document::Element& object);
};
