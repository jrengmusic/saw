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

/** @brief File names saw reads: rendered help frames. */

static constexpr std::string_view sawHelp     { "HELP.frame"    };///< Rendered top-level help text.
static constexpr std::string_view findHelp    { "find.frame"    };///< Rendered find verb help text.
static constexpr std::string_view replaceHelp { "replace.frame" };///< Rendered replace verb help text.
static constexpr std::string_view linesHelp   { "lines.frame"   };///< Rendered lines verb help text.
static constexpr std::string_view fieldsHelp  { "fields.frame"  };///< Rendered fields verb help text.
static constexpr std::string_view patchHelp   { "patch.frame"   };///< Rendered patch verb help text.
static constexpr std::string_view undoHelp    { "undo.frame"    };///< Rendered undo verb help text.
static constexpr std::string_view excerptHelp { "excerpt.frame" };///< Rendered excerpt verb help text.
static constexpr std::string_view screenErase { "erase.frame"   };///< Screen erase sequence.
static constexpr std::string_view serverTools { "tools.json"    };///< MCP tool definitions.

/**______________________________END OF NAMESPACE______________________________*/
}// namespace files
