# PLAN: saw — Part 4: TextDocument, Pattern, IgnoreRules, Paths, Jobs, and find

**RFC:** none — objective from ARCHITECT prompt
**Date:** 2026-11-22
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.17 + JAM (LANGUAGE.md "C++ / JUCE")

## Overview

This part implements the read and traversal layer of `saw` (`TextDocument`, `Pattern`, `IgnoreRules`, `Paths`, `Jobs`) and the `find` verb (`SPEC.md` §4.4, §4.4.1, §4.5, §4.6, §4.8, §5.1), then validates `find` and `jam::RegExp::findNext` through `test/run.lua`.

## Language / Framework Constraints

- One object, one translation unit (`CODING.md:899-937`): `TextDocument.h/.cpp`, `Pattern.h/.cpp`, `IgnoreRules.h/.cpp`, `Paths.h/.cpp`, `Find.h/.cpp`, plus header-only template `Jobs.h`.
- Zero raw `new` (`CODING.md:357`): `TextDocument` and `Pattern` factories use a private `struct Access {};` passkey with `std::make_unique<T> (Access {}, ...)` (`P15`, `ling/Source/SymbolDocument.h:511,520`).
- Zero fake-carrier structs (`S8`): line ranges use `juce::Range<int>` (`A10`), file lists use `jam::Strings` (`A20`), and per-file `Jobs::run` outputs use parallel `jam::Array` slots.
- Complete at creation (`MANIFESTO.md:248-257`): `TextDocument`, `Pattern`, and `IgnoreRules` are complete when constructed; descending into a child directory returns a new `IgnoreRules` via `getChildRules`.
- At most 3 branches per function (`S14`); positive nesting in `void` functions; zero bail-out guards and zero `else` after `return` (`S3`); `.at()` on containers (`S4`); zero out-parameters (`S7`); zero forbidden terms (`S12`).
- No comments and no doxygen before the audit (`CAROL.md` Code Hygiene).

## Framework Inventory

Full tables live in `PLAN-saw-0-tracker.md` §Framework Inventory. This part uses:

- **API:** A1 (`juce::MemoryMappedFile`), A4 (`juce::File`), A5 (`juce::RangedDirectoryIterator`), A6 (`juce::ThreadPool`), A7 (`juce::SHA256`), A8 (`juce::ArgumentList`), A9 (`juce::String::matchesWildcard`), A10 (`juce::String::formatted`, `juce::Range<int>`), A12 (`jam::simd::findByte`), A13 (`jam::RegExp`, `findNext`, `hasMatch`), A18 (`jam::HashMap`), A19 (`jam::Array`), A20 (`jam::Strings`, `sort (false)`), A23 (`jam::File::getUserDirectory`), A25 (`Id::`, `Chars::`).
- **Pattern:** P3 (`Find::run`), P5 (integer validation), P7 (`Jobs::run`), P14 (`std::pair<juce::Result, ValueType>`), P15 (`Access` passkey + `std::make_unique`), P17 (global namespace).
- **Names:** N1 (`Find`), N2 (`Paths`, `Jobs`), N3 (`TextDocument`), N6 (`get`, `add`, `is`, `has`, `run`), N10 (`Pattern`, `IgnoreRules`), N12 (`Pattern::findNext`), N13 (`globTokens`).
- **Synthesis:** S1, S3, S4, S5, S6, S7, S8, S9, S10, S11, S12, S13, S14, S15.
- **Absence:** X2, X3, X4, X5, X6, X7, X13.

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against `MANIFESTO.md` (BLESSED), `NAMES.md`, `CODING.md`, and the locked PLAN decisions. @Auditor runs ONCE, after the final step — never per step.

## Steps

### Step 1: Implement `Source/TextDocument.h` and `Source/TextDocument.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/TextDocument.h`, `~/Documents/Poems/dev/saw/Source/TextDocument.cpp`
**Action:**
1. Create `Source/TextDocument.h` (zero raw `new` via private `Access` passkey `P15`; single responsibility — no CLI line-range parsing):
   ```cpp
   #pragma once
   #include <JuceHeader.h>

   class TextDocument
   {
       struct Access {};

   public:
       static constexpr int lineHashLength { 4 };

       TextDocument (Access,
                     std::unique_ptr<juce::MemoryMappedFile> newMappedFile,
                     std::string newOwnedBytes,
                     const juce::String& newDisplayPath);

       static std::pair<juce::Result, std::unique_ptr<TextDocument>> fromFile (const juce::File& file,
                                                                               const juce::String& displayPath);
       static std::unique_ptr<TextDocument> fromString (std::string text,
                                                        const juce::String& displayPath);
       static bool isBinaryFile (const juce::File& file) noexcept;

       const juce::String& getDisplayPath() const noexcept;
       std::string_view getBytes() const noexcept;
       bool isBinary() const noexcept;
       int getNumLines() const noexcept;
       std::string_view getLine (int lineNumber) const;
       std::string_view getLineEnding() const noexcept;
       bool hasTrailingNewline() const noexcept;
       juce::String getLineHash (int lineNumber) const;
       juce::String getContentHash() const;

   private:
       std::unique_ptr<juce::MemoryMappedFile> mappedFile;
       std::string ownedBytes;
       juce::String displayPath;
       const char* data { nullptr };
       size_t size { 0 };
       std::string_view dominantLineEnding { "\n" };
       jam::Array<size_t> lineStarts;
       jam::Array<size_t> lineLengths;
   };
   ```
2. Create `Source/TextDocument.cpp`:
   - `fromFile`: check `file.existsAsFile()` (else `failNotFound`); when `file.getSize() == 0`, return `{ juce::Result::ok(), std::make_unique<TextDocument> (Access {}, nullptr, std::string {}, displayPath) }` (`A1`, `SPEC.md` §4.5); otherwise map with `std::make_unique<juce::MemoryMappedFile> (file, juce::MemoryMappedFile::readOnly)`, check `mapped->getData() != nullptr`, and return `{ juce::Result::ok(), std::make_unique<TextDocument> (Access {}, std::move (mapped), std::string {}, displayPath) }`.
   - `fromString`: return `std::make_unique<TextDocument> (Access {}, nullptr, std::move (text), displayPath)`.
   - `isBinaryFile`: return `false` for missing or 0-byte files; map read-only and check `jam::simd::findByte (..., 0, '\0') < size` (`A12`, `SPEC.md` §4.5).
   - `isBinary()`: return `size > 0 and jam::simd::findByte (data, size, 0, '\0') < size`.
   - `hasTrailingNewline()`: return `size > 0 and getBytes().at (size - 1) == static_cast<char> (Chars::newline)` (`S4`).
   - Constructor: set `data`/`size`; `if (size > 0 and not isBinary())` build `lineStarts`, `lineLengths`, and `dominantLineEnding` (`"\r\n"` vs `"\n"` from first newline) via `jam::simd::findByte` (`A12`, `X4`, `S3`).
   - `getLine (int lineNumber)`: `assert (lineNumber >= 1 and lineNumber <= getNumLines());` return `std::string_view { data + lineStarts.at (lineNumber - 1), lineLengths.at (lineNumber - 1) }`.
   - `getLineHash (int lineNumber)`: `juce::SHA256 (view.data(), view.size()).toHexString().substring (0, lineHashLength)` (`A7`, `SPEC.md` §4.6).
   - `getContentHash()`: `juce::SHA256 (data, size).toHexString()` (`A7`).
**Inventory:** A1, A7, A10, A12, A19, A25, P14, P15, P17, N3, N6, S3, S4, S6, S7, S8, S10, S14, X4, X7.
**Validation:** COUNSELOR reads `Source/TextDocument.h/.cpp` and checks: zero raw `new`, zero manual boolean flags, positive nesting in constructor, `.at()` everywhere, ≤3 branches per function.

### Step 2: Implement `Source/Pattern.h` and `Source/Pattern.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/Pattern.h`, `~/Documents/Poems/dev/saw/Source/Pattern.cpp`
**Action:**
1. Create `Source/Pattern.h` (zero raw `new` via `struct Access {};` passkey `P15`):
   ```cpp
   #pragma once
   #include <JuceHeader.h>

   class Pattern
   {
       struct Access {};

   public:
       Pattern (Access,
                std::unique_ptr<jam::RegExp> newRegExp,
                std::string newLiteral,
                int newNumCaptureGroups,
                bool newIsIgnoreCase,
                bool newIsExactLine);

       static std::pair<juce::Result, std::unique_ptr<Pattern>> create (const juce::String& patternText,
                                                                        bool isRegex,
                                                                        bool isIgnoreCase,
                                                                        bool isExactLine = false);

       jam::Array<std::optional<std::string_view>> findNext (std::string_view line, size_t offset) const;
       bool hasMatch (std::string_view line) const;
       int getNumCaptureGroups() const noexcept;

   private:
       static char getLowerCaseAscii (char byte) noexcept;
       static int getCaptureGroupCount (std::string_view patternText) noexcept;
       jam::Array<std::optional<std::string_view>> findNextLiteral (std::string_view line, size_t offset) const;
       jam::Array<std::optional<std::string_view>> findNextIgnoreCaseLiteral (std::string_view line, size_t offset) const;

       std::unique_ptr<jam::RegExp> regExp;
       std::string literal;
       int numCaptureGroups { 0 };
       bool isIgnoreCase { false };
       bool isExactLine { false };
   };
   ```
2. Create `Source/Pattern.cpp`:
   - `getLowerCaseAscii`: fold `Chars::upperA`..`Chars::upperZ` only (`X6`, `SPEC.md` §4.8).
   - `getCaptureGroupCount (std::string_view patternText)`: walk with `.at (index)` skipping `\` escapes and `[...]` character classes; count unescaped `(` when:
     - `index + 1 >= size` or `patternText.at (index + 1) != '?'` (numbered group `(...)`), OR
     - `index + 3 < size` and `patternText.at (index + 1) == '?'` and `patternText.at (index + 2) == '<'` and `patternText.at (index + 3) != '='` and `patternText.at (index + 3) != '!'` (named group `(?<name>...)`, explicitly excluding lookbehinds `(?<=...)` and `(?<!...)`).
   - `create`: reject empty pattern (`failPatternEmpty`); in regex mode validate raw pattern in `try`/`catch (const std::invalid_argument&)` (`failRegexInvalid`), wrap with `(?i:` + pattern + `)` when `isIgnoreCase`, and construct via `std::make_unique<Pattern> (Access {}, ...)`; in literal mode fold ASCII when `isIgnoreCase` and construct via `std::make_unique<Pattern> (Access {}, ...)`.
   - `findNext`, `findNextLiteral` (`A12`, `X5`), `findNextIgnoreCaseLiteral` (`X6`), `hasMatch`: exact line check compares `captures.at (0)->data() == line.data() and captures.at (0)->size() == line.size()`.
**Inventory:** A10, A12, A13, A19, A25, P14, P15, P17, N6, N10, N12, S3, S4, S6, S7, S10, S14, X5, X6.
**Validation:** COUNSELOR reads `Source/Pattern.h/.cpp` and checks: zero raw `new`, lookbehinds excluded from `getCaptureGroupCount`, `.at()` everywhere, ≤3 branches per function.

### Step 3: Implement `Source/IgnoreRules.h` and `Source/IgnoreRules.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/IgnoreRules.h`, `~/Documents/Poems/dev/saw/Source/IgnoreRules.cpp`
**Action:**
1. Create `Source/IgnoreRules.h`:
   ```cpp
   #pragma once
   #include <JuceHeader.h>

   class IgnoreRules
   {
   public:
       explicit IgnoreRules (const juce::File& walkRoot);

       IgnoreRules getChildRules (const juce::File& directory) const;
       bool isIgnored (const juce::File& entry, bool isDirectory) const;

   private:
       struct Rule
       {
           std::shared_ptr<jam::RegExp> regExp;
           juce::String basePrefix;
           bool isNegated { false };
           bool isDirectoryOnly { false };
       };

       IgnoreRules (const juce::File& newTopLevel, jam::Array<Rule> newRules);

       static juce::File getTopLevelDirectory (const juce::File& walkRoot);
       static juce::File getGlobalExcludesFile();
       static juce::String getExcludesFileFromConfig (const juce::File& configFile);
       static std::optional<Rule> getParsedRule (std::string_view rawLine,
                                                 const juce::String& baseRelativePrefix);
       static std::string getTranslatedGlobRegex (std::string_view pattern, bool isAnchored);
       void addRulesFromFile (const juce::File& file, const juce::String& baseRelativePrefix);
       juce::String getRelativePath (const juce::File& entry) const;

       juce::File topLevel;
       jam::Array<Rule> rules;
   };
   ```
2. Create `Source/IgnoreRules.cpp` implementing `SPEC.md` §4.4.1 (`X2`, `X3`):
   - **Constructor `IgnoreRules::IgnoreRules (const juce::File& walkRoot)`** (loads sources from lowest to highest precedence so reverse scan in `isIgnored` gives last-match-wins):
     1. Set `topLevel = getTopLevelDirectory (walkRoot)`.
     2. Load lowest-precedence global excludes: `addRulesFromFile (getGlobalExcludesFile(), {})`.
     3. Load repository excludes: `addRulesFromFile (topLevel.getChildFile (Id::gitExcludePath), {})`.
     4. Collect the directory chain from `walkRoot` up to `topLevel` (inclusive) into `jam::Array<juce::File> ancestors`; iterate `ancestors` in reverse (`topLevel` down to `walkRoot`) and call `addRulesFromFile (dir.getChildFile (Id::gitignoreFile), getRelativePath (dir))`.
   - `getGlobalExcludesFile()`: obtain `const juce::File homeDir { jam::File::getUserDirectory() };` (`A23` — cross-platform macOS and Windows) and `XDG_CONFIG_HOME` via `juce::SystemStats::getEnvironmentVariable (Id::xdgConfigHomeVariable, {})`; read XDG config then `homeDir.getChildFile (Id::userGitConfigFile)` (last `[core]` `excludesFile` wins); expand leading `~/` to `homeDir`; default to `$XDG_CONFIG_HOME/git/ignore` or `homeDir.getChildFile (Id::defaultGitIgnoreRelativePath)`.
   - `getTranslatedGlobRegex`: use `static const jam::HashMap<char, std::string_view> globTokens` (`N13`, `S14`); translate `/**/`, leading `**/`, trailing `/**`, `*`, `\c`, and `[...]` character classes (translating a leading `[!` to `[^` per gitignore semantics).
   - `getParsedRule`, `addRulesFromFile`, `getChildRules`, `isIgnored`: implement `SPEC.md` §4.4.1 with positive nesting (`S3`), `.at()` (`S4`), zero forbidden terms (`S12`), and ≤3 branches per function (`S14`).
**Inventory:** A1, A4, A13, A18, A19, A23, A25, P17, N6, N10, N13, S1, S3, S4, S6, S7, S10, S11, S12, S13, S14, X2, X3.
**Validation:** COUNSELOR reads `Source/IgnoreRules.h/.cpp` and checks: complete constructor load order (`global` → `.git/info/exclude` → `topLevel..walkRoot`), `jam::File::getUserDirectory()` used, `[!...]` translated to `[^...]`, ≤3 branches per function.

### Step 4: Implement `Source/Jobs.h`, `Source/Paths.h`, and `Source/Paths.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/Jobs.h`, `~/Documents/Poems/dev/saw/Source/Paths.h`, `~/Documents/Poems/dev/saw/Source/Paths.cpp`
**Action:**
1. Create `Source/Jobs.h` matching `cast/Source/Jobs.h:1-39` (`A6`, `P7`).
2. Create `Source/Paths.h` and `Source/Paths.cpp`:
   - `hasMultipleOrDirectoryInputs (const jam::Strings& rawPaths)`.
   - `getFiles (rawPaths, includeGlobs, excludeGlobs, isIncludingHidden, isIgnoringRules)`:
     - Resolve each `rawPath` via `getCurrentWorkingDirectory().getChildFile (rawPath)` and `getLinkedTarget()` (`A4`).
     - Missing path → `failNotFound`.
     - Explicit file → normalize `\` to `/`, add via `addIfNotAlreadyThere` (never filtered by ignore rules or binary skip).
     - Explicit directory → compute normalized `displayPrefix`: replace `\` with `/`, strip trailing `/` (unless the path is `"/"`), then append `"/"` (so `"."` → `"./"`, `"dir"` and `"dir/"` → `"dir/"`, `"/abs/dir/"` → `"/abs/dir/"`). Walk non-recursively (`A5`, `X13`), skip `.git`, hidden entries (unless `isIncludingHidden`), ignored entries (unless `isIgnoringRules`), non-matching include/exclude globs (`A9`), and binary files (`TextDocument::isBinaryFile`).
     - Sort ascending via `collectedPaths.sort (false)` (`A20`, `SPEC.md` §4.4).
**Inventory:** A4, A5, A6, A9, A10, A20, A25, P7, P14, P17, N2, N6, S1, S3, S4, S6, S7, S8, S10, S14, X13.
**Validation:** COUNSELOR reads `Source/Jobs.h` and `Source/Paths.h/.cpp` and verifies prefix normalization, symlink resolution, and binary-file walk skip vs direct-file preservation.

### Step 5: Implement `Source/Find.h`, `Source/Find.cpp`, and wire `Id::find` in `Source/Main.cpp`
**Scope:** `~/Documents/Poems/dev/saw/Source/Find.h`, `~/Documents/Poems/dev/saw/Source/Find.cpp`, `~/Documents/Poems/dev/saw/Source/Main.cpp`
**Action:**
1. Create `Source/Find.h` (`struct Find { static int run (const juce::ArgumentList& arguments); };`).
2. Create `Source/Find.cpp` implementing `SPEC.md` §5.1:
   - Parse `Arguments arguments (argumentList, 2, flagOptions, valueOptions)` with all 17 options of `SPEC.md` §5.1.
   - Validate `--context` (`>= 0`), `--after` (`>= 0`), `--lines` (`Arguments::getLineRange (..., false)`), and compile `Pattern::create`.
   - Formatting rules (unambiguous, matching `grep` and `SPEC.md` §5.1):
     - `-c` (`--count`): emit `<path>:<count>\n` (when `isShowingPathPrefix`) or `<count>\n` (single input) for every file, including `0` counts.
     - Matched line: optional `<path>:` + (`<line>:<hhhh>|` when `--hash`, `<line>:` when line numbers enabled, empty when `--no-line-number`) + `<lineText>\n`.
     - Context line (`-C <N>`, `N > 0`): optional `<path>-` + (`<line>-<hhhh>|` when `--hash`, `<line>-` when line numbers enabled, empty when `--no-line-number`) + `<lineText>\n`. Emit `"--\n"` between non-contiguous context groups (matching `grep -n -C N`).
   - Run over stdin (`Stdio::getInput()`) or `Paths::getFiles` via `Jobs::run` with parallel `statuses`, `outputs`, `matchCounts` arrays (`S8`).
   - Return `Stdio::exitSuccess` (`0`) when `totalMatches > 0`, else `Stdio::exitMismatch` (`1`).
3. Wire `Id::find` to `&Find::run` in `Source/Main.cpp`.
**Inventory:** A6, A8, A10, A17, A19, A20, P3, P6, P7, P14, P17, N1, N6, N11, S1, S2, S3, S4, S6, S7, S8, S10, S11, S14.
**Validation:** COUNSELOR reads `Source/Find.h/.cpp` and `Source/Main.cpp` and verifies context formatting, `-c` output, zero fake-carrier structs, `.at()`, and ≤3 branches per function.

### Step 6: Add Part 4 `find` and `RegExp::findNext` cases to `test/cases.md`
**Scope:** `~/Documents/Poems/dev/saw/test/cases.md`, `~/Documents/Poems/dev/saw/test/fixture/find-basic/`, `~/Documents/Poems/dev/saw/test/fixture/find-ignore/`, `~/Documents/Poems/dev/saw/test/stdin/`, `~/Documents/Poems/dev/saw/test/baseline/`
**Action:** Create fixtures and add reference (`grep -n`, `grep -nE`, `grep -ni`, `grep -nx`, `grep -nv`, `grep -c`, `grep -n -C 1`, `grep`) and baseline test cases (including `-q`, `--hash`, `--after` + `--first`, `--lines`, no-trailing-newline, 0-byte file, stdin, `.gitignore` + `!` negation + `[!...]` class, `--hidden --no-ignore`, `--include`/`--exclude`, direct binary file exit `2`, and invalid flag/regex/pattern errors) to `test/cases.md`.
**Inventory:** A26, P12.
**Validation:** ARCHITECT builds `saw` and runs `nvim --headless -l test/run.lua ~/.local/bin/saw`; every case prints `PASS` and exits `0`.

## BLESSED Alignment

- **B — Bound:** `TextDocument` and `Pattern` use `std::make_unique` (`P15`, zero raw `new`). `IgnoreRules::getChildRules` returns a complete child value on directory descent.
- **L — Lean:** `TextDocument` holds only document data/hashing (CLI range parsing lives in `Arguments`). Every function has ≤3 branches.
- **E — Explicit:** Context lines (`-`, `--`), binary handling, and ignore load order are explicit with zero inference.
- **S — SSOT:** `TextDocument::getLineHash` is the single implementation of 4-hex line hashes.
- **S — Stateless:** `Paths` and `Find` are stateless; `TextDocument`, `Pattern`, and `IgnoreRules` are complete at creation.
- **E — Encapsulation:** `Find` consumes `TextDocument`, `Pattern`, `Paths`, and `Jobs` via public APIs only.
- **D — Deterministic:** Sorted UTF-8 paths (`A20`) and index-ordered `Jobs::run` slots (`P7`).

## Risks / Open Questions

None.
