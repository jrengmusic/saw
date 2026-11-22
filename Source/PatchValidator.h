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
};
