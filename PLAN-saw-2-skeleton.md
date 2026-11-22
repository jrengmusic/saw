# PLAN: saw — Part 2: Project Skeleton, Vocabulary, Stdio, Arguments, and Help

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-11-22
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.17 + JAM (LANGUAGE.md "C++ / JUCE")

## Overview

This part creates the `saw` project skeleton from `~/Documents/Poems/dev/archetype/cli` (`SPEC.md` §1.2, §2), adds the `cast` generation layer in the shape of `~/Documents/Poems/dev/scout/cast/`, writes `Source/HELP.md` and the six `Source/help/<verb>.md` documents (`SPEC.md` §4.1), and implements `Stdio`, `Arguments`, and `Main.cpp`.

## Language / Framework Constraints

- One object, one translation unit (`CODING.md:899-937`): `Stdio` in `Source/Stdio.h/.cpp`, `Arguments` in `Source/Arguments.h/.cpp`, entry point and help rendering in `Source/Main.cpp`.
- Global namespace for project types (`P17`).
- Scope `juce::ScopedJuceInitialiser_GUI` strictly inside `printBannerAndHelp()` and `printHelp()` in `Source/Main.cpp` (`SPEC.md` §2 Architecture Constraints, `P1`).
- Erase the terminal screen only on the help path (`SPEC.md` §1.2, §4.1, `A22`), and never erase twice inside `printBannerAndHelp()`.
- Every diagnostic and summary string comes from `cast/text.md` (`text::Diagnostics`), and every identifier or option string comes from JAM `Id::` or `cast/identifiers.md` (`Id::`). Zero raw user-visible string literals in `.cpp` (`PLAN-saw-0-tracker.md` Rule 4).
- Do not redeclare in `cast/identifiers.md` or `cast/files.md` any symbol already defined in the `jam_tui` closure (`jam/generated/jam_Core.h`, `jam_Gui.h`, `jam_Terminal.h`, `jam_Mermaid.h`, `jam_Style.h`): `Id::after`, `Id::array`, `Id::command`, `Id::commit`, `Id::config`, `Id::diagnosticSeparator`, `Id::endLine`, `Id::file`, `Id::id`, `Id::insertAfter`, `Id::insertBefore`, `Id::line`, `Id::object`, `Id::path`, `Id::text`, `Id::tokens`, `Id::undo`, `Id::version`, and `files::styleSheet`.
- No comments and no doxygen before the audit (`CAROL.md` Code Hygiene).

## Framework Inventory

Full tables live in `PLAN-saw-0-tracker.md` §Framework Inventory. This part uses:

- **API:** A8 (`juce::ArgumentList`), A10 (`juce::String::formatted`, `juce::Range<int>`), A15 (`jam::Format::getPreColon`, `getPostColon`), A17 (`jam::Function::Map`), A18 (`jam::HashMap`), A19 (`jam::Array`), A20 (`jam::Strings`), A21 (`jam::TerminalPage`, `jam::MarkdownDocument::parse`, `BinaryData::getString`, `BinaryData::fetcher`), A22 (`jam::TerminalEncoder::writeEraseScreen`), A24 (`ProjectInfo`, `CAST_COMMIT`), A25 (`Id::diagnosticSeparator`, `Chars::doubleDash`, `Chars::newline`, `Chars::space`, `Chars::colon`, `Chars::dash`, `Chars::openParen`, `Chars::closeParen`, `Chars::zero`, `Chars::nine`).
- **Pattern:** P1 (CLI entry, UTF-8 console, scoped GUI initialiser for help, version line, `printError`), P2 (`juce::ArgumentList` in `main`), P4 (blocking stdin read via `fread`, binary mode on Windows), P5 (integer flag check by round trip), P6 (Identifier-keyed `jam::Function::Map` dispatch), P8 (generated vocabulary from `cast/*.md`), P9 (`<name>Option`), P14 (`std::pair<juce::Result, T>`), P17 (global namespace).
- **Names:** N2 (`Stdio`), N5 (`Arguments`), N6 (`run`, `print`, `write`, `get`, `set`, `is`, `contains`), N7 (`<name>Option`), N8 (`sawHelp`, `findHelp`, `replaceHelp`, `linesHelp`, `fieldsHelp`, `patchHelp`, `undoHelp`), N9 (`text::Diagnostics::fail*`), N11 (`Stdio::exitSuccess`, `Stdio::exitMismatch`, `Stdio::exitError`), N13 (`verbCommands`, `verbHelpFiles`).
- **Synthesis:** S1, S2, S3, S4, S5, S6, S7, S8, S9, S10, S11, S12, S13, S14.
- **Absence:** X10 (TTY detection via `isatty` / `_isatty`), X11 (blocking stdin read via `fread`), X12 (`juce::ArgumentList`).

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against `MANIFESTO.md` (BLESSED), `NAMES.md`, `CODING.md`, and the locked PLAN decisions. @Auditor runs ONCE, after the final step — never per step.

## Steps

### Step 1: Initialize the skeleton from `archetype/cli` and copy build templates
**Scope:** `~/Documents/Poems/dev/saw/project-info.md`, `~/Documents/Poems/dev/saw/RELEASE.md`, `~/Documents/Poems/dev/saw/LICENSE`, `~/Documents/Poems/dev/saw/cast/spell.md`, `~/Documents/Poems/dev/saw/cast/signing.md`, `~/Documents/Poems/dev/saw/cast/cmake.cast`, `~/Documents/Poems/dev/saw/cast/installer.cast`, `~/Documents/Poems/dev/saw/cast/installer/resources/`
**Action:**
1. Run `cast INIT.md ~/Documents/Poems/dev/saw` from `~/Documents/Poems/dev/archetype/cli` (`SPEC.md` §2).
2. Copy `~/Documents/Poems/dev/archetype/cli/cast/cmake.cast` to `~/Documents/Poems/dev/saw/cast/cmake.cast`.
3. Copy `~/Documents/Poems/dev/archetype/cli/cast/installer.cast` to `~/Documents/Poems/dev/saw/cast/installer.cast`.
4. Copy `~/Documents/Poems/dev/archetype/cli/cast/installer/resources/` (`background.png`, `background-dark.png`) to `~/Documents/Poems/dev/saw/cast/installer/resources/`.
5. Copy `~/Documents/Poems/dev/scout/LICENSE` to `~/Documents/Poems/dev/saw/LICENSE` and `~/Documents/Poems/dev/scout/cast/signing.md` to `~/Documents/Poems/dev/saw/cast/signing.md`.
**Inventory:** P8.
**Validation:** COUNSELOR checks that all listed files exist in `~/Documents/Poems/dev/saw/`.

### Step 2: Configure `project-info.md` and `cast/spell.md`
**Scope:** `~/Documents/Poems/dev/saw/project-info.md`, `~/Documents/Poems/dev/saw/cast/spell.md`
**Action:**
1. Update `~/Documents/Poems/dev/saw/project-info.md` to match `~/Documents/Poems/dev/scout/project-info.md` with these project values (`SPEC.md` §1.2, §2, §4.1):
   - `## project info`: `projectName = saw`, `companyName = JRENG`, `legalCompanyName = Jubilant Research of Eclectic Novelty Generation`, `versionString = 0.0.1`, `versionNumber = 0x1`, `productWebsite = https://jrengmusic.com`, `companyEmail = info@jrengmusic.com`, `presetExtension = saw`, `presetDefault = INIT`.
   - `## cmake`: `projectDescription = Safe Atomic Writer`, `banner = saw`, `bundleIdentifier = com.jrengmusic.saw`, `targetName = saw`, `companyCopyright = © 2026 JRENG`, `productName = saw`, `buildVersion = 0.0.1`, `documentExtensions = saw`, `qaDirectory = $ENV{HOME}/Documents/Poems/dev/___builds___`, `repository = jrengmusic/saw`; all other rows identical to `scout/project-info.md`.
   - `## juce module`: `juce_core`, `juce_cryptography`, `juce_data_structures`, `juce_events`, `juce_graphics`, `juce_gui_basics`, `juce_audio_processors` (`SPEC.md` §2).
   - `## user module`: the 14-module closure of `jam_tui` (`jam_core`, `jam_document`, `jam_markdown`, `jam_freetype`, `jam_graphics`, `jam_data_structures`, `jam_animation`, `jam_web`, `jam_style`, `jam_gui`, `jam_mermaid_diagram`, `jam_markdown_graphics`, `jam_terminal`, `jam_tui`).
   - `## macro`: all rows of `scout/project-info.md:149-167` (including `NOMINMAX=1` on Windows).
   - `## binary`: `Source/HELP.md`, `Source/help/find.md`, `Source/help/replace.md`, `Source/help/lines.md`, `Source/help/fields.md`, `Source/help/patch.md`, `Source/help/undo.md`, `../jam/jam_tui/page/style.css`.
   - `## release notes`: `First release: SAW replaces sed, grep, and file-oriented awk with one deterministic, dry-run-first, atomic CLI tool.`
2. Update `~/Documents/Poems/dev/saw/cast/spell.md` to match `~/Documents/Poems/dev/scout/cast/spell.md` (replacing `SCOUT`/`scout` descriptions with `SAW`/`saw`).
**Inventory:** A24, P8.
**Validation:** COUNSELOR reads `project-info.md` and `cast/spell.md` and verifies table syntax, `+---+` separators, module rows, and binary rows.

### Step 3: Author `cast/identifiers.md`, `cast/text.md`, `cast/banner.md`, `cast/files.md`, and `cast/source.cast`
**Scope:** `~/Documents/Poems/dev/saw/cast/identifiers.md`, `~/Documents/Poems/dev/saw/cast/text.md`, `~/Documents/Poems/dev/saw/cast/banner.md`, `~/Documents/Poems/dev/saw/cast/files.md`, `~/Documents/Poems/dev/saw/cast/source.cast`
**Action:**
1. Write `cast/source.cast` matching `scout/cast/source.cast:1-6`.
2. Write `cast/files.md` defining `sawHelp` (`HELP.md`), `findHelp` (`find.md`), `replaceHelp` (`replace.md`), `linesHelp` (`lines.md`), `fieldsHelp` (`fields.md`), `patchHelp` (`patch.md`), `undoHelp` (`undo.md`). Do not redeclare `files::styleSheet`.
3. Write `cast/banner.md` with the 8-row `S A W` glyph artwork in the `cast/cast/banner.md` colour palette.
4. Write `cast/identifiers.md` with:
   - `@id`: `find`, `replace`, `lines`, `fields`, `patch`, `startCol`, `endCol`, `startLine`, `deletion` (`delete`), `hash`, `timestamp`, `files`, `blob`, `preHash`, `postHash`, `executable`.
   - `@string` options (`N7`): `helpOption` (`--help|-h`), `versionOption` (`--version|-V`), `regexOption` (`--regex|-r`), `ignoreCaseOption` (`--ignore-case|-i`), `exactOption` (`--exact|-x`), `invertOption` (`--invert|-v`), `countOption` (`--count|-c`), `quietOption` (`--quiet|-q`), `lineNumberOption` (`--line-number|-n`), `noLineNumberOption` (`--no-line-number`), `hashOption` (`--hash`), `contextOption` (`--context|-C`), `afterOption` (`--after`), `linesOption` (`--lines`), `firstOption` (`--first`), `includeOption` (`--include`), `excludeOption` (`--exclude`), `hiddenOption` (`--hidden`), `noIgnoreOption` (`--no-ignore`), `withStdinOption` (`--with-stdin`), `withFileOption` (`--with-file`), `applyOption` (`--apply`), `expectOption` (`--expect`), `deleteOption` (`--delete`), `replaceOption` (`--replace`), `insertBeforeOption` (`--insert-before`), `insertAfterOption` (`--insert-after`), `stdinOption` (`--stdin`), `verifyHashOption` (`--verify-hash`), `delimiterOption` (`--delimiter|-d`), `outputDelimiterOption` (`--output-delimiter|-o`), `matchOption` (`--match`), `strictOption` (`--strict`), `fileOption` (`--file`), `listOption` (`--list`), `forceOption` (`--force`).
   - `@string` path/config/undo constants: `stdinPath` (`<stdin>`), `currentDirectory` (`.`), `gitDirectory` (`.git`), `gitignoreFile` (`.gitignore`), `gitExcludePath` (`.git/info/exclude`), `xdgConfigHomeVariable` (`XDG_CONFIG_HOME`), `gitConfigRelativePath` (`git/config`), `defaultGitConfigRelativePath` (`.config/git/config`), `userGitConfigFile` (`.gitconfig`), `gitIgnoreRelativePath` (`git/ignore`), `defaultGitIgnoreRelativePath` (`.config/git/ignore`), `coreSection` (`core`), `excludesFileKey` (`excludesfile`), `homePrefix` (`~/`), `ignoreCaseRegexPrefix` (`(?i:`), `ignoreCaseRegexSuffix` (`)`), `undoFolder` (`undo`), `metaFile` (`meta.json`), `blobSuffix` (`.blob`), `utcCompactFormat` (`%Y%m%dT%H%M%S`), `utcIsoFormat` (`%Y-%m-%dT%H:%M:%SZ`).
   (Zero collisions in `namespace Id` verified across all `jam/generated/jam_*.h`.)
5. Write `cast/text.md` holding every diagnostic, summary, diff, and validation-detail string in `SPEC.md` (`N9`):
   - All `fail*` messages from `SPEC.md` §5.1–§5.6 and §8:
     - `failCommandUnknown` (`unknown command: %s`)
     - `failFlagUnknown` (`unknown flag: %s`)
     - `failFlagValueMissing` (`flag requires a value: %s`)
     - `failUnexpectedArgument` (`unexpected argument: %s`)
     - `failMissingPath` (`missing path argument`)
     - `failNotFound` (`not found: %s`)
     - `failBinaryFile` (`binary file: %s`)
     - `failStandardInput` (`cannot read stdin`)
     - `failIntegerInvalid` (`invalid integer for %s: %s`)
     - `failFindRequiresPattern` (`find requires <pattern>`)
     - `failReplaceRequiresArguments` (`replace requires <find> and <with>`)
     - `failWithSourcesConflict` (`specify at most one of --with-stdin, --with-file`)
     - `failLinesRequiresRange` (`lines requires <range>`)
     - `failFieldsRequiresSpec` (`fields requires <spec>`)
     - `failPatternEmpty` (`pattern cannot be empty`)
     - `failRegexInvalid` (`invalid regex: %s`)
     - `failApplyRequiresExpect` (`--apply requires --expect <N>`)
     - `failApplyStandardInput` (`cannot --apply on stdin; specify file paths`)
     - `failExpectMismatch` (`expected %d edit(s), found %d; nothing written`)
     - `failCaptureInvalid` (`invalid capture group $%d in replacement` — description: `Replacement references $k where k > capture group count.`)
     - `failWrite` (`write failed for %s: %s; rolled back`)
     - `failLineRangeInvalid` (`invalid line range: %s`)
     - `failLineRangeBounds` (`line range %s out of bounds for %s (%d lines)`)
     - `failInsertSingleLine` (`insert requires a single line number, got %s`)
     - `failMutateFlagsConflict` (`specify at most one of --delete, --replace, --insert-before, --insert-after`)
     - `failHashMismatch` (`hash mismatch at %s:%d: expected %s, got %s`)
     - `failFieldSpecInvalid` (`invalid field spec: %s`)
     - `failFieldSpecZero` (`invalid field spec: %s (1-based or negative)`)
     - `failDelimiterEmpty` (`delimiter cannot be empty`)
     - `failFieldBounds` (`field %d out of range at %s:%d`)
     - `failPatchRequiresInput` (`patch requires stdin or --file <path>`)
     - `failPatchJsonInvalid` (`invalid patch JSON: %s`)
     - `failPatchRootNotArray` (`root must be an array`)
     - `failPatchEntryInvalid` (`invalid patch entry %d: %s`)
     - `failPatchEntryNotObject` (`entry must be an object`)
     - `failPatchEntryShape` (`unrecognized or conflicting operation fields`)
     - `failPatchEntryFields` (`missing or invalid required fields`)
     - `failPatchConflict` (`conflicting edits in %s at line %d`)
     - `failPatchBounds` (`out of bounds in %s at entry %d`)
     - `failUndoEmpty` (`no undo transactions available`)
     - `failUndoUnknown` (`unknown transaction: %s`)
     - `failUndoCorrupt` (`corrupt transaction %s: %s`)
     - `failUndoDrift` (`cannot undo %s: %s changed since apply (pass --force to overwrite)`)
     - `failUndoMissing` (`cannot undo %s: %s missing since apply (pass --force to overwrite)`)
     - `failUndoWrite` (`undo failed for %s: %s; rolled back`)
   - Summary and format strings:
     - `summaryDryRun` (`dry-run — %d edit(s) in %d file(s); nothing written. Pass --apply --expect %d to write.`)
     - `summaryDryRunZero` (`dry-run — 0 edits; nothing written.`)
     - `summaryApplied` (`applied %d edit(s) in %d file(s) (tx: %s)`)
     - `summaryAppliedZero` (`applied 0 edit(s) in 0 file(s)`)
     - `summaryNoContentChanged` (`no content changed; nothing written`)
     - `summaryDryRunUndo` (`dry-run undo (%s) — %d file(s); nothing written. Pass --apply to restore.`)
     - `summaryUndone` (`undone %s — restored %d file(s)`)
     - `undoListFormat` (`%s\t%s\t%d file(s)\t%s`)
     - `diffOldHeaderFormat` (`--- %s\n`)
     - `diffNewHeaderFormat` (`+++ %s (planned)\n`)
     - `diffNoNewline` (`\\ No newline at end of file\n`)
**Inventory:** A10, A25, P8, P9, N7, N8, N9, S12.
**Validation:** COUNSELOR checks all five files in `cast/`: `+---+` separators between rows, zero `Id::` collisions, zero Windows SDK macro names (`S12`), and every `SPEC.md` diagnostic present verbatim.

### Step 4: Author `Source/HELP.md` and `Source/help/<verb>.md`
**Scope:** `~/Documents/Poems/dev/saw/Source/HELP.md`, `~/Documents/Poems/dev/saw/Source/help/{find,replace,lines,fields,patch,undo}.md`
**Action:**
1. Write `Source/HELP.md` in the structure of `scout/Source/HELP.md:1-48`, derived strictly from `SPEC.md` §1, §3, §4, §6, and §8.
2. Write the six verb help files under `Source/help/`, each derived strictly from its section of `SPEC.md` (§5.1–§5.6).
**Inventory:** A21, N8.
**Validation:** COUNSELOR reads all seven markdown files and checks zero divergence from `SPEC.md`.

### Step 5: Implement `Source/Stdio.h` and `Source/Stdio.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/Stdio.h`, `~/Documents/Poems/dev/saw/Source/Stdio.cpp`
**Action:**
1. Create `Source/Stdio.h` (`N2`, `N6`, `N11`):
   ```cpp
   #pragma once
   #include <JuceHeader.h>

   struct Stdio
   {
       static constexpr int exitSuccess { 0 };
       static constexpr int exitMismatch { 1 };
       static constexpr int exitError { 2 };

       static void setBinaryOutput() noexcept;
       static bool isTerminalInput() noexcept;
       static bool isTerminalOutput() noexcept;
       static void print (const juce::String& message);
       static void printSummary (const juce::String& message);
       static void printError (const juce::String& message);
       static void write (std::string_view text);
       static std::pair<juce::Result, std::string> getInput();
   };
   ```
2. Create `Source/Stdio.cpp`:
   - Include `"Stdio.h"`, `"generated/Generated.h"`, and platform headers (`<io.h>`, `<fcntl.h>` under `#ifdef _WIN32`, else `<unistd.h>`).
   - `setBinaryOutput()`: under `#ifdef _WIN32`, call `_setmode (_fileno (stdout), _O_BINARY);` (`P4`).
   - `isTerminalInput()` / `isTerminalOutput()`: `_isatty (_fileno (...)) != 0` on Windows, `isatty (fileno (...)) != 0` on POSIX (`X10`).
   - `print (message)`: `setBinaryOutput(); printf ("%s\n", message.toRawUTF8());`.
   - `printSummary (message)`: `print (ProjectInfo::projectName + Id::diagnosticSeparator + message);` (`SPEC.md` §4.3).
   - `printError (message)`: `fprintf (stderr, "%s\n", (ProjectInfo::projectName + Id::diagnosticSeparator + message).toRawUTF8());` (`SPEC.md` §4.3).
   - `write (text)`: `setBinaryOutput(); fwrite (text.data(), sizeof (char), text.size(), stdout);` (`P4`).
   - `getInput()`: under `#ifdef _WIN32`, call `_setmode (_fileno (stdin), _O_BINARY);`. Read `BUFSIZ` chunks into `std::array<char, BUFSIZ> buffer {}` via `fread` into `std::string content` until `bytesRead == 0`. If `ferror (stdin) != 0`, return `{ juce::Result::fail (text::Diagnostics::failStandardInput), {} }`. Return `{ juce::Result::ok(), std::move (content) }` (`P4`, `X11`).
**Inventory:** A24, A25, P1, P4, P14, P17, N2, N6, N11, S3, S5, S6, S7, S10, X10, X11.
**Validation:** COUNSELOR reads `Source/Stdio.h/.cpp` and checks: zero GUI initialization, zero screen erase, zero comments, zero bail-out guards, zero `else` after `return`, brace initialization, alternative tokens.

### Step 6: Implement `Source/Arguments.h` and `Source/Arguments.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/Arguments.h`, `~/Documents/Poems/dev/saw/Source/Arguments.cpp`
**Action:**
1. Create `Source/Arguments.h` (`N5`, `N6`, complete at creation; owns CLI line-range parsing so `TextDocument` stays single-responsibility):
   ```cpp
   #pragma once
   #include <JuceHeader.h>

   class Arguments
   {
   public:
       static constexpr int openRangeEnd { std::numeric_limits<int>::max() };

       Arguments (const juce::ArgumentList& arguments,
                  int startIndex,
                  const jam::Strings& flagOptions,
                  const jam::Strings& valueOptions);

       static bool containsOptionBeforeTerminator (const juce::ArgumentList& arguments,
                                                   int startIndex,
                                                   const juce::String& option);
       static std::pair<juce::Result, juce::Range<int>> getLineRange (const juce::String& rangeText,
                                                                      bool allowZeroStart);

       const juce::Result& getStatus() const noexcept;
       bool contains (const juce::String& option) const;
       juce::String getValue (const juce::String& option) const;
       jam::Strings getValues (const juce::String& option) const;
       const jam::Strings& getPositionals() const noexcept;
       std::pair<juce::Result, int> getInteger (const juce::String& option, int minimumValue) const;

   private:
       static int getMatchingOptionIndex (const juce::ArgumentList::Argument& argument,
                                          const jam::Strings& optionList);
       static int getTerminatorIndex (const juce::ArgumentList& arguments, int startIndex);
       static bool isOptionArgument (const juce::ArgumentList::Argument& argument) noexcept;
       static std::pair<bool, int> getParsedLineNumber (const juce::String& token, int minimumValue);

       juce::Result status { juce::Result::ok() };
       jam::Strings matchedFlags;
       jam::Array<std::pair<juce::String, juce::String>> matchedValues;
       jam::Strings positionals;
   };
   ```
2. Create `Source/Arguments.cpp`:
   - `getMatchingOptionIndex`: return first `index` where `argument == optionList.at (index)` (`A8`), else `-1`.
   - `getTerminatorIndex`: return first `index >= startIndex` where `arguments[index].text.compare (Chars::doubleDash) == 0`, else `arguments.size()`.
   - `containsOptionBeforeTerminator`: return `true` if any `index` in `[startIndex, getTerminatorIndex (arguments, startIndex))` matches `arguments[index] == option`.
   - `isOptionArgument`: return `argument.text.startsWithChar (Chars::dash) and argument.text.length() > 1 and not (argument.text[1] >= Chars::zero and argument.text[1] <= Chars::nine)`.
   - Constructor: walk `[startIndex, terminatorIndex)` while `status.wasOk()` with 3 branches (`flagIndex >= 0`, `valueIndex >= 0`, `else` → unknown flag or positional); append `[terminatorIndex + 1, arguments.size())` to `positionals` (`SPEC.md` §4.1).
   - `contains`, `getValue`, `getValues`: use structured bindings `const auto& [matchedOption, matchedValue]` (`S11`) and `.at()` (`S4`), comparing via `matchedOption.compare (option) == 0` (`S1`).
   - `getInteger (option, minimumValue)`: round-trip check (`P5`) returning `{ juce::Result::ok(), value }` or `{ juce::Result::fail (failIntegerInvalid), 0 }`.
   - `getLineRange (rangeText, allowZeroStart)` (`SPEC.md` §5.3 Range Grammar, `A10`, `A15`):
     - Single line (no `:`): validate `A >= (allowZeroStart ? 0 : 1)` → `{ ok, juce::Range<int> (A, A + 1) }`.
     - Colon form (`A:B`, `A:`, `:B`): split with `jam::Format::getPreColon` / `getPostColon` (`A15`); reject if `postColon.containsChar (Chars::colon)` or both halves empty; `startLine = pre.isEmpty() ? 1 : A` (`A >= 1`), `endExclusive = post.isEmpty() ? openRangeEnd : B + 1` (`B >= startLine`); return `{ ok, juce::Range<int> (startLine, endExclusive) }` or `{ fail (failLineRangeInvalid), {} }`.
**Inventory:** A8, A10, A15, A19, A20, A25, P2, P5, P14, P17, N5, N6, N7, S1, S3, S4, S6, S7, S9, S10, S11, S14, X12.
**Validation:** COUNSELOR reads `Source/Arguments.h/.cpp` and checks: `Argument::operator==` (`S1`), structured bindings (`S11`), `jam::Format::getPreColon`/`getPostColon` (`S9`), ≤3 branches per function (`S14`), zero `else` after `return` (`S3`).

### Step 7: Implement `Source/Main.cpp` and verify the build
**Scope:** `~/Documents/Poems/dev/saw/Source/Main.cpp`
**Action:**
1. Create `Source/Main.cpp`:
   - Include `<JuceHeader.h>`, `"generated/Generated.h"`, `"Stdio.h"`, `"Arguments.h"` (plus `<windows.h>` under `#ifdef _WIN32`).
   - Constants: `static constexpr int verbArgumentIndex { 1 };` and `static constexpr int firstVerbOptionIndex { 2 };`.
   - `static void writeScreenErase()` (`A22`, `S3`, `S5`): `if (Stdio::isTerminalOutput())` write `jam::TerminalEncoder::writeEraseScreen` to `stdout` and `fflush (stdout)`.
   - `static void printBanner()`: iterate `for (const auto& [name, text] : map::banner)` (`S11`) and call `Stdio::print (text)`.
   - `static void writeHelpPage (const juce::String& resourceFileName)`: construct `jam::TerminalPage` with `{ BinaryData::fetcher }` and `jam::Strings { files::styleSheet }`, parse `jam::MarkdownDocument::parse (BinaryData::getString (resourceFileName), resourceFileName)`, call `setDocument` and `writeToStream` (`A21`). (Does NOT erase screen or construct `ScopedJuceInitialiser_GUI`.)
   - `static void printBannerAndHelp()`: `writeScreenErase(); const juce::ScopedJuceInitialiser_GUI juceInitialiser; printBanner(); Stdio::print ({}); writeHelpPage (files::sawHelp);` (single erase, single GUI initialiser, banner preserved).
   - `static void printHelp (const juce::String& resourceFileName)`: `writeScreenErase(); const juce::ScopedJuceInitialiser_GUI juceInitialiser; writeHelpPage (resourceFileName);`.
   - `static void writeVersion()`: print `ProjectInfo::projectName + " " + ProjectInfo::versionString + " (" + CAST_COMMIT + ")"` (`A24`, `A25`, `P1`).
   - `getVerbHelpFiles()` (`N13`): static `jam::HashMap<juce::Identifier, juce::String>` mapping `Id::find`..`Id::undo` to `files::findHelp`..`files::undoHelp`.
   - `getVerbCommands()` (`N13`): static `jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, int (const juce::ArgumentList&)>` (`A17`, `P6`, `S2`) with stub lambdas returning `Stdio::exitError` (replaced in Parts 4–8).
   - `runGlobalOption`, `runVerb`, `runCommandLine`, `main`:
     - `runGlobalOption`: reject extra arguments (`failFlagUnknown`, exit `2`); handle `Id::versionOption` → `writeVersion()`; otherwise `printBannerAndHelp()`.
     - `runVerb`: unknown verb → `failFlagUnknown` (if `isOption()`) or `failCommandUnknown` (exit `2`); if `Arguments::containsOptionBeforeTerminator (arguments, firstVerbOptionIndex, Id::helpOption)` → `printHelp (verbHelpFiles.at (verb))` (exit `0`); otherwise `getVerbCommands().get (verb, arguments)`.
     - `main`: under `#ifdef _WIN32`, `SetConsoleOutputCP (CP_UTF8);`; return `runCommandLine (argc, argv)`.
2. ARCHITECT builds with `cast cast/spell.md --no-sign` in `~/Documents/Poems/dev/saw`.
**Inventory:** A8, A10, A17, A18, A21, A22, A24, A25, P1, P6, P17, N6, N8, N11, N13, S1, S2, S3, S4, S5, S10, S11, S14.
**Validation:** COUNSELOR reads `Source/Main.cpp` and checks: single screen erase in `printBannerAndHelp`, `ScopedJuceInitialiser_GUI` scoped strictly inside `printBannerAndHelp` and `printHelp`, ≤3 branches per function (`S14`), zero `else` after `return` (`S3`).

## BLESSED Alignment

- **B — Bound:** `ScopedJuceInitialiser_GUI` lives on the stack of `printBannerAndHelp` and `printHelp` only. `Arguments` owns its parsed members by value.
- **L — Lean:** `Stdio` owns stdio/exit codes; `Arguments` owns CLI parsing; `Main.cpp` owns routing and help rendering. Every function has ≤3 branches.
- **E — Explicit:** Every exit code, option string, and diagnostic template is named in one place.
- **S — SSOT:** `cast/*.md` is the single source of vocabulary and messages.
- **S — Stateless:** `Stdio` is stateless; `Arguments` is complete at creation.
- **E — Encapsulation:** Commands read parsed options through `Arguments` without touching raw `argv`.
- **D — Deterministic:** Single-pass left-to-right parsing with explicit `--` termination.

## Risks / Open Questions

None.
