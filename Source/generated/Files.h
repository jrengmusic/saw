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

static constexpr std::string_view sawHelp              { "HELP.frame"      };///< Rendered top-level help text.
static constexpr std::string_view findHelp             { "find.frame"      };///< Rendered find verb help text.
static constexpr std::string_view replaceHelp          { "replace.frame"   };///< Rendered replace verb help text.
static constexpr std::string_view linesHelp            { "lines.frame"     };///< Rendered lines verb help text.
static constexpr std::string_view fieldsHelp           { "fields.frame"    };///< Rendered fields verb help text.
static constexpr std::string_view patchHelp            { "patch.frame"     };///< Rendered patch verb help text.
static constexpr std::string_view writeHelp            { "write.frame"     };///< Rendered write verb help text.
static constexpr std::string_view pageHelp             { "page.frame"      };///< Rendered page verb help text.
static constexpr std::string_view undoHelp             { "undo.frame"      };///< Rendered undo verb help text.
static constexpr std::string_view excerptHelp          { "excerpt.frame"   };///< Rendered excerpt verb help text.
static constexpr std::string_view sawDocument          { "HELP.md"         };///< Top-level help document.
static constexpr std::string_view findDocument         { "find.md"         };///< Find verb help document.
static constexpr std::string_view replaceDocument      { "replace.md"      };///< Replace verb help document.
static constexpr std::string_view linesDocument        { "lines.md"        };///< Lines verb help document.
static constexpr std::string_view fieldsDocument       { "fields.md"       };///< Fields verb help document.
static constexpr std::string_view patchDocument        { "patch.md"        };///< Patch verb help document.
static constexpr std::string_view writeDocument        { "write.md"        };///< Write verb help document.
static constexpr std::string_view pageDocument         { "page.md"         };///< Page verb help document.
static constexpr std::string_view undoDocument         { "undo.md"         };///< Undo verb help document.
static constexpr std::string_view excerptDocument      { "excerpt.md"      };///< Excerpt verb help document.
static constexpr std::string_view jsonHelp             { "json.frame"      };///< Rendered json verb help text.
static constexpr std::string_view jsonDocument         { "json.md"         };///< Json verb help document.
static constexpr std::string_view screenErase          { "erase.frame"     };///< Screen erase sequence.
static constexpr std::string_view serverTools          { "tools.json"      };///< MCP tool definitions.
static constexpr std::string_view lingConfig           { ".config/ling"    };///< Ling configuration folder, under home.
static constexpr std::string_view lingCache            { ".cache/ling"     };///< Ling cache folder, under home (Unix).
static constexpr std::string_view lingLocalCache       { "ling"            };///< Ling folder under LOCALAPPDATA.
static constexpr std::string_view language             { "language"        };///< Folder of the language libraries.
static constexpr std::string_view libraryBuild         { "build"           };///< Build folder of a language library.
static constexpr std::string_view pageLibraryName      { "page"            };///< Name of the page library.
static constexpr std::string_view tokensLibraryName    { "tokens"          };///< Name of the tokens library.
static constexpr std::string_view pageLibrary          { "libpage.dylib"   };///< Page library file on macOS.
static constexpr std::string_view pageLibraryWindows   { "page.dll"        };///< Page library file on Windows.
static constexpr std::string_view tokensLibrary        { "libtokens.dylib" };///< Tokens library file on macOS.
static constexpr std::string_view tokensLibraryWindows { "tokens.dll"      };///< Tokens library file on Windows.
static constexpr std::string_view styleSheetExtension  { ".css"            };///< Extension of a page style sheet.

/**______________________________END OF NAMESPACE______________________________*/
}// namespace files
