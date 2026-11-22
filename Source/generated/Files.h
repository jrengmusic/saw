/*******************************************************************************
                        Codegen Annotated Source of Truth
————————————————————————————————————————————————————————————————————————————————

            ░░████████████░░████████████░░████████████░░████████████
            ░░████  ░░████░░████  ░░████░░████  ░░████    ░░████
            ░░████        ░░████  ░░████░░████            ░░████
            ░░████        ░░████████████░░████████████    ░░████
            ░░████        ░░████  ░░████        ░░████    ░░████
            ░░████  ░░████░░████  ░░████░░████  ░░████    ░░████
            ░░████████████░░████  ░░████░░████████████    ░░████

————————————————————————————————————————————————————————————————————————————————
                         FOR YOUR EYES ONLY, DO NOT EDIT
********************************************************************************/

/**
 * @file Files.h
 * @brief saw's own file names, referenced by the program.
 */

#pragma once

namespace files
{
/*_____________________________________________________________________________*/

/** @brief File names saw reads: help texts. */

extern const juce::String sawHelp;    ///< Rendered top-level help text.
extern const juce::String findHelp;   ///< Rendered find verb help text.
extern const juce::String replaceHelp;///< Rendered replace verb help text.
extern const juce::String linesHelp;  ///< Rendered lines verb help text.
extern const juce::String fieldsHelp; ///< Rendered fields verb help text.
extern const juce::String patchHelp;  ///< Rendered patch verb help text.
extern const juce::String undoHelp;   ///< Rendered undo verb help text.

/**______________________________END OF NAMESPACE______________________________*/
}// namespace files
