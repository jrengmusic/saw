# PLAN: saw — Tracker

**RFC:** none — objective from ARCHITECT prompt ("/goplan DESIGN BY CONTRACT")
**Date:** 2026-11-22
**BLESSED Compliance:** verified
**Language Constraints:** C++17 / JUCE 8.0.17 + JAM (LANGUAGE.md "C++ / JUCE"); Lua 5.1 on Neovim 0.10+ for the test runner; bash for benchmark scripts

## Overview

This tracker splits `saw` v0.0.1 (`SPEC.md`) into nine incremental parts. Each part is one sprint: COUNSELOR executes its steps with @Engineer, validates each step against CONTRACT, runs @Auditor once at the end of the part, and logs the sprint. The Framework Inventory below is the single source of truth for every part.

## Authority

1. `SPEC.md` (2026-11-22) governs every behavior, option, exit code, and output string.
2. `MANIFESTO.md`, `CODING.md`, `NAMES.md`, and `LANGUAGE.md` govern every design, name, and implementation detail without exception.
3. This tracker governs file layout, object boundaries, names, execution order, and the Framework Inventory.
4. Each part file governs its own steps.

## Prerequisites (ARCHITECT)

1. `carol init` in `~/Documents/Poems/dev/saw` is complete (`carol/` exists).
2. ARCHITECT builds and runs `nvim --headless -l test/run.lua ~/.local/bin/saw` at each part endpoint (CAROL.md Build).

## Parts

| Part | File | Repository | Delivers | Depends on | Status |
|---|---|---|---|---|---|
| 1 | `PLAN-saw-1-regexp.md` | `~/Documents/Poems/dev/jam` | `jam::RegExp::findNext` | — | pending |
| 2 | `PLAN-saw-2-skeleton.md` | saw | Project skeleton, generated vocabulary, `Stdio`, `Arguments`, `Main.cpp`, help documents | — | pending |
| 3 | `PLAN-saw-3-runner.md` | saw | `test/run.lua`, `test/cases.md`, initial CLI error cases | 2 | pending |
| 4 | `PLAN-saw-4-find.md` | saw | `TextDocument`, `Pattern`, `IgnoreRules`, `Paths`, `Jobs`, `Find` | 1, 3 | pending |
| 5 | `PLAN-saw-5-read.md` | saw | `Fields`, `Lines` read mode | 4 | pending |
| 6 | `PLAN-saw-6-replace.md` | saw | `LineDiff`, `Writer`, `UndoCache`, `Transaction`, `Replace` | 5 | pending |
| 7 | `PLAN-saw-7-mutate.md` | saw | `Lines` mutate mode, `PatchValidator`, `Patch` | 6 | pending |
| 8 | `PLAN-saw-8-undo.md` | saw | `Undo` | 7 | pending |
| 9 | `PLAN-saw-9-benchmark.md` | saw | `scout` benchmark manifests and scripts | 8 | pending |

COUNSELOR sets `Status` to `done` when the part is logged.

## Source Layout (final state)

| File | Object | Responsibility | Part |
|---|---|---|---|
| `Source/Main.cpp` | entry point | CLI routing, banner, help page, version line | 2 |
| `Source/Stdio.h/.cpp` | `struct Stdio` | Process exit codes, stdin read, binary stdout/stderr writes, TTY check | 2 |
| `Source/Arguments.h/.cpp` | `class Arguments` | Complete-at-creation CLI option parser over `juce::ArgumentList` + line-range parser | 2 |
| `Source/TextDocument.h/.cpp` | `class TextDocument` | Mapped or owned text buffer, line index, line/content SHA-256 hashes | 4 |
| `Source/Pattern.h/.cpp` | `class Pattern` | Literal SIMD and `jam::RegExp` per-line matcher | 4 |
| `Source/IgnoreRules.h/.cpp` | `class IgnoreRules` | Git ignore hierarchy and `core.excludesFile` matcher | 4 |
| `Source/Paths.h/.cpp` | `struct Paths` | File resolution, symlink target, non-recursive directory walk, UTF-8 sort | 4 |
| `Source/Jobs.h` | `struct Jobs` | Header-only template parallel job runner (`juce::ThreadPool`) | 4 |
| `Source/Find.h/.cpp` | `struct Find` | `saw find` command | 4 |
| `Source/Fields.h/.cpp` | `struct Fields` | `saw fields` command | 5 |
| `Source/Lines.h/.cpp` | `struct Lines` | `saw lines` command (read mode in Part 5, mutate mode in Part 7) | 5, 7 |
| `Source/LineDiff.h/.cpp` | `class LineDiff` | Line-level Myers diff, planned text assembly, GNU unified diff formatting | 6 |
| `Source/Writer.h/.cpp` | `struct Writer` | Atomic `juce::TemporaryFile` write (`toFile`) and POSIX execute-bit query | 6 |
| `Source/UndoCache.h/.cpp` | `struct UndoCache` | Rolling 32-transaction store under `<app-data>/saw/undo/` | 6 |
| `Source/Transaction.h/.cpp` | `class Transaction` | Planned multi-file batch, dry-run/expect gate, two-phase atomic commit and rollback | 6 |
| `Source/Replace.h/.cpp` | `struct Replace` | `saw replace` command | 6 |
| `Source/PatchValidator.h/.cpp` | `struct PatchValidator` | `: jam::JsonValidator` — schema, bounds, hash, and conflict validation | 7 |
| `Source/Patch.h/.cpp` | `struct Patch` | `saw patch` command | 7 |
| `Source/Undo.h/.cpp` | `struct Undo` | `saw undo` command | 8 |
| `Source/HELP.md`, `Source/help/<verb>.md` | help documents | Embedded markdown help pages | 2 |
| `cast/identifiers.md`, `cast/text.md`, `cast/banner.md`, `cast/files.md`, `cast/source.cast` | generation inputs | Single source of truth for vocabulary and messages | 2 |
| `test/run.lua`, `test/cases.md`, `test/fixture/`, `test/stdin/`, `test/baseline/` | test harness | Deterministic Neovim Lua runner and test cases | 3–8 |
| `benchmark/` | benchmark suite | `scout` manifests and workload scripts | 9 |

Each object is one header and one `.cpp` (`CODING.md:899-937`). `Jobs.h` is a template and stays header-only (`CODING.md:927`).

## Framework Inventory

Doxygen indexes: JAM `~/Documents/Poems/dev/jam/docs/xml/index.xml`, JUCE `~/Documents/Poems/JUCE/docs/xml/index.xml`.

### 1. API

| ID | Need | Module | API | Source | Part |
|---|---|---|---|---|---|
| A1 | Zero-copy file read; 0-byte safe | juce_core | `juce::MemoryMappedFile (file, MemoryMappedFile::readOnly)`, `getData()`, `getSize()` | `juce_MemoryMappedFile.h:48-53,72,101,106` | 4 |
| A2 | Atomic swap via temp file in target folder | juce_core | `juce::TemporaryFile (target, TemporaryFile::useHiddenFile)`, `getFile()`, `overwriteTargetFileWithTemporary()`, `deleteTemporaryFile()` | `juce_TemporaryFile.h:80-87,141-142,164,185,190` | 6, 8 |
| A3 | Checked write | juce_core | `juce::FileOutputStream (file)`, `failedToOpen()`, `write()`, `flush()`, `getStatus()` | `juce_FileOutputStream.h:78-79,93,98,112,115` | 6, 8 |
| A4 | Paths and execute permission | juce_core | `juce::File::getCurrentWorkingDirectory()`, `getChildFile()`, `getLinkedTarget()`, `getRelativePathFrom()`, `isDirectory()`, `existsAsFile()`, `deleteRecursively()`, `getFileName()`, `getParentDirectory()`, `setExecutePermission()` | `juce_File.h:124,132,199,379,480,508,1019,1090` | 4, 6, 8 |
| A5 | One-level directory listing | juce_core | `juce::RangedDirectoryIterator (dir, false, "*", File::findFilesAndDirectories, File::FollowSymlinks::no)` | `juce_RangedDirectoryIterator.h:134-138` | 4, 6 |
| A6 | Parallel per-file work | juce_core | `juce::ThreadPool`, `addJob (std::function<void()>)`, `getNumJobs()` | `juce_ThreadPool.h:232,301` | 4, 5, 6 |
| A7 | SHA-256 | juce_cryptography | `juce::SHA256 (const void*, size_t)`, `toHexString()` | `juce_SHA256.h:74,102` | 4, 6, 8 |
| A8 | Command line | juce_core | `juce::ArgumentList (argc, argv)`, `size()`, `operator[]`, `Argument::text`, `Argument::operator== (StringRef)` | `juce_ConsoleApplication.h:56,73,119,129,141,153` | 2 |
| A9 | Include and exclude globs | juce_core | `juce::String::matchesWildcard (wildcard, false)` | `juce_String.h:513` | 4 |
| A10 | Message templates and integer ranges | juce_core | `juce::String::formatted (formatStr, args...)`; `juce::Range<int> (start, end)`, `getStart()`, `getEnd()`, `getLength()`, `contains()`, `intersects()`, `getIntersectionWith()` | `juce_String.h:947`; `juce_Range.h:42-350` | 2, 4, 5, 6, 7 |
| A11 | Transaction random hex | juce_core | `juce::Random::getSystemRandom().nextInt (int)`, `juce::String::toHexString (int)`, `juce::String::paddedLeft` | `juce_Random.h:138`; `juce_String.h:1123-1124` | 6 |
| A12 | Byte scan | jam_core | `jam::simd::findByte (data, size, offset, target)` | `jam_Simd.h:142,300,383` | 4 |
| A13 | Regex | jam_core | `jam::RegExp (std::string_view)`, `findNext (text, offset)` (Part 1), `getCaptures`, `hasMatch` | `jam_RegExp.h:54-58`; `jam_RegExp.cpp:15-33,307-313` | 1, 4 |
| A14 | JSON read and validation | jam_core | `jam::JsonDocument::parse`, `isValid`, `getValue()`, `get()`, `contains()`, `getChildByToken()`, `getString()`, `getBoolean()`, `getView()`; `jam::JsonValidator` (`addRules`, `getRules`) | `jam_JsonDocument.h:24,101,194-254`; `jam_Document.h:1155`; `jam_JsonValidator.h:12-32` | 6, 7, 8 |
| A15 | Text formatting and parsing | jam_core | `jam::Format::getNumber<int>`, `appendDecimal`, `appendBytes`, `trim`, `getPreColon`, `getPostColon` | `jam_Format.h:402,427,676,681,748,760` | 2, 4, 6, 7 |
| A16 | JSON write | jam_core | `jam::JsonWriter::appendObject`, `appendKey`, `appendString`, `appendBoolean`, `appendArray`, `appendSeparator` | `jam_JsonWriter.h:63-159` | 6 |
| A17 | Dispatch table | jam_core | `jam::Function::Map<KeyType, capacity, ReturnType (Args...)>`, `add`, `get`; `jam::Function::pointerCapacity` | `jam_Function.h:17,185-249` | 2, 7 |
| A18 | Hash map | jam_core | `jam::HashMap`, `contains`, `at`, `try_emplace` | `classjam_1_1HashMap.xml` | 2, 4, 7 |
| A19 | Arrays | jam_core | `jam::Array`, `add`, `at`, `size`, `isEmpty`, `last`, `resize` | `classjam_1_1Array.xml` | all |
| A20 | Sorted UTF-8 strings | jam_core | `jam::Strings`, `add`, `addIfNotAlreadyThere`, `contains`, `sort (false)`, `size`, `getView`, `at` | `jam_Strings.h:215,265,329,515,558,565,578` | 2, 4, 5, 6, 7 |
| A21 | Help page | jam_tui, jam_markdown, jam_core | `jam::TerminalPage ({ BinaryData::fetcher }, jam::Strings { files::styleSheet })`, `setDocument`, `writeToStream`; `jam::MarkdownDocument::parse`; `BinaryData::getString` | `jam_TerminalPage.h:11-43`; `jam_Raw.h:81-85,111-123` | 2 |
| A22 | Screen erase | jam_terminal | `jam::TerminalEncoder::writeEraseScreen (juce::MemoryOutputStream&)` | `structjam_1_1TerminalEncoder.xml`; `scout/Source/Main.cpp:302-305` | 2 |
| A23 | JAM file directories | jam_core | `jam::File::getUserApplicationDataDirectory (ProjectInfo::projectName)`, `jam::File::getUserDirectory()`, `jam::File::getOrCreateDirectory (parent, child)` | `jam_File.h:36-37,119-120,203` | 4, 6, 8 |
| A24 | Project metadata | generated | `ProjectInfo::projectName`, `ProjectInfo::versionString`, `CAST_COMMIT` | `scout/Source/Main.cpp:117-126` | 2 |
| A25 | Core vocabulary | jam_core generated | `Id::diagnosticSeparator`, `Id::array`, `Id::object`, `Id::tokens`, `Id::file`, `Id::line`, `Id::text`, `Id::id`, `Id::path`, `Id::version`, `Chars::doubleDash`, `Chars::newline`, `Chars::carriageReturn`, `Chars::space`, `Chars::colon`, `Chars::comma`, `Chars::dash`, `Chars::openParen`, `Chars::closeParen`, `Chars::upperA`..`upperZ`, `Chars::lowerA`..`lowerZ`, `Chars::zero`..`nine` | `jam/generated/jam_Core.h:44-169,277-399` | 2, 4, 5, 6, 7 |
| A26 | Runner process and file API | Neovim | `vim.system`, `vim.uv.fs_mkdtemp`, `fs_realpath`, `fs_copyfile`, `fs_scandir`, `fs_scandir_next`, `fs_stat`, `fs_chmod`, `fs_readlink`, `fs_symlink`, `fs_rename`, `os_getpid`, `os_tmpdir`, `vim.fs.joinpath`, `vim.fn.delete`, `io.open`, `os.exit` | Neovim 0.10+ Lua API | 3 |
| A27 | Benchmark | scout | manifest `## run` (`epochs`, `command`, `samples`, `output`, `baseline`) and `## toolchain` (`command`, `flag`) | `scout/benchmark/mcp.md:1-21` | 9 |

### 2. Pattern Usage

| ID | Pattern | Exemplar | Use sites | Part |
|---|---|---|---|---|
| P1 | CLI entry: plain `main`, Windows UTF-8 console, help in its own GUI initialiser scope, version line, `printError` with project prefix | `scout/Source/Main.cpp:19-34,117-126,132-139,161-173,294-309` | `cast/Source/main.cpp`, `whatdbg/Source/Main.cpp` | 2 |
| P2 | `juce::ArgumentList` built in `main` and passed to verb `run` | `whatdbg/Source/Main.cpp:230-238,260` | `cast/Source/main.cpp` | 2 |
| P3 | Stateless command struct with `static int run (const juce::ArgumentList&)` | `cast/Source/Pack.h:19-49`, `cast/Source/Sync.h:24-37` | — | 4–8 |
| P4 | Blocking stdin read via `fread` with Windows `_O_BINARY`; binary stdout via `fwrite` | `cast/Source/main.cpp:698-718,1116-1128` | — | 2 |
| P5 | Integer validation by round trip: `getIntValue()` then `juce::String (value).compare (text) == 0` | `cast/Source/main.cpp:199-209` | — | 2, 4, 5 |
| P6 | Identifier-keyed `jam::Function::Map` dispatch (replaces >3-branch chains) | `jam_ModelContextServer.h:172-176`, `jam_ModelContextServer.cpp:25-53` | `jam_Document.h:824` | 2, 7 |
| P7 | Transient `juce::ThreadPool`, one job per index, drain by polling | `cast/Source/Jobs.h:13-39` | `cast/Source/Writer.h:19` | 4, 5, 6 |
| P8 | Generated vocabulary from `cast/*.md` into `Source/generated/` | `scout/cast/spell.md:1-222`, `scout/cast/text.md:1-18` | `cast/cast/`, `ling/cast/` | 2 |
| P9 | Option identifiers named `<name>Option` | `whatdbg/Source/Main.cpp:234,237,282` | — | 2 |
| P10 | `jam::JsonWriter` static append helpers into `std::string` | `scout/Source/ToolServer.cpp:210-215` | `jam_ModelContextServer.cpp:320-337` | 6 |
| P11 | `jam::JsonDocument` read in place without copying into transfer structs | `jam_ModelContextServer.cpp:10-20,85-96` | `jam_JsonSchema.cpp:161-173` | 6, 7, 8 |
| P12 | Lua runner: binary from `arg[1]`, `vim.system`, baseline compare, `os.exit (0/1)` | `ling/test/mcp.lua:1-8,105-145` | `scout/test/mcp.lua` | 3 |
| P13 | `scout` manifest plus script writing empty samples file | `scout/benchmark/mcp.md:1-21`, `scout/benchmark/mcp.sh:1-9` | — | 9 |
| P14 | Return status + value as `std::pair<juce::Result, ValueType>` | `cast/Source/main.cpp:698` | — | 2, 4, 5, 6 |
| P15 | Static factory using private `struct Access {};` passkey + `std::make_unique<T> (Access {}, ...)` (zero raw `new`) | `ling/Source/SymbolDocument.h:511,520` | — | 4 |
| P16 | Namespace enclosure format in JAM files | `CODING.md:671-679`, `jam_RegExp.h:3-5,105-106` | every JAM file | 1 |
| P17 | Project files live in the global namespace | `scout/Source/Main.cpp`, `cast/Source/Pack.h:19` | every sibling `Source/` | 2–8 |

### 3. Names

| ID | Family | Members | Intent | Part |
|---|---|---|---|---|
| N1 | Command structs named for CLI verbs | `Pack` (`cast/Source/Pack.h:19`), `Sync` (`cast/Source/Sync.h:24`) | One stateless struct per command | `Find`, `Replace`, `Lines`, `Fields`, `Patch`, `Undo` (4–8) |
| N2 | Static utility structs with plural noun or I/O domain names | `Jobs` (`cast/Source/Jobs.h:13`), `Items` (`Items.h:17`), `Writer` (`cast/Source/Writer.h:19`), `jam::StdioServer` (`jam_StdioServer.h`) | Stateless operations over caller data | `Stdio` (2), `Paths` (4), `Jobs` (4), `Writer` (6) |
| N3 | `*Document`: mapped or parsed content, complete at creation | `jam::JsonDocument`, `TemplateDocument` (`cast/Source/TemplateDocument.h:14`), `SymbolDocument` (`ling/Source/SymbolDocument.h:14`) | Owns or maps bytes; reads in place | `TextDocument` (4) |
| N4 | `*Validator : jam::*Validator` with `addRules` / `getRules` / `isValid` | `Validator : jam::MarkdownValidator` (`cast/Source/Validator.h:19`), `SwapValidator : jam::Document::Validator` (`ling/Source/SwapValidator.h:9`), `jam::JsonValidator` (`jam_JsonValidator.h:12`) | Validates parsed document in place, returns `juce::Result` | `PatchValidator` (7) |
| N5 | `juce::TextDiff` and `juce::ArgumentList` fallback families | `juce::TextDiff`, `Change { start; length; }`, `changes` (`juce_TextDiff.h:48-84`); `juce::ArgumentList` (`juce_ConsoleApplication.h:56`) | JUCE fallback naming (`NAMES.md` Rule -1) | `Arguments` (2), `LineDiff`, `LineDiff::Change`, `changes` (6) |
| N6 | Verb prefixes (`NAMES.md` Rule 1 Verb Contract) | `get`, `add`, `set`, `remove`, `is`/`has`, `apply` (callable parameter only), `getOrCreate`, `toFile` (`cast/Source/Writer.h:50`, `scout/Source/ProfileReport.h:13`), `run` (`cast/Source/Sync.h:26`), `print`, `write`, `commit` | Exact semantic action per verb | all |
| N7 | Option strings `<name>Option` | P9 | Pipe list of long and short forms | 2 |
| N8 | Help file constants `<subject>Help` | `scoutHelp` (`scout/cast/files.md:18`), `castHelp` (`cast/Source/generated/Files.h:33`) | Embedded help filename | `sawHelp`, `findHelp`, `replaceHelp`, `linesHelp`, `fieldsHelp`, `patchHelp`, `undoHelp` (2) |
| N9 | Diagnostic texts `fail<Condition>` | `failNotFound`, `failFlagUnknown` (`scout/cast/text.md:10-16`) | One template per failure | `text::Diagnostics::fail*` (2) |
| N10 | Domain terms (`NAMES.md` Rule -1 exception 1) | `SPEC.md` §4.4.1, §4.8, §6; `jam::Document::Rules` | Domain vocabulary | `Pattern`, `IgnoreRules` (4), `UndoCache`, `Transaction` (6) |
| N11 | Exit code constants | `EXIT_SUCCESS`, `EXIT_FAILURE` (`scout/Source/Main.cpp:195,199`) | Named process exit codes | `Stdio::exitSuccess` (0), `Stdio::exitMismatch` (1), `Stdio::exitError` (2) (2) |
| N12 | `find*` returning captures | `jam::RegExp::getCaptures` (`jam_RegExp.h:58`) | Captures, slot 0 is full match | `jam::RegExp::findNext` (1), `Pattern::findNext` (4) |
| N13 | Dispatch tables are nouns (`NAMES.md` Rule 8) | `selectors`, `markupOperators`, `treeConstruction` (`NAMES.md:534-539`) | Plain noun or verb-noun, never agent plural | `verbCommands`, `verbHelpFiles` (2), `globTokens` (4), `lineMutations`, `entryValidators`, `entryChanges` (7) |

### 4. Synthesis

| ID | Rule (`CODING.md` CRITICAL RULES) | Established form | Exemplar |
|---|---|---|---|
| S1 | "No `==` / `!=` on strings" | Identifier-keyed `jam::Function::Map` or `jam::HashMap`; `Argument::operator==` with pipe lists; `juce::String::compare (…) == 0` | `jam_ModelContextServer.h:172-176`; `scout/Source/Main.cpp:77` |
| S2 | "Uniform signature in the type" | `jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, ReturnType (Args...)>` | `jam_Function.h:185-249` |
| S3 | "No bail-out guards" / "No `else` after `return`" | Positive nesting in `void` functions; result returns when answer is determined; never `else` after `return` | `MANIFESTO.md:97-158`; `CODING.md:685-716,1068-1070` |
| S4 | "Use `.at()`" | `.at()` on `jam::Array`, `jam::Strings`, `jam::HashMap`, `std::string`, `std::string_view`, `std::array`, `std::vector` | `CODING.md:980-1021,1071` |
| S5 | "NO anonymous namespaces" | `static` free functions and constants in `.cpp` | `scout/Source/Main.cpp:19-309` |
| S6 | "One object, one translation unit" / "No Redundant STL Includes" | One `.h` and one `.cpp` per object; headers include `<JuceHeader.h>`, zero redundant STL headers | `CODING.md:778-801,864-938` |
| S7 | "No out-parameters" | Return value or `std::pair<juce::Result, ValueType>` | `CODING.md:1090` |
| S8 | "No fake-carrier structs" / "Complete at Creation" | Read `jam::JsonDocument`, `TextDocument`, `juce::Range<int>`, `jam::Strings` in place; zero transfer structs | `MANIFESTO.md:248-324`; `CODING.md:570-610,1091` |
| S9 | "Use the framework API fully" | Every need maps to an API row or an Absence row (`jam::File::getUserDirectory`, `jam::JsonValidator`, `jam::Format::appendDecimal`, `juce::Range::getIntersectionWith`) | `CODING.md:1085` |
| S10 | "Aggregate (brace) initialization"; "alternative tokens"; "NEVER use raw `new`" | `int x { 0 };`, `not`, `and`, `or`, `std::make_unique` with `Access` passkey (`P15`), parentheses for same-container copy/move (`CODING.md:183-197`) | `CODING.md:183-197,357,1066,1072`; `ling/Source/SymbolDocument.h:511` |
| S11 | "Structured bindings for all pair/tuple results" | `auto [status, value] { … };` with descriptive names, never `.first`/`.second` | `CODING.md:1044-1060` |
| S12 | "No Windows SDK macro names"; "Forbidden Terms" | Zero identifiers from `CODING.md:205-217` and zero forbidden terms from `NAMES.md:455-485` | `CODING.md:205-217`; `NAMES.md:455-485` |
| S13 | "Owner first, narrowest scope" / "`apply` takes a callable; a write is `set`" | Single-consumer types/helpers live in narrowest scope; `apply*` requires a callable; deletions use `remove*`; file writes use `Writer::toFile` | `CODING.md:263-296,1089`; `NAMES.md:118-139` |
| S14 | "300/30/3" | At most 3 branches per function; >3 branches use `jam::Function::Map` or `jam::HashMap` | `MANIFESTO.md:54-69` |
| S15 | "No local struct definitions inside function bodies" | Types declared at class scope or header | `CODING.md:486-509` |

### 5. Absence

| ID | Need | Query | Result |
|---|---|---|---|
| X1 | Line-level diff (Myers / LCS) | JAM `index.xml`, JUCE `index.xml` | Only character-level `juce::TextDiff` (`juce_TextDiff.cpp:38-214`). → `LineDiff` (6). |
| X2 | gitignore pattern parser | JAM `index.xml`, JUCE `matchesWildcard` (`*`/`?` only) | 0 full gitignore parsers. → `IgnoreRules` (4) translates patterns to `jam::RegExp` (A13). |
| X3 | Git config reader | JAM and JUCE `index.xml` | 0. → `IgnoreRules` (4) reads `core.excludesFile` per `SPEC.md` §4.4.1. |
| X4 | Copy-free plain-text line index | `jam_Document.cpp:274-282`, `jam_Strings.cpp:456-465` | No copy-free plain-text line index. → `TextDocument` (4) with `jam::simd::findByte` (A12). |
| X5 | Substring search | `jam_Simd.h:1-421` | Only `findByte` / `findNonPlainByte`. → `Pattern` (4): `findByte` + `std::memcmp`. |
| X6 | ASCII-only case folding | JAM and JUCE `index.xml` | 0 ASCII-only folder (`UnicodeProperty` folds full Unicode). → `Pattern` (4) folds `A`–`Z` via `Chars::upperA`..`upperZ`. |
| X7 | SHA-256 in JAM | JAM `index.xml` | 0 in JAM. → `juce::SHA256` (A7). |
| X8 | UTC time formatting | `juce_Time.h:241-279` (local time only), `jam_Format.h` | 0 UTC formatters. → `std::time` + `gmtime_s` (Windows) / `gmtime_r` (POSIX) + `std::strftime` in `UndoCache` (6). |
| X9 | Execute-bit query | `juce_File.h:350-399` (`setExecutePermission` only) | No getter. → `Writer::isExecutable` (6) queries `stat` on POSIX (`false` on Windows) and restores via `juce::File::setExecutePermission` (A4). |
| X10 | TTY detection | JAM and JUCE `index.xml` | 0. → `isatty (fileno (…))` / `_isatty (_fileno (…))` in `Stdio` (2). |
| X11 | Blocking stdin read | `jam_StdioServer.h` (async only) | No blocking reader. → `fread` loop in `Stdio::getInput` (2, `P4`). |
| X12 | argv parsing in JAM | JAM `index.xml` | 0. → `juce::ArgumentList` (A8) + `Arguments` (2). |
| X13 | Directory pruning in recursive iterator | `juce_RangedDirectoryIterator.h:110-185` | No prune API. → non-recursive iterator per folder (A5). |
| X14 | In-process unit test harness | JAM `index.xml`, sibling `CMakeLists.txt` | 0. → `nvim --headless -l test/run.lua` (3, `SPEC.md` §10). |
| X15 | Child process with stdin + separate stderr | `jam_ChildProcess.h`, `juce_ChildProcess.h` | Merge stdout/stderr or lack stdin. → `vim.system` (A26). |

## Validation Gate

Each step is validated by COUNSELOR before proceeding to the next — against `MANIFESTO.md` (BLESSED), `NAMES.md`, `CODING.md`, and the locked PLAN decisions (no deviation, no scope drift). @Auditor runs ONCE, after the final step of each part — never per step.

## Rules for Every Part

1. Names: only the names in the part file and this tracker (`CODING.md:1084`).
2. No comments and no doxygen before the audit (`CAROL.md` Code Hygiene).
3. Text search on C++ source only for a string literal or a `#` directive (`doxygen-protocol` skill).
4. Every user-visible string comes from `text::Diagnostics` (`cast/text.md`) or `Id::` (`cast/identifiers.md`), never from a raw literal in `.cpp`.
5. Each part adds its runner cases in the same part. A part is done only when ARCHITECT runs `nvim --headless -l test/run.lua ~/.local/bin/saw` and every case passes.
6. No git command without ARCHITECT's explicit instruction (`CAROL.md` Git).

## BLESSED Alignment

- **B — Bound:** `TextDocument` owns its `juce::MemoryMappedFile` or `std::string` via `std::make_unique` (`P15`, zero raw `new`). `Transaction` owns the documents of one apply. `Writer::toFile` scopes `juce::TemporaryFile` and `juce::FileOutputStream` with RAII. `test/run.lua` restores the undo folder in `xpcall` cleanup.
- **L — Lean:** One object per responsibility (Source Layout). `Writer` owns atomic file writes; `UndoCache` owns only the rolling store; `PatchValidator` owns only validation. Every function has at most 3 branches (`S14`).
- **E — Explicit:** Every string, option, and exit code is named (`text::Diagnostics`, `Id::`, `Stdio::exit*`). Positive nesting in `void` functions; result returns when the answer is determined; zero bail-out guards and zero `else` after `return` (`S3`).
- **S — SSOT:** `SPEC.md` holds behavior. `cast/*.md` holds vocabulary. `LineDiff::changes` drives both `getPlannedText` and `getUnifiedDiff`. `Transaction` reads display paths from `originalDocuments` with zero shadow `displayPaths`. `jam::JsonDocument` is read in place (`S8`).
- **S — Stateless:** Commands (`Find`, `Replace`, `Lines`, `Fields`, `Patch`, `Undo`), `Stdio`, `Paths`, `Jobs`, `Writer`, `UndoCache`, and `PatchValidator` are stateless structs. `Arguments`, `TextDocument`, `Pattern`, `IgnoreRules`, and `LineDiff` are complete at creation.
- **E — Encapsulation:** Commands tell `Transaction::run (arguments, isStandardInput)`. `PatchValidator : jam::JsonValidator` keeps JSON schema rules in the JAM validator family.
- **D — Deterministic:** Sorted UTF-8 paths (`A20`), per-file `Jobs::run` slots joined in path order (`P7`), ascending-sorted `LineDiff::changes`, byte-exact Lua runner (`SPEC.md` §10).
- **LANGUAGE.md C++ / JUCE:** One object, one translation unit; `Jobs.h` is header-only because it is a template.

## Risks / Open Questions

None.
